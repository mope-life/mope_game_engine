#include "mope_game_engine/texture.hxx"

#include "mope_game_engine/game_engine_error.hxx"
#include "mope_game_engine/resource_id.hxx"
#include "glad/glad.h"

#include <string>
#include <type_traits>
#include <utility>

namespace
{
    using pixel_format = mope::gl::pixel_format;

    auto map_pixel_format(pixel_format format) -> std::pair<GLint, GLenum>
    {

        switch (format) {
        case pixel_format::r:
            return { GL_RED, GL_RED };
        case pixel_format::rg:
            return { GL_RG, GL_RG };
        case pixel_format::rgb:
            return { GL_RGB, GL_RGB };
        case pixel_format::bgr:
            return { GL_RGB, GL_BGR };
        case pixel_format::rgba:
            return { GL_RGBA, GL_RGBA };
        case pixel_format::bgra:
            return { GL_RGBA, GL_BGRA};
        default:
            throw mope::game_engine_error{
                "Invalid pixel format: "
                + std::to_string(static_cast<std::underlying_type_t<decltype(format)>>(format))
            };
        }
    }
}

void mope::gl::texture::make(
    unsigned char const* bytes, pixel_format format, std::size_t width, std::size_t height
)
{
    auto&& [internal_format, gl_format] = map_pixel_format(format);
    bind();
    ::glTexImage2D(
        GL_TEXTURE_2D,
        0,
        internal_format,
        static_cast<GLsizei>(width),
        static_cast<GLsizei>(height),
        0,
        gl_format,
        GL_UNSIGNED_BYTE,
        bytes
    );
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
