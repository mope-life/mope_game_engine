#pragma once

#include "mope_game_engine/resource_id.hxx"
#include "mope_vec/mope_vec.hxx"

namespace mope::gl
{
    class shader
    {
    public:
        void make(char const* vert_source, char const* frag_source);
        void bind();

        template <typename T>
        void set_uniform(char const* name, T&& t)
        {
            set_uniform_impl(name, t);
        }

    private:
        auto ensure_id() -> resource_id const&;
        void set_uniform_impl(char const* name, float value);
        void set_uniform_impl(char const* name, int value);
        void set_uniform_impl(char const* name, vec2f const& value);
        void set_uniform_impl(char const* name, mat2f const& value);
        void set_uniform_impl(char const* name, mat3f const& value);
        void set_uniform_impl(char const* name, mat4f const& value);

        resource_id m_id;
    };
} // namespace mope::gl
