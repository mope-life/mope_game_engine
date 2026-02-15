#include "mope_game_engine/texture.hxx"

#include "glad/glad.h"
#include "mope_game_engine/resource_id.hxx"
#include "mope_vec/mope_vec.hxx"

#include <algorithm>
#include <array>
#include <cstddef>
#include <ranges>
#include <utility>

namespace
{
    using namespace mope;
    using namespace mope::gl;

    auto map_pixel_format(pixel_format format) -> std::pair<GLint, GLenum>
    {
        switch (format) {
        case pixel_format::r: return { GL_R8, GL_RED };
        case pixel_format::rg: return { GL_RG8, GL_RG };
        case pixel_format::rgb: return { GL_RGB8, GL_RGB };
        case pixel_format::bgr: return { GL_RGB8, GL_BGR };
        case pixel_format::rgba: return { GL_RGBA8, GL_RGBA };
        case pixel_format::bgra: return { GL_RGBA8, GL_BGRA };
        default: std::unreachable();
        }
    }

    auto map_color_component(color_component component) -> GLenum
    {
        switch (component) {
        case color_component::red: return GL_RED;
        case color_component::green: return GL_GREEN;
        case color_component::blue: return GL_BLUE;
        case color_component::alpha: return GL_ALPHA;
        case color_component::one: return GL_ONE;
        case color_component::zero: return GL_ZERO;
        default: std::unreachable();
        }
    }

    auto map_min_filter(texture_min_filter min_filter) -> std::pair<GLenum, bool>
    {
        switch (min_filter) {
        case texture_min_filter::nearest: return { GL_NEAREST, false };
        case texture_min_filter::linear: return { GL_LINEAR, false };
        case texture_min_filter::nearest_mipmap_nearest: return { GL_NEAREST_MIPMAP_NEAREST, true };
        case texture_min_filter::linear_mipmap_nearest: return { GL_LINEAR_MIPMAP_NEAREST, true };
        case texture_min_filter::nearest_mipmap_linear: return { GL_NEAREST_MIPMAP_LINEAR, true };
        case texture_min_filter::linear_mipmap_linear: return { GL_LINEAR_MIPMAP_LINEAR, true };
        default: std::unreachable();
        }
    }

    auto map_mag_filter(texture_mag_filter mag_filter) -> GLenum
    {
        switch (mag_filter) {
        case texture_mag_filter::nearest: return GL_NEAREST;
        case texture_mag_filter::linear: return GL_LINEAR;
        default: std::unreachable();
        }
    }
}

void mope::gl::texture::bind()
{
    if (!m_id) {
        auto id = GLuint{};
        ::glGenTextures(1, &id);
        m_id = resource_id{
            id,
            [](GLuint id) {
                ::glDeleteTextures(1, &id);
            }
        };
    }
    ::glBindTexture(GL_TEXTURE_2D, m_id);
}

auto mope::gl::texture::make(
    std::byte const* bytes,
    vec2i size,
    pixel_format input_format,
    texture_extra_options const& extra_options
) & -> texture&
{
    bind();

    ::glPixelStorei(GL_UNPACK_ALIGNMENT, extra_options.row_alignment);
    auto&& [internal_format, format] = map_pixel_format(input_format);

    ::glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internal_format,
        size.x(),
        size.y(),
        0,
        format,
        GL_UNSIGNED_BYTE,
        bytes
    );

    auto mag_filter = map_mag_filter(extra_options.mag_filter);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, mag_filter);

    auto [min_filter, gen_mipmap] = map_min_filter(extra_options.min_filter);
    ::glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, min_filter);
    if (gen_mipmap) {
        ::glGenerateMipmap(GL_TEXTURE_2D);
    }

    return *this;
}

auto mope::gl::texture::make(
    std::byte const* bytes,
    vec2i size,
    pixel_format input_format,
    texture_extra_options const& extra_options
) && -> texture&&
{
    return std::move(make(bytes, size, input_format, extra_options));
}

auto mope::gl::texture::swizzle(std::array<color_component, 4> const& sources) & -> texture&
{
    bind();

    auto swizzle_mask = std::array<GLint, 4>{};
    auto view = sources | std::views::transform([](auto&& component)
        {
            return map_color_component(component);
        });
    std::copy_n(view.begin(), 4, swizzle_mask.begin());

    ::glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzle_mask.data());
    return *this;
}

auto mope::gl::texture::swizzle(std::array<color_component, 4> const& sources) && -> texture&&
{
    return std::move(swizzle(sources));
}
