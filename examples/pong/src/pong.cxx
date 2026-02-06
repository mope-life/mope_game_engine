#include "future.hxx"
#include "glfw_game_window/glfw_game_window.hxx"
#include "mope_game_engine/collisions.hxx"
#include "mope_game_engine/components/component.hxx"
#include "mope_game_engine/components/logger.hxx"
#include "mope_game_engine/components/sprite.hxx"
#include "mope_game_engine/components/transform.hxx"
#include "mope_game_engine/events/tick.hxx"
#include "mope_game_engine/game_engine.hxx"
#include "mope_game_engine/game_scene.hxx"
#include "mope_game_engine/game_system.hxx"
#include "mope_game_engine/transforms.hxx"
#include "mope_vec/mope_vec.hxx"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <tuple>
#include <utility>
#include <vector>

namespace
{
    class pong : public mope::game_scene
    {
        void on_load(mope::game_engine& engine) override;
    };
}

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <sstream>

namespace
{
    class logger : public mope::I_logger
    {
        void log(const char* message, log_level level) const override
        {
            std::ostringstream output{};
            output << "[" << level_string(level) << "] " << message << '\n';
            OutputDebugStringA(output.str().c_str());
        }
    };
}

#define MAIN() int WINAPI WinMain(          \
    _In_ HINSTANCE /*hInstance*/,           \
    _In_opt_ HINSTANCE /*hPrevInstance*/,   \
    _In_ LPSTR /*lpCmdLine*/,               \
    _In_ int /*nShowCmd*/                   \
)

#else // !defined(_WIN32)

#include <iostream>

namespace
{
    class logger : public mope::I_logger
    {
        void log(const char* message, log_level level) const override
        {
            std::cout << "[" << level_string(level) << "] " << message << '\n';
        }
    };
}

#define MAIN() int main(int, char* [])

#endif // defined(_WIN32)

MAIN()
{
    auto window = mope::glfw::window{ 1024, 768, "Pong" };
    window.set_cursor_mode(mope::glfw::window::cursor_mode::disabled);

    logger log;
    auto engine = mope::game_engine{ };
    engine.set_tick_rate(60.0);
    engine.add_scene(std::make_unique<pong>());
    engine.run(window, &log);

    return EXIT_SUCCESS;
}

namespace
{
    static constexpr auto OrthoWidth{ 1024.f };
    static constexpr auto OrthoHeight{ 768.f };
    static constexpr auto PaddleWidth{ 12.f };
    static constexpr auto PaddleHeight{ 80.f };
    static constexpr auto OpponentMaxPixelsPerSecond{ 300.f };

    struct player_component : public mope::entity_component
    {
        unsigned int score;
    };

    struct opponent_component : public mope::entity_component
    {
        unsigned int score;
    };

    struct ball_component : public mope::entity_component
    {
        mope::vec3f velocity;
    };

    enum class collision_type
    {
        normal,
        erratic,    ///< Adds vertical velocity to the ball.
    };

    struct ball_collider_component : public mope::entity_component
    {
        collision_type type;
    };

    struct reset_round_event {};

    struct collision_detected_event
    {
        double previous_remaining_time;
        mope::collision collision;
        collision_type type;
        ball_component& ball;
        mope::transform_component& ball_transform;
        mope::transform_component& collider_transform;
    };

    struct collision_resolved_event
    {
        double remaining_time;
    };

    struct all_collisions_resolved_event {};

    void exit_on_escape(mope::game_scene& scene, mope::tick_event const& event)
    {
        if (event.inputs.pressed_keys.test(mope::glfw::ESCAPE)) {
            scene.set_done();
        }
    }

    void reset_round(mope::game_scene& scene, reset_round_event const&)
    {
        for (auto&& ball : scene.query<ball_component>()) {
            scene.set_components(
                // Set the initial ball velocity
                ball_component{
                    ball.entity,
                    { ((std::rand() % 2) * 2 - 1) * OrthoWidth / 2.5f,
                    static_cast<float>(std::rand() % 401 - 200),
                    0.0f }
                },
                // Set the initial position of the ball
                mope::transform_component{
                    ball.entity,
                    { 0.5f * (OrthoWidth - PaddleWidth), 0.5f * (OrthoHeight - PaddleWidth), 0.0f },
                    { PaddleWidth, PaddleWidth, 1.0f }
                });
        }

        for (auto&& player : scene.query<player_component>()) {
            scene.set_components(
                // Set the initial position of the player
                mope::transform_component{
                    player.entity,
                    { 2.0f * PaddleWidth, 0.5f * (OrthoHeight - PaddleHeight), 0.0f },
                    { PaddleWidth, PaddleHeight, 1.0f }
                });
        }

        for (auto&& opponent : scene.query<opponent_component>()) {
            scene.set_components(
                // Set the initial position of the opponent
                mope::transform_component{
                    opponent.entity,
                    { OrthoWidth - (3.0f * PaddleWidth), 0.5f * (OrthoHeight - PaddleHeight), 0.0f },
                    { PaddleWidth, PaddleHeight, 1.0f }
                });
        }
    }

    void player_movement(mope::game_scene& scene, mope::tick_event const& event)
    {
        for (auto&& [player, transform] : scene
            .query<player_component, mope::transform_component>())
        {
            auto previous_y = transform.position().y();
            auto min_showing = 0.5f * (transform.size().y() + transform.size().x());
            auto y_delta = event.inputs.cursor_deltas.y();
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
            .query<opponent_component, mope::transform_component>()
            .join<ball_component, mope::transform_component>())
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
            std::optional<std::tuple<mope::collision, collision_type, mope::transform_component&>>
        > m_collision_cache;

        void operator()(mope::game_scene& scene, mope::tick_event const& event) override
        {
            find_collisions(scene, event.time_step);
        }

        // Resolving a collision might cause more collisions, so...
        // Each time we resolve a collision, we look for more collisions.
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

            for (auto&& [ball, ball_transform] : scene
                .query<ball_component, mope::transform_component>())
            {
                auto v = scene.query<ball_collider_component, mope::transform_component>();

                future::assign_range(
                    m_collision_cache,
                    v | std::views::transform([&ball_transform, &ball](auto&& collider_components)
                        {
                            auto&& [collider, collider_transform] = collider_components;
                            return mope::axis_aligned_object_collision(
                                ball_transform.position(),
                                ball_transform.size(),
                                ball.velocity,
                                collider_transform.position(),
                                collider_transform.size()
                            ).transform([&collider, &collider_transform](auto&& collision)
                                {
                                    return std::make_tuple(collision, collider.type, std::ref(collider_transform));
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
                    auto&& [collision, type, collider_transform] = *std::ranges::min(
                        m_collision_cache,
                        std::ranges::less{},
                        [](auto&& opt) { return std::get<0>(*opt).contact_time; });

                    scene.emplace_event<collision_detected_event>(
                        remaining_time,
                        std::move(collision),
                        type,
                        ball,
                        ball_transform,
                        collider_transform
                    );
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
        // Move the ball by the amount of time before the collision occurred.
        event.ball_transform.slide(
            static_cast<mope::vec3f>(event.collision.contact_time * event.ball.velocity)
        );

        auto new_velocity = static_cast<mope::vec3d>(event.ball.velocity);

        // Subtract twice the magnitude of the velocity projected along the
        // contact normal.
        // This has the effect of reversing our velocity on that axis.
        new_velocity -= 2 * new_velocity.dot(event.collision.contact_normal)
            * event.collision.contact_normal;

        // If we bounced off a paddle, add vertical velocity away from the
        // center of the paddle.
        if (collision_type::erratic == event.type) {
            auto diff = event.collision.contact_point.y()
                - (event.collider_transform.y_position() + 0.5f * event.collider_transform.y_size());
            new_velocity.y() = static_cast<float>(event.ball.velocity.y() + 4.0 * diff);
        }

        event.ball.velocity = static_cast<mope::vec3f>(new_velocity);

        // Tell the collision detection system to look for more collisions with
        // our new velocity and remaining time.
        scene.emplace_event<collision_resolved_event>(
            event.previous_remaining_time - event.collision.contact_time
        );
    }

    void end_round(mope::game_scene& scene, all_collisions_resolved_event const&)
    {
        for (auto&& [ball, transform] : scene
            .query<ball_component, mope::transform_component>())
        {
            if (transform.x_position() > OrthoWidth) {
                for (auto&& player : scene.query<player_component>()) {
                    ++player.score;
                }
                scene.emplace_event<reset_round_event>();
            }
            else if (transform.x_position() + transform.x_size() < 0.0f) {
                for (auto&& opponent : scene.query<opponent_component>()) {
                    ++opponent.score;
                }
                scene.emplace_event<reset_round_event>();
            }
        }
    }
}

namespace
{
    void pong::on_load(mope::game_engine& engine)
    {
        // Random-number integrity is not paramount for our purposes.
        std::srand(static_cast<unsigned int>(std::time(0)));

        mope::mat4f projection = mope::gl::orthographic_projection_matrix(
            0, OrthoWidth, 0, OrthoHeight, -10, 10
        );
        set_projection_matrix(projection);

        add_game_system(exit_on_escape);
        add_game_system(reset_round);
        add_game_system(player_movement);
        add_game_system(opponent_movement);
        emplace_game_system<ball_movement>();
        add_game_system(resolve_collisions);
        add_game_system(end_round);

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
            ball_component{ ball, { 0.0f, 0.0f, 0.0f } },
            player_component{ player, 0 },
            opponent_component{ opponent, 0 },
            ball_collider_component{ player, collision_type::erratic },
            ball_collider_component{ opponent, collision_type::erratic },
            ball_collider_component{ top, collision_type::normal },
            ball_collider_component{ bottom, collision_type::normal },
            mope::transform_component{
                top,
                { -0.5f * OrthoWidth, -1.0f, 0.0f },
                { 2.0f * OrthoWidth, 1.0f, 1.0f }
            },
            mope::transform_component{
                bottom,
                { -0.5f * OrthoWidth, OrthoHeight, 0.0f },
                { 2.0f * OrthoWidth, 1.0f, 1.0f }
            }
        );

        emplace_event<reset_round_event>();
    }
}
