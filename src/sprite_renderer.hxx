#pragma once

#include "buffer_object.hxx"
#include "shader.hxx"
#include "vao.hxx"

namespace mope
{
    class ecs_manager;

    class sprite_renderer
    {
    public:
        sprite_renderer();
        void pre_tick(ecs_manager& ecs);
        void render(ecs_manager& ecs, double alpha);

        gl::shader m_shader;
        gl::vao m_vao;
        gl::vbo m_vbo;
        gl::ebo m_ebo;
    };
} // namespace mope
