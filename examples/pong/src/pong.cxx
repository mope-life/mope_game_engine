#include "future.hxx"
#include "glfw_game_window/glfw_game_window.hxx"
#include "mope_game_engine/collisions.hxx"
#include "mope_game_engine/components/component.hxx"
#include "mope_game_engine/components/logger.hxx"
#include "mope_game_engine/components/sprite.hxx"
#include "mope_game_engine/components/transform.hxx"
#include "mope_game_engine/events/tick.hxx"
#include "mope_game_engine/font.hxx"
#include "mope_game_engine/game_engine.hxx"
#include "mope_game_engine/game_scene.hxx"
#include "mope_game_engine/game_system.hxx"
#include "mope_game_engine/query.hxx"
#include "mope_game_engine/transforms.hxx"
#include "mope_vec/mope_vec.hxx"

#include <ctime>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
    constexpr auto OrthoWidth{ 1024.f };
    constexpr auto OrthoHeight{ 768.f };
    constexpr auto PaddleWidth{ 12.f };
    constexpr auto PaddleHeight{ 80.f };
    constexpr auto OpponentMaxPixelsPerSecond{ 300.f };
    constexpr auto PaddleCollisionErraticism{ 4.0f };

    class pong : public mope::game_scene
    {
        void on_load(mope::I_game_engine& engine) override;
    };

    int run_app(mope::I_logger* logger)
    {
        try {
            auto window = mope::glfw::window{
                static_cast<int>(OrthoWidth),
                static_cast<int>(OrthoHeight),
                "Pong"
            };
            window.set_cursor_mode(mope::glfw::window::cursor_mode::disabled);

            auto engine = mope::game_engine_create();
            engine->set_tick_rate(60.0);
            engine->add_scene(std::make_unique<pong>());
            engine->run(window, logger);

            return EXIT_SUCCESS;
        }
        catch (std::exception const& ex) {
            logger->log(ex.what(), mope::I_logger::log_level::error);
            return EXIT_FAILURE;
        }
    }
}

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <sstream>

int WINAPI WinMain(
    _In_ HINSTANCE /*hInstance*/,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ LPSTR /*lpCmdLine*/,
    _In_ int /*nShowCmd*/
)
{
    class : public mope::I_logger
    {
    public:
        void log(char const* message, log_level level) const override
        {
            std::ostringstream output{};
            output << "[" << level_string(level) << "] " << message << '\n';
            OutputDebugStringA(output.str().c_str());
        }
    } logger;

    return run_app(&logger);
}

#else // !defined(_WIN32)

#include <iostream>

int main(int, char* [])
{
    class : public mope::I_logger
    {
        void log(char const* message, log_level level) const override
        {
            std::cout << "[" << level_string(level) << "] " << message << '\n';
        }
    } logger;

    return run_app(&logger);
}

#endif // defined(_WIN32)

namespace
{
    struct round_setup_component : public mope::entity_component
    {
        mope::vec3f initial_position;
        mope::vec3f initial_scale;
    };

    struct competitor_component : public mope::entity_component
    {
        bool(*victory_check)(mope::transform_component const&);
    };

    struct ball_tag : public mope::entity_component{ };
    struct player_behavior : public mope::entity_component { };
    struct opponent_behavior : public mope::entity_component { };

    struct ball_behavior : public mope::entity_component
    {
        mope::vec3f velocity;
    };

    enum class text_justification
    {
        left,
        right,
    };

    struct score_component : public mope::entity_component
    {
        unsigned int value;
        mope::vec3f display_position;
        text_justification justification;
    };

    struct score_display : public mope::relationship { };

    enum class collision_type
    {
        normal,
        erratic,    ///< Adds vertical velocity to the ball.
    };

    struct collides_with : public mope::relationship
    {
        collision_type type;
    };

    struct collision_detected_event
    {
        mope::entity_id ball_entity;
        mope::entity_id collided_entity;
        mope::collision collision;
        collision_type type;
        double previous_remaining_time;
    };

    struct collision_resolved_event
    {
        double remaining_time;
    };

    struct reset_round_event {};
    struct all_collisions_resolved_event {};
    struct score_changed_event
    {
        mope::entity_id entity;
        int increment;
    };

    void exit_on_escape(mope::game_scene& scene, mope::tick_event const& event)
    {
        if (event.inputs.pressed_keys.test(mope::glfw::ESCAPE)) {
            scene.set_done();
        }
    }

    void reset_round(mope::game_scene& scene, reset_round_event const&)
    {
        for (auto&& setup : scene.query<round_setup_component>().exec()) {
            scene.set_component(
                mope::transform_component{
                    setup.entity,
                    setup.initial_position,
                    setup.initial_scale
                }
            );
        }

        for (auto&& ball : scene.query<ball_tag>().exec()) {
            scene.set_component(
                // Set the initial ball velocity
                ball_behavior{
                    ball.entity,
                    { ((std::rand() % 2) * 2 - 1) * OrthoWidth / 2.5f,
                    static_cast<float>(std::rand() % 401 - 200),
                    0.0f }
                }
            );
        }
    }

    void player_movement(mope::game_scene& scene, mope::tick_event const& event)
    {
        for (auto&& [player, transform] : scene
            .query<player_behavior, mope::transform_component>()
            .exec())
        {
            auto previous_y = transform.position().y();
            auto min_showing = 0.5f * (transform.size().y() + transform.size().x());
            auto y_delta = -event.inputs.cursor_deltas.y();
            auto new_y = std::max(
                std::min(y_delta + previous_y, OrthoHeight - min_showing),
                min_showing - transform.size().y()
            );
            transform.set_y(new_y);
        }
    }

    void opponent_movement(mope::game_scene& scene, mope::tick_event const& event)
    {
        for (auto&& [opponent, opponent_transform, ball, ball_transform] : scene
            .query<opponent_behavior, mope::transform_component>()
            .cross<ball_tag, mope::transform_component>()
            .exec())
        {
            auto opponent_center = opponent_transform.y_position() + 0.5f * opponent_transform.y_size();
            auto ball_center = ball_transform.y_position() + 0.5f * ball_transform.y_size();
            auto diff = ball_center - opponent_center;
            auto actual_change = std::copysign(
                std::min(
                    static_cast<float>(event.time_step * OpponentMaxPixelsPerSecond),
                    std::abs(diff)
                ),
                diff
            );
            opponent_transform.slide({ 0.f, actual_change, 0.f });
        }
    }

    class ball_movement : public mope::game_system<mope::tick_event, collision_resolved_event>
    {
        std::vector<
            std::optional<std::tuple<mope::collision, collision_type, mope::entity_id>>
        > m_collision_cache;

        void operator()(mope::game_scene& scene, mope::tick_event const& event) override
        {
            find_collisions(scene, event.time_step);
        }

        // We are resolving collisions by sweeping along the ball's path. After
        // resolving a collision, we need to continue sweeping along the ball's
        // new path for the remainder of the time step.
        void operator()(mope::game_scene& scene, collision_resolved_event const& event) override
        {
            find_collisions(scene, event.remaining_time);
        }

        void find_collisions(mope::game_scene& scene, double remaining_time)
        {
            if (remaining_time <= 0.0) {
                scene.emplace_event<all_collisions_resolved_event>();
                return;
            }

            for (auto&& [ball, ball_transform, collides_with_view] : scene.query<
                ball_behavior,
                mope::transform_component,
                mope::related<collides_with, mope::transform_component>>().exec())
            {
                future::assign_range(
                    m_collision_cache,
                    collides_with_view
                    | std::views::transform([&ball_transform, &ball](auto&& collider_components)
                        {
                            auto&& [collider, collider_transform] = collider_components;
                            return mope::axis_aligned_object_collision(
                                ball_transform.position(),
                                ball_transform.size(),
                                ball.velocity,
                                collider_transform.position(),
                                collider_transform.size()
                            ).transform([&collider](auto&& collision)
                                {
                                    return std::make_tuple(collision, collider.type, collider.related_entity);
                                });
                        })
                    | std::views::filter([remaining_time](auto const& opt)
                        {
                            if (opt.has_value()) {
                                auto&& contact_time = std::get<0>(*opt).contact_time;
                                return !std::signbit(contact_time) && contact_time < remaining_time;
                            }
                            else {
                                return false;
                            }
                        })
                );

                // There is at least once collision this frame.
                if (!m_collision_cache.empty()) {
                    // Find the collision that happens first, determined by the minimum contact time.
                    auto&& [collision, type, collided_entity] = *std::ranges::min(
                        m_collision_cache,
                        std::ranges::less{},
                        [](auto&& opt) { return std::get<0>(*opt).contact_time; });

                    scene.emplace_event<collision_detected_event>(
                        ball.entity,
                        collided_entity,
                        std::move(collision),
                        type,
                        remaining_time);
                }
                else {
                    // No collision found. Move the ball the rest of the way along its path.
                    ball_transform.slide(static_cast<mope::vec3f>(remaining_time * ball.velocity));
                    scene.emplace_event<all_collisions_resolved_event>();
                }
            }
        }
    };

    void resolve_collisions(mope::game_scene& scene, collision_detected_event const& event)
    {
        if (auto opt = scene
            .query<ball_behavior, mope::transform_component>(event.ball_entity)
            .exec())
        {
            auto&& [ball, ball_transform] = *opt;

            // Move the ball by the amount of time before the collision occurred.
            ball_transform.slide(mope::vec3f{ event.collision.contact_time * ball.velocity });

            auto new_velocity = mope::vec3d{ ball.velocity };

            // Subtract twice the magnitude of the velocity projected along the
            // contact normal.
            // This has the effect of reversing our velocity on that axis.
            new_velocity -= 2 * new_velocity.dot(event.collision.contact_normal)
                * event.collision.contact_normal;

            if (collision_type::erratic == event.type) {
                if (auto collider_transform = scene
                    .query<mope::transform_component>(event.collided_entity)
                    .exec())
                {
                     // If we bounced off a paddle, add vertical velocity away
                     // from the center of the paddle.
                     auto mid = collider_transform->y_position() + 0.5f * collider_transform->y_size();
                     auto diff = event.collision.contact_point.y() - mid;
                     new_velocity.y() += PaddleCollisionErraticism * diff;
                }
            }

            ball.velocity = mope::vec3f{ new_velocity };

            // Tell the collision detection system to look for more collisions
            // with our new velocity and remaining time.
            scene.emplace_event<collision_resolved_event>(
                event.previous_remaining_time - event.collision.contact_time);
        }
    }

    void end_round(mope::game_scene& scene, all_collisions_resolved_event const&)
    {
        for (auto&& [ball, ball_transform, competitor] : scene
            .query<ball_tag, mope::transform_component>()
            .cross<competitor_component>()
            .exec())
        {
            if (competitor.victory_check(ball_transform)) {
                scene.emplace_event<score_changed_event>(competitor.entity, 1);
                scene.emplace_event<reset_round_event>();
            }
        }
    }

    class set_score : public mope::game_system<score_changed_event>
    {
    public:
        set_score(mope::I_game_engine& engine)
            : m_font{ engine.make_font("fonts/Share_Tech_Mono/ShareTechMono-Regular.ttf", 0) }
        {
            m_font.set_px(100);
        }

    private:
        auto make_text(
            mope::game_scene& scene,
            std::string_view text,
            mope::vec3f const& origin,
            text_justification justification)
        {
            auto result = std::vector<mope::entity_id>{};
            result.reserve(text.length());

            auto pen = origin;

            for (auto i = 0uz; i < text.size(); ++i) {
                auto ch = text_justification::left == justification
                    ? text[i]
                    : text[text.size() - (1uz + i)];

                auto glyph = m_font.make_glyph(ch);

                if (text_justification::right == justification) {
                    pen -= mope::vec3f{ glyph.advance };
                }

                auto entity = scene.create_entity();
                scene.set_component(mope::sprite_component{ entity, glyph.texture });
                // TODO: If we want to generalize this function, we would want to
                // account for kerning here.
                scene.set_component(mope::transform_component{
                    entity,
                    pen + mope::vec3f{ glyph.bearing },
                    mope::vec3f{ glyph.size } + mope::vec3f{ 0.0f, 0.0f, 1.0f } });

                if (text_justification::left == justification) {
                    pen += mope::vec3f{ glyph.advance };
                }

                result.push_back(entity);
            }

            return result;
        }

        void operator()(mope::game_scene& scene, score_changed_event const& event)
        {
            for (auto&& rel : scene.query<score_display>(event.entity).exec()) {
                scene.destroy_entity(rel.related_entity);
            }
            scene.remove_component<score_display>(event.entity);

            if (auto score = scene.query<score_component>(event.entity).exec()) {
                score->value += event.increment;

                for (auto entity : make_text(
                    scene,
                    std::to_string(score->value),
                    score->display_position,
                    score->justification))
                {
                    scene.set_component(score_display{ event.entity, entity });
                }
            }
        }

        mope::font m_font;
    };

    bool did_player_win(mope::transform_component const& ball_transform)
    {
        return ball_transform.x_position() > OrthoWidth;
    }

    bool did_opponent_win(mope::transform_component const& ball_transform)
    {
        return ball_transform.x_position() + ball_transform.x_size() < 0.0f;
    }
}

namespace
{
    void pong::on_load(mope::I_game_engine& engine)
    {
        // Random-number integrity is not paramount for our purposes.
        std::srand(static_cast<unsigned int>(std::time(0)));

        auto projection = mope::gl::orthographic_projection_matrix(
            0.0f, OrthoWidth, 0.0f, OrthoHeight, 10.0f, -10.0f
        );
        set_projection_matrix(projection);

        add_game_system(exit_on_escape);
        add_game_system(reset_round);
        add_game_system(player_movement);
        add_game_system(opponent_movement);
        emplace_game_system<ball_movement>();
        add_game_system(resolve_collisions);
        add_game_system(end_round);
        emplace_game_system<set_score>(engine);

        auto player = create_entity();
        auto opponent = create_entity();
        auto ball = create_entity();
        auto top = create_entity();
        auto bottom = create_entity();

        auto const& default_texture = engine.get_default_texture();
        set_components(
            mope::sprite_component{ player, default_texture },
            mope::sprite_component{ opponent, default_texture },
            mope::sprite_component{ ball, default_texture },
            ball_tag{ ball },
            ball_behavior{ ball, { 0.0f, 0.0f, 0.0f } },
            player_behavior{ player },
            opponent_behavior{ opponent },
            competitor_component{ player, did_player_win },
            competitor_component{ opponent, did_opponent_win },
            score_component{
                player,
                0,
                { 0.5f * OrthoWidth - 100.0f, OrthoHeight - 150.0f, 0.0f },
                text_justification::right },
            score_component{
                opponent,
                0,
                { 0.5f * OrthoWidth + 100.0f, OrthoHeight - 150.0f, 0.0f },
                text_justification::left },
            collides_with{ ball, player, collision_type::erratic },
            collides_with{ ball, opponent, collision_type::erratic },
            collides_with{ ball, top, collision_type::normal },
            collides_with{ ball, bottom, collision_type::normal },
            mope::transform_component{
                top,
                { -0.5f * OrthoWidth, -1.0f, 0.0f },
                { 2.0f * OrthoWidth, 1.0f, 1.0f } },
            mope::transform_component{
                bottom,
                { -0.5f * OrthoWidth, OrthoHeight, 0.0f },
                { 2.0f * OrthoWidth, 1.0f, 1.0f } },
            round_setup_component{
                ball,
                /*position*/ { 0.5f * (OrthoWidth - PaddleWidth), 0.5f * (OrthoHeight - PaddleWidth), 0.0f },
                /*scale*/ { PaddleWidth, PaddleWidth, 1.0f } },
            round_setup_component{
                player,
                /*position*/ { 2.0f * PaddleWidth, 0.5f * (OrthoHeight - PaddleHeight), 0.0f },
                /*scale*/ { PaddleWidth, PaddleHeight, 1.0f } },
            round_setup_component{
                opponent,
                /*position*/ { OrthoWidth - (3.0f * PaddleWidth), 0.5f * (OrthoHeight - PaddleHeight), 0.0f },
                /*scale*/ { PaddleWidth, PaddleHeight, 1.0f } }
        );

        emplace_event<score_changed_event>(player, 0);
        emplace_event<score_changed_event>(opponent, 0);
        emplace_event<reset_round_event>();
    }
}
