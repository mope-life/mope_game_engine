#pragma once

#include <utility>

namespace mope::gl
{
    ////////////////////////////////////////////////////////////////////////////
    /// @class resource_id
    /// @brief A handle to an OpenGL resource that must be cleaned up.
    /// @details @todo
    ////////////////////////////////////////////////////////////////////////////
    class resource_id final
    {
    public:
        static auto outstanding_count() -> int;

        resource_id();
        resource_id(unsigned int id, void (*release)(unsigned int));
        resource_id(resource_id const& that);
        resource_id(resource_id&& that) noexcept;
        ~resource_id();

        auto operator=(resource_id) noexcept -> resource_id&;
        operator unsigned int() const;
        operator bool() const;

        friend void swap(resource_id& a, resource_id& b)
        {
            using std::swap;
            swap(a.m_id, b.m_id);
            swap(a.m_release, b.m_release);
            swap(a.m_use_count, b.m_use_count);
        }

    private:
        unsigned int m_id;
        void (*m_release)(unsigned int);
        long* m_use_count;
    };
} // namespace mope::gl
