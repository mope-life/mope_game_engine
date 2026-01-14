#include "mope_game_engine/mope_game_engine_export.hxx"
#include "mope_vec/mope_vec.hxx"

#include <optional>

namespace mope
{
    class game_object;

    struct MOPE_GAME_ENGINE_EXPORT ray
    {
        vec3d origin;
        vec3d velocity;
    };

    struct MOPE_GAME_ENGINE_EXPORT bounding_box
    {
        vec3d anchor;
        vec3d opposite;
    };

    struct MOPE_GAME_ENGINE_EXPORT collision
    {
        double contact_time;
        vec3d contact_point;
        vec3d contact_normal;
    };

    auto MOPE_GAME_ENGINE_EXPORT ray_bounding_box_collision(
        ray const& r, bounding_box const& bb
    ) -> std::optional<collision>;

    auto MOPE_GAME_ENGINE_EXPORT axis_aligned_object_collision(
        vec3f const& actor_position, vec3f const& actor_size, vec3f const& actor_velocity,
        vec3f const& target_position, vec3f const& target_size
    ) -> std::optional<collision>;
}
