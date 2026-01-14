#pragma once

#include "mope_game_engine/mope_game_engine_export.hxx"
#include "mope_game_engine/resource_id.hxx"

#include <array>

namespace mope::gl
{
    ////////////////////////////////////////////////////////////////////////////
    /// @class buffer_object
    /// @brief @todo
    /// @details @todo
    ////////////////////////////////////////////////////////////////////////////
    class buffer_object
    {
    public:
        buffer_object(int target);

        void fill(const void* data, std::size_t size);

        template <typename T, std::size_t N>
        void fill(const std::array<T, N>& data)
        {
            fill(data.data(), data.size() * sizeof(T));
        }

        void bind();

    protected:
        ~buffer_object() = default;

    private:
        resource_id m_id;
        int m_target;
    };

    class vbo : public buffer_object
    {
    public:
        vbo();
    };

    class ebo : public buffer_object
    {
    public:
        ebo();
    };
} // namespace mope::gl
