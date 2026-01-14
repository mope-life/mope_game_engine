#include "vao.hxx"

#include "mope_game_engine/game_engine_error.hxx"
#include "mope_game_engine/resource_id.hxx"
#include "glad/glad.h"

#include <string>

namespace
{
    auto map_attribute_type(decltype(mope::gl::attribute::type) attr) -> GLenum
    {
        using attribute_type = decltype(mope::gl::attribute::type);

        switch (attr)
        {
        case attribute_type::float_type:
            return GL_FLOAT;
        default:
            throw mope::game_engine_error{
                "Invalid attribute type: " + std::to_string(attr)
            };
        }
    }
}

void mope::gl::vao::add_attribute(const attribute& attr)
{
    bind();
    ::glVertexAttribPointer(
        attr.index,
        attr.size,
        map_attribute_type(attr.type),
        GL_FALSE,
        attr.stride,
        reinterpret_cast<void*>(attr.offset)
    );
    ::glEnableVertexAttribArray(attr.index);
    ::glVertexAttribDivisor(attr.index, attr.divisor);
}

void mope::gl::vao::bind()
{
    if (!m_id) {
        auto id = GLuint{};
        ::glGenVertexArrays(1, &id);
        m_id = resource_id{
            id,
            [](unsigned int id) {
                ::glDeleteVertexArrays(1, &id);
            }
        };
    }
    ::glBindVertexArray(m_id);
}
