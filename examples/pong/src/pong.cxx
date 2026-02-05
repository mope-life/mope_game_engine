#include "future.hxx"
#include "glfw_game_window/glfw_game_window.hxx"
#include "mope_game_engine/collisions.hxx"
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

    public:
        void reset_round();

        mope::entity_id player{ create_entity() };
        mope::entity_id opponent{ create_entity() };
        mope::entity_id ball{ create_entity() };
        mope::entity_id top{ create_entity() };
        mope::entity_id bottom{ create_entity() };
        mope::entity_id player_score{ create_entity() };
        mope::entity_id opponent_score{ create_entity() };
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

    struct ball_collider_component : public mope::entity_component
    {
        enum
        {
            boundary,
            paddle,
        } collision_type;
    };

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

    class ball_movement : public mope::game_system<mope::tick_event>
    {
        std::vector<std::tuple<
            ball_collider_component&,
            mope::transform_component&>> m_collider_cache;

        std::vector<std::pair<
            std::size_t,
            std::optional<mope::collision>>> m_collision_cache;

        void operator()(mope::game_scene& scene, mope::tick_event const& event) override
        {
            // We need to make multiple passes over these components, so we cache them first.
            future::assign_range(m_collider_cache, scene
                .query<ball_collider_component, mope::transform_component>());

            for (auto&& [ball, ball_transform] : scene
                .query<ball_component, mope::transform_component>())
            {
                double remaining_time = event.time_step;

                do {
                    future::assign_range(
                        m_collision_cache,
                        m_collider_cache
                        | std::views::enumerate
                        | std::views::transform([&ball_transform, &ball](auto&& pair) {
                            auto&& [i, collider_pair] = pair;
                            auto&& collider_transform = std::get<1>(collider_pair);
                            return std::make_pair(
                                i,
                                mope::axis_aligned_object_collision(
                                    ball_transform.position(),
                                    ball_transform.size(),
                                    ball.velocity,
                                    collider_transform.position(),
                                    collider_transform.size()
                                )
                            );
                            })
                        | std::views::filter([remaining_time](auto&& pair) {
                            auto& collision = pair.second;
                            return collision.has_value()
                                && !std::signbit(collision->contact_time)
                                && collision->contact_time < remaining_time;
                            })
                    );

                    if (!m_collision_cache.empty()) {
                        auto&& [i, collision] = std::ranges::min(
                            m_collision_cache,
                            std::ranges::less{},
                            [](auto&& pair) { return pair.second->contact_time; });
                        auto&& [collider, collider_transform] = m_collider_cache[i];

                        // Move the ball by the amount of time before the collision
                        // occurred, then change course.
                        ball_transform.slide(static_cast<mope::vec3f>(collision->contact_time * ball.velocity));
                        remaining_time -= collision->contact_time;

                        auto velocity = static_cast<mope::vec3d>(ball.velocity);
                        velocity -= 2 * velocity.dot(collision->contact_normal) * collision->contact_normal;

                        if (collider.paddle == collider.collision_type) {
                            auto diff = collision->contact_point.y()
                                - (collider_transform.y_position() + 0.5f * collider_transform.y_size());
                            velocity.y() = static_cast<float>(velocity.y() + 4.0 * diff);
                        }

                        ball.velocity = static_cast<mope::vec3f>(velocity);
                    }
                    else {
                        // No collision found. Move the ball the rest of the way
                        // along its path.
                        ball_transform.slide(static_cast<mope::vec3f>(remaining_time * ball.velocity));
                        remaining_time = 0.0;
                    }
                } while (remaining_time > 0.0);
            }
        }
    };

    void end_round(mope::game_scene& scene, mope::tick_event const&)
    {
        for (auto&& [ball, transform] : scene
            .query<ball_component, mope::transform_component>())
        {
            if (transform.x_position() > OrthoWidth) {
                for (auto&& player : scene.query<player_component>()) {
                    ++player.score;
                }
                static_cast<pong&>(scene).reset_round();
            }
            else if (transform.x_position() + transform.x_size() < 0.0f) {
                for (auto&& opponent : scene.query<opponent_component>()) {
                    ++opponent.score;
                }
                static_cast<pong&>(scene).reset_round();
            }
        }
    }

    void exit_on_escape(mope::game_scene& scene, mope::tick_event const& event)
    {
        if (event.inputs.pressed_keys.test(mope::glfw::ESCAPE)) {
            scene.set_done();
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

        add_game_system<mope::tick_event>(player_movement);
        add_game_system<mope::tick_event>(opponent_movement);
        add_game_system(std::make_unique<ball_movement>());
        add_game_system<mope::tick_event>(exit_on_escape);
        add_game_system<mope::tick_event>(end_round);

        auto const& default_texture = engine.get_default_texture();
        set_components(
            mope::sprite_component{ player, default_texture },
            mope::sprite_component{ opponent, default_texture },
            mope::sprite_component{ ball, default_texture },
            ball_component{ ball, { 0.0f, 0.0f, 0.0f } },
            player_component{ player, 0 },
            opponent_component{ opponent, 0 },
            ball_collider_component{ player, ball_collider_component::paddle },
            ball_collider_component{ opponent, ball_collider_component::paddle },
            ball_collider_component{ top, ball_collider_component::boundary },
            ball_collider_component{ bottom, ball_collider_component::boundary }
        );

        reset_round();
    }

    void pong::reset_round()
    {
        set_components(
            ball_component{
                ball,
                { ((std::rand() % 2) * 2 - 1) * OrthoWidth / 2.5f
                , static_cast<float>(std::rand() % 401 - 200)
                , 0.0f }
            },
            mope::transform_component{
                player,
                { 2.0f * PaddleWidth, 0.5f * (OrthoHeight - PaddleHeight), 0.0f },
                { PaddleWidth, PaddleHeight, 1.0f }
            },
            mope::transform_component{
                opponent,
                { OrthoWidth - (3.0f * PaddleWidth), 0.5f * (OrthoHeight - PaddleHeight), 0.0f },
                { PaddleWidth, PaddleHeight, 1.0f }
            },
            mope::transform_component{
                ball,
                { 0.5f * (OrthoWidth - PaddleWidth), 0.5f * (OrthoHeight - PaddleWidth), 0.0f },
                { PaddleWidth, PaddleWidth, 1.0f }
            },
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
    }
}
