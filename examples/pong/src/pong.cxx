#include "pong.hxx"

#include "future.hxx"
#include "glfw.hxx"
#include "mope_game_engine/collisions.hxx"
#include "mope_game_engine/component.hxx"
#include "mope_game_engine/components/input_state.hxx"
#include "mope_game_engine/components/sprite.hxx"
#include "mope_game_engine/components/transform.hxx"
#include "mope_game_engine/ecs_manager.hxx"
#include "mope_game_engine/game_engine.hxx"
#include "mope_game_engine/game_scene.hxx"
#include "mope_game_engine/game_system.hxx"
#include "mope_game_engine/transforms.hxx"
#include "mope_vec/mope_vec.hxx"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <ctime>
#include <functional>
#include <memory>
#include <optional>
#include <ranges>
#include <utility>
#include <vector>

namespace
{
    static constexpr auto OrthoWidth{ 1024.f };
    static constexpr auto OrthoHeight{ 768.f };
    static constexpr auto PaddleWidth{ 12.f };
    static constexpr auto PaddleHeight{ 80.f };
    static constexpr auto OpponentMaxPixelsPerSecond{ 300.f };

    class pong : public mope::game_scene
    {
    public:
        auto on_initial_tick(mope::game_engine& engine) -> void override;

        mope::entity player{ create_entity() };
        mope::entity opponent{ create_entity() };
        mope::entity ball{ create_entity() };
        mope::entity top{ create_entity() };
        mope::entity bottom{ create_entity() };
        mope::entity player_score{ create_entity() };
        mope::entity opponent_score{ create_entity() };
    };

    struct player_component : public mope::entity_component
    {
    };

    struct player_score_component : public mope::entity_component
    {
        unsigned int score;
    };

    struct opponent_component : public mope::entity_component
    {
    };

    struct opponent_score_component : public mope::entity_component
    {
        unsigned int score;
    };

    struct ball_component : public mope::entity_component
    {
        bool out_of_bounds;
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

    class player_movement : public mope::game_system<player_component, mope::transform_component, mope::input_state>
    {
        void process_tick(mope::game_scene& scene, double time_step) override
        {
            for (auto&& [player, transform, inputs] : components(scene)) {
                auto previous_y = transform.position().y();
                auto min_showing = 0.5f * (transform.size().y() + transform.size().x());
                auto y_delta = inputs.cursor_deltas.y();
                auto new_y = std::max(
                    std::min(y_delta + previous_y, OrthoHeight - min_showing),
                    min_showing - transform.size().y()
                );
                transform.set_y(new_y);
            }
        }
    };

    class opponent_movement : public mope::game_system<
        opponent_component,
        mope::transform_component,
        mope::relationship<ball_component, mope::transform_component>>
    {
        void process_tick(mope::game_scene& scene, double time_step) override
        {
            for (auto&& [opponent, opponent_transform, ball_components] : components(scene)) {
                for (auto&& [ball, ball_transform] : ball_components) {
                    auto opponent_center = opponent_transform.y_position() + 0.5f * opponent_transform.y_size();
                    auto ball_center = ball_transform.y_position() + 0.5f * ball_transform.y_size();
                    auto diff = ball_center - opponent_center;

                    auto actual_change = std::copysign(
                        std::min(
                            static_cast<float>(time_step * OpponentMaxPixelsPerSecond),
                            std::abs(diff)
                        ),
                        diff
                    );

                    opponent_transform.slide({ 0.f, actual_change, 0.f });
                }
            }
        }
    };

    class ball_movement : public mope::game_system<
        ball_component,
        mope::transform_component,
        mope::relationship<ball_collider_component, mope::transform_component>>
    {
        std::vector<std::pair<
            ball_collider_component&,
            mope::transform_component&>> m_collider_cache;

        std::vector<std::pair<
            std::size_t,
            std::optional<mope::collision>>> m_collision_cache;

        void process_tick(mope::game_scene& scene, double time_step) override
        {
            for (auto&& [ball, ball_transform, collider_components] : components(scene)) {
                // The nested range is single-pass (std::ranges::input_range) only.
                // So we need to cache the elements in case we need to make multiple
                // passes.
                future::assign_range(m_collider_cache, collider_components);

                double remaining_time = time_step;

                do {
                    future::assign_range(
                        m_collision_cache,
                        m_collider_cache
                            | std::views::enumerate
                            | std::views::transform([&ball_transform, &ball](auto&& pair) {
                                  auto&& [i, collider_pair] = pair;
                                  auto&& collider_transform = collider_pair.second;
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
                        auto&& [collider, collider_transform] = m_collider_cache.at(i);

                        // Move the ball by the amount of time before the collision
                        // occurred, then change course.
                        ball_transform.slide(static_cast<mope::vec3f>(collision->contact_time * ball.velocity));
                        remaining_time -= collision->contact_time;

                        auto velocity = static_cast<mope::vec3d>(ball.velocity);
                        auto dot = velocity.dot(collision->contact_normal);
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

    class victory_or_defeat : public mope::game_system<
        ball_component,
        mope::transform_component,
        mope::relationship<player_score_component>,
        mope::relationship<opponent_score_component>>
    {
        void process_tick(mope::game_scene& scene, double time_step) override
        {
            for (auto&& [ball, transform, player_score_view, opponent_score_view] : components(scene)) {
                if (transform.x_position() > OrthoWidth) {
                    for (auto&& player_score : player_score_view) {
                        ++player_score.score;
                    }
                    ball.out_of_bounds = true;
                }
                else if (transform.x_position() + transform.x_size() < 0.0f) {
                    for (auto&& opponent_score : opponent_score_view) {
                        ++opponent_score.score;
                    }
                    ball.out_of_bounds = true;
                }
            }
        }
    };

    class reset_game : public mope::game_system<ball_component>
    {
        void process_tick(mope::game_scene& scene, double time_step) override
        {
            for (auto&& ball : components(scene)) {
                if (ball.out_of_bounds) {
                    ball.out_of_bounds = false;
                    ball.velocity = {
                        ((std::rand() % 2) * 2 - 1) * OrthoWidth / 2.5f,
                        static_cast<float>(std::rand() % 401 - 200),
                        0.0f
                    };

                    auto& entities = static_cast<pong&>(scene);

                    scene.set_components(
                        mope::transform_component{
                            entities.player,
                            { 2.0f * PaddleWidth, 0.5f * (OrthoHeight - PaddleHeight), 0.0f },
                            { PaddleWidth, PaddleHeight, 1.0f }
                        },
                        mope::transform_component{
                            entities.opponent,
                            { OrthoWidth - (3.0f * PaddleWidth), 0.5f * (OrthoHeight - PaddleHeight), 0.0f },
                            { PaddleWidth, PaddleHeight, 1.0f }
                        },
                        mope::transform_component{
                            entities.ball,
                            { 0.5f * (OrthoWidth - PaddleWidth), 0.5f * (OrthoHeight - PaddleWidth), 0.0f },
                            { PaddleWidth, PaddleWidth, 1.0f }
                        },
                        mope::transform_component{
                            entities.top,
                            { -0.5f * OrthoWidth, -1.0f, 0.0f },
                            { 2.0f * OrthoWidth, 1.0f, 1.0f }
                        },
                        mope::transform_component{
                            entities.bottom,
                            { -0.5f * OrthoWidth, OrthoHeight, 0.0f },
                            { 2.0f * OrthoWidth, 1.0f, 1.0f }
                        }
                    );
                }
            }
        }
    };

    class exit_on_escape : public mope::game_system<mope::input_state>
    {
        void process_tick(mope::game_scene& scene, double time_step) override
        {
            for (auto&& inputs : components(scene)) {
                if (inputs.pressed_keys.test(mope::glfw::ESCAPE)) {
                    scene.set_done();
                }
            }
        }
    };

    void pong::on_initial_tick(mope::game_engine& engine)
    {
        // Random-number integrity is not paramount for our purposes.
        std::srand(std::time(0));

        mope::mat4f projection = mope::gl::orthographic_projection_matrix(
            0, OrthoWidth, 0, OrthoHeight, -10, 10
        );
        set_projection_matrix(projection);

        emplace_game_system<exit_on_escape>();
        emplace_game_system<player_movement>();
        emplace_game_system<opponent_movement>();
        emplace_game_system<ball_movement>();
        emplace_game_system<victory_or_defeat>();
        emplace_game_system<reset_game>();

        auto const& default_texture = engine.get_default_texture();
        set_components(
            mope::sprite_component{ player, default_texture },
            mope::sprite_component{ opponent, default_texture },
            mope::sprite_component{ ball, default_texture },
            // Initially set the ball out of bounds so that the reset system will see it.
            ball_component{ ball, true, { 0.0f, 0.0f, 0.0f } },
            player_component{ player },
            opponent_component{ opponent },
            player_score_component{ player_score, 0u },
            opponent_score_component{ opponent_score, 0u },
            ball_collider_component{ player, ball_collider_component::paddle },
            ball_collider_component{ opponent, ball_collider_component::paddle },
            ball_collider_component{ top, ball_collider_component::boundary },
            ball_collider_component{ bottom, ball_collider_component::boundary }
        );

        // All transforms will be set by the reset system.
    }
}

int app_main(std::shared_ptr<mope::I_logger> logger)
{
    auto window = mope::glfw::window{ 1024, 768, "Pong" };
    window.set_cursor_mode(mope::glfw::window::cursor_mode::disabled);

    auto engine = mope::game_engine{ logger };
    engine.set_tick_rate(60.0);
    engine.run(window, std::make_unique<pong>());

    return EXIT_SUCCESS;
}
