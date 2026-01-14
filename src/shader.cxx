#include "shader.hxx"

#include "mope_game_engine/game_engine_error.hxx"
#include "mope_game_engine/resource_id.hxx"
#include "mope_vec/mope_vec.hxx"
#include "glad/glad.h"

namespace
{
    auto compile_shader(const char* src, GLenum type) -> GLuint
    {
        GLuint shader = ::glCreateShader(type);
        ::glShaderSource(shader, 1, &src, 0);
        ::glCompileShader(shader);

        int success;
        ::glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            throw mope::game_engine_error{ "Shader compilation failed." };
        }
        return shader;
    }

    void link_shader_program(GLuint program, GLuint vert_shader, GLuint frag_shader)
    {
        ::glAttachShader(program, vert_shader);
        ::glAttachShader(program, frag_shader);
        ::glLinkProgram(program);

        int success;
        ::glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            throw mope::game_engine_error{ "Shader linking failed." };
        }

        ::glDetachShader(program, frag_shader);
        ::glDetachShader(program, vert_shader);
    }

    inline void delete_shader(GLuint shader)
    {
        ::glDeleteShader(shader);
    }
}

void mope::gl::shader::make(char const* vert_source, char const* frag_source)
{
    auto vert_shader = compile_shader(vert_source, GL_VERTEX_SHADER);
    auto frag_shader = compile_shader(frag_source, GL_FRAGMENT_SHADER);
    link_shader_program(ensure_id(), vert_shader, frag_shader);
    delete_shader(frag_shader);
    delete_shader(vert_shader);
}

void mope::gl::shader::bind()
{
    ::glUseProgram(ensure_id());
}

auto mope::gl::shader::ensure_id() -> resource_id const&
{
    if (!m_id) {
        auto id = ::glCreateProgram();
        m_id = resource_id{ id, ::glDeleteProgram };
    }
    return m_id;
}

void mope::gl::shader::set_uniform_impl(char const* name, float value)
{
    GLint loc = ::glGetUniformLocation(ensure_id(), name);
    ::glUniform1f(loc, value);
}

void mope::gl::shader::set_uniform_impl(char const* name, int value)
{
    GLint loc = ::glGetUniformLocation(ensure_id(), name);
    ::glUniform1i(loc, value);
}

void mope::gl::shader::set_uniform_impl(char const* name, vec2f const& value)
{
    GLint loc = ::glGetUniformLocation(ensure_id(), name);
    ::glUniform2fv(loc, 1, value.data());
}

void mope::gl::shader::set_uniform_impl(char const* name, mat2f const& value)
{
    GLint loc = ::glGetUniformLocation(ensure_id(), name);
    ::glUniformMatrix2fv(loc, 1, false, &value[0][0]);
}

void mope::gl::shader::set_uniform_impl(char const* name, mat3f const& value)
{
    GLint loc = ::glGetUniformLocation(ensure_id(), name);
    ::glUniformMatrix3fv(loc, 1, false, &value[0][0]);
}

void mope::gl::shader::set_uniform_impl(char const* name, mat4f const& value)
{
    GLint loc = ::glGetUniformLocation(ensure_id(), name);
    ::glUniformMatrix4fv(loc, 1, false, &value[0][0]);
}
