#pragma once

#include "buffer_object.hxx"
#include "mope_vec/mope_vec.hxx"
#include "shader.hxx"
#include "vao.hxx"

namespace mope
{
    class game_scene;

    class sprite_renderer
    {
    public:
        sprite_renderer();
        void set_projection(mope::mat4f const& projection);
        void pre_tick(game_scene& scene);
        void render(game_scene& scene, double alpha);

    private:
        gl::shader m_shader;
        gl::vao m_vao;
        gl::vbo m_vbo;
        gl::ebo m_ebo;
    };
} // namespace mope
