#include "mope_game_engine/collisions.hxx"
#include "mope_vec/mope_vec.hxx"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

auto mope::ray_bounding_box_collision(
    ray const& r, bounding_box const& bb
) -> std::optional<collision>
{
    auto t_u = (bb.anchor - r.origin).hadamard_division(r.velocity);
    auto t_v = (bb.opposite - r.origin).hadamard_division(r.velocity);

    constexpr auto inf = std::numeric_limits<double>::infinity();

    auto mins = {
        std::min(std::max(t_u.x(), -inf), std::max(t_v.x(), -inf)),
        std::min(std::max(t_u.y(), -inf), std::max(t_v.y(), -inf)),
        std::min(std::max(t_u.z(), -inf), std::max(t_v.z(), -inf)),
    };
    auto t_min = std::max_element(mins.begin(), mins.end());

    auto maxes = {
        std::max(std::min(t_u.x(), inf), std::min(t_v.x(), inf)),
        std::max(std::min(t_u.y(), inf), std::min(t_v.y(), inf)),
        std::max(std::min(t_u.z(), inf), std::min(t_v.z(), inf)),
    };
    auto t_max = std::min_element(maxes.begin(), maxes.end());

    if (*t_min > *t_max) {
        return std::nullopt;
    }
    else {
        auto contact_point = r.origin + *t_min * r.velocity;
        auto contact_normal = vec3d{ 0.0, 0.0, 0.0 };
        auto i = t_min - mins.begin();
        contact_normal[i] = std::copysign(1.0, r.origin[i] - contact_point[i]);
        return collision{
            .contact_time = *t_min,
            .contact_point = contact_point,
            .contact_normal = contact_normal,
        };
    }
}

auto mope::axis_aligned_object_collision(
    vec3f const& actor_position, vec3f const& actor_size, vec3f const& actor_velocity,
    vec3f const& target_position, vec3f const& target_size
) -> std::optional<collision>
{
    auto half_size = 0.5 * actor_size;

    auto bb = bounding_box{};
    for (auto i = std::size_t{ 0 }; i < 3; ++i) {
        auto padding = std::copysign(half_size[i], target_size[i]);
        bb.anchor[i] = target_position[i] - padding;
        bb.opposite[i] = target_position[i] + target_size[i] + padding;
    }

    ray r = {
        .origin = actor_position + half_size,
        .velocity = static_cast<vec3d>(actor_velocity)
    };

    return ray_bounding_box_collision(r, bb);
}
