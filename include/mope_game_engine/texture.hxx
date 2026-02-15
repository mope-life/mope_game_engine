#pragma once

#include "mope_game_engine/resource_id.hxx"
#include "mope_vec/mope_vec.hxx"

#include <array>

namespace mope::gl
{
    enum class pixel_format
    {
        r,
        rg,
        rgb,
        bgr,
        rgba,
        bgra,
    };

    enum class color_component
    {
        red,
        green,
        blue,
        alpha,
        one,
        zero,
    };

    enum class texture_min_filter
    {
        nearest,
        linear,
        nearest_mipmap_nearest,
        linear_mipmap_nearest,
        nearest_mipmap_linear,
        linear_mipmap_linear,
    };

    enum class texture_mag_filter
    {
        nearest,
        linear,
    };

    struct texture_extra_options
    {
        int row_alignment = 4;
        texture_min_filter min_filter = texture_min_filter::nearest_mipmap_linear;
        texture_mag_filter mag_filter = texture_mag_filter::linear;
    };

    class texture;

    class texture
    {
    public:
        void bind();

        auto make(
            std::byte const* bytes,
            vec2i size,
            pixel_format input_format,
            texture_extra_options const& extra_options = texture_extra_options{}
        ) & -> texture&;

        auto make(
            std::byte const* bytes,
            vec2i size,
            pixel_format input_format,
            texture_extra_options const& extra_options = texture_extra_options{}
        ) && -> texture&&;

        auto swizzle(std::array<color_component, 4> const& sources) & -> texture&;
        auto swizzle(std::array<color_component, 4> const& sources) && -> texture&&;

    private:
        resource_id m_id;
    };
} // namespace mope::gl
