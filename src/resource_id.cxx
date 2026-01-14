#include "mope_game_engine/resource_id.hxx"

namespace
{
    auto OutstandingCount{ 0 };
}

auto mope::gl::resource_id::outstanding_count() -> int
{
    return OutstandingCount;
}

mope::gl::resource_id::resource_id()
    : m_id{ 0 }
    , m_release{ nullptr }
    , m_use_count{ nullptr }
{
}

mope::gl::resource_id::resource_id(unsigned int id, void (*release)(unsigned int))
    : m_id{ id }
    , m_release{ release }
    , m_use_count{ new long(1) }
{
    ++OutstandingCount;
}

mope::gl::resource_id::resource_id(resource_id const& that)
    : m_id{ that.m_id }
    , m_release{ that.m_release }
    , m_use_count{ that.m_use_count }
{
    if (nullptr != m_use_count) {
        ++*m_use_count;
    }
}

mope::gl::resource_id::resource_id(resource_id&& that) noexcept
    : resource_id{}
{
    swap(*this, that);
}

auto mope::gl::resource_id::operator=(resource_id that) noexcept -> resource_id&
{
    swap(*this, that);
    return *this;
}

mope::gl::resource_id::~resource_id()
{
    if (nullptr != m_use_count) {
        if (0 == --*m_use_count) {
            if (0 != m_id && nullptr != m_release) {
                m_release(m_id);
                --OutstandingCount;
            }
            delete m_use_count;
        }
    }
}

mope::gl::resource_id::operator unsigned int() const
{
    return m_id;
}

mope::gl::resource_id::operator bool() const
{
    return 0 != m_id;
}
