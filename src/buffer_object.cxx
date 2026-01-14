#include "buffer_object.hxx"

#include "mope_game_engine/resource_id.hxx"
#include "glad/glad.h"

mope::gl::buffer_object::buffer_object(int target)
    : m_target{ target }
{
}

void mope::gl::buffer_object::fill(const void* data, std::size_t size)
{
    bind();
    ::glBufferData(m_target, size, data, GL_STATIC_DRAW);
}

void mope::gl::buffer_object::bind()
{
    if (!m_id) {
        auto id = GLuint{};
        ::glGenBuffers(1, &id);
        m_id = resource_id{
            id,
            [](unsigned int id) {
                ::glDeleteBuffers(1, &id);
             }
        };
    }
    ::glBindBuffer(m_target, m_id);
}

mope::gl::vbo::vbo()
    : buffer_object{ GL_ARRAY_BUFFER }
{
}

mope::gl::ebo::ebo()
    : buffer_object{ GL_ELEMENT_ARRAY_BUFFER }
{
}
