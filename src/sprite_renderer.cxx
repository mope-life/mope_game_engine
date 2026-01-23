#include "sprite_renderer.hxx"

#include "glad/glad.h"
#include "mope_game_engine/components/sprite.hxx"
#include "mope_game_engine/components/transform.hxx"
#include "mope_game_engine/game_scene.hxx"
#include "mope_game_engine/game_system.hxx"
#include "mope_vec/mope_vec.hxx"
#include "vao.hxx"

#include <array>
#include <cstdint>

mope::sprite_renderer::sprite_renderer()
    : m_shader{ }
    , m_vao{ }
    , m_vbo{ }
    , m_ebo{ }
{
    m_shader.make(R"%%(
#version 330 core
uniform mat4 u_model;
uniform mat4 u_view;
uniform mat4 u_projection;
layout (location = 0) in vec3 i_pos;
layout (location = 1) in vec4 i_color;
out vec4 frag_color;
out vec2 tex_coord;
void main()
{
    frag_color = i_color;
    tex_coord = i_pos.xy;
    gl_Position = u_projection * u_view * u_model * vec4(i_pos, 1.0f);
}
)%%", R"%%(
#version 330 core
in vec4 frag_color;
in vec2 tex_coord;
out vec4 o_color;
uniform sampler2D u_texture_2d;
void main()
{
    // o_color = texture(u_texture_2d, tex_coord) * frag_color;
    // o_color = frag_color;
    o_color = texture(u_texture_2d, tex_coord);
}
)%%");
    m_shader.bind();
    m_shader.set_uniform("u_model", mat4f::identity());
    m_shader.set_uniform("u_view", mat4f::identity());
    m_shader.set_uniform("u_projection", mat4f::identity());

    m_vao.bind();
    constexpr auto vertices = std::to_array<float>({
        // coordinates
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        // color
        1.0f, 1.0f, 1.0f, 1.0f,
        });
    m_vbo.fill(vertices);
    m_vao.add_attribute(gl::attribute{
        .index = 0,
        .size = 3,
        .type = gl::attribute::float_type,
        .stride = 0,
        .offset = 0,
        });
    m_vao.add_attribute(gl::attribute{
        .index = 1,
        .size = 4,
        .type = gl::attribute::float_type,
        .stride = 0,
        .offset = 3 * 4 * sizeof(float),
        .divisor = 1,
        });
    constexpr auto indices = std::to_array<uint8_t>({ 0, 1, 2, 3 });
    m_ebo.fill(indices);
}

void mope::sprite_renderer::pre_tick(game_scene& scene)
{
    for (auto&& [sprite, transform]
        : detail::component_gatherer<sprite_component, transform_component>::gather(scene))
    {
        transform.save_model();
    }
}

void mope::sprite_renderer::render(game_scene& scene, double alpha)
{
    for (auto&& [sprite, transform]
        : detail::component_gatherer<sprite_component, transform_component>::gather(scene))
    {
        sprite.texture.bind();
        m_shader.set_uniform("u_model", transform.blend(static_cast<float>(alpha)));
        m_vao.bind();
        ::glDrawElements(GL_TRIANGLE_STRIP, 4, GL_UNSIGNED_BYTE, 0);
    }
}
