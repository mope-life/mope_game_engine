#pragma once

#include "mope_game_engine/component.hxx"
#include "mope_game_engine/transforms.hxx"
#include "mope_vec/mope_vec.hxx"

#include <optional>
#include <utility>

namespace mope
{
    struct transform_component : public entity_component
    {
        transform_component(entity en, vec3f position, vec3f size)
            : entity_component{ en }
            , m_position{ std::move(position) }
            , m_size{ std::move(size) }
            , m_stale{ false }
            , m_model{ build_model() }
            , m_saved_model{ std::nullopt }
        { }

        auto position() const -> vec3f const&
        {
            return m_position;
        }

        auto x_position() const -> float
        {
            return m_position.x();
        }

        auto y_position() const -> float
        {
            return m_position.y();
        }

        auto z_position() const -> float
        {
            return m_position.z();
        }

        void set_position(vec3f p)
        {
            m_position = std::move(p);
            m_stale = true;
        }

        void set_x(float x)
        {
            m_position.x() = x;
            m_stale = true;
        }

        void set_y(float y)
        {
            m_position.y() = y;
            m_stale = true;
        }

        void set_z(float z)
        {
            m_position.z() = z;
            m_stale = true;
        }

        void slide(vec3f const& dpos)
        {
            m_position += dpos;
            m_stale = true;
        }

        void slide_x(float dx)
        {
            m_position.x() += dx;
            m_stale = true;
        }

        void slide_y(float dy)
        {
            m_position.y() += dy;
            m_stale = true;
        }

        void slide_z(float dz)
        {
            m_position.z() += dz;
            m_stale = true;
        }

        auto size() const -> vec3f const&
        {
            return m_size;
        }

        auto x_size() const -> float
        {
            return m_size.x();
        }

        auto y_size() const -> float
        {
            return m_size.y();
        }

        auto z_size() const -> float
        {
            return m_size.z();
        }

        void set_size(vec3f s)
        {
            m_size = std::move(s);
            m_stale = true;
        }

        auto get_model() const -> mat4f const&
        {
            if (m_stale) {
                m_model = build_model();
                m_stale = false;
            }
            return m_model;
        }

        void save_model()
        {
            m_saved_model = get_model();
        }

        auto blend(float alpha) const -> mat4f
        {
            auto const& model = get_model();
            if (m_saved_model.has_value()) {
                return *m_saved_model + alpha * (model - *m_saved_model);
            }
            else {
                return model;
            }
        }

    private:
        auto build_model() const -> mat4f
        {
            return gl::translation_matrix(m_position) * gl::scale_matrix(m_size);
        }

        vec3f m_position;
        vec3f m_size;
        mutable bool m_stale;
        mutable mat4f m_model;
        std::optional<mat4f> m_saved_model;
    };
} // namespace mope
