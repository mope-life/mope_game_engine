#pragma once

#include "mope_game_engine/mope_game_engine_export.hxx"
#include "mope_game_engine/resource_id.hxx"

#include <concepts>
#include <type_traits>

namespace mope::gl
{
    ////////////////////////////////////////////////////////////////////////////
    /// @class attribute
    /// @brief @todo
    /// @details @todo
    ////////////////////////////////////////////////////////////////////////////
    struct attribute
    {
        unsigned int index;
        int size;

        enum
        {
            float_type
        } type;

        int stride;
        std::size_t offset;
        std::size_t divisor = 0;
    };

    ////////////////////////////////////////////////////////////////////////////
    /// @class vao
    /// @brief @todo
    /// @details @todo
    ////////////////////////////////////////////////////////////////////////////
    class vao
    {
    public:
        void add_attribute(attribute const& attr);

        template <std::same_as<std::remove_cvref_t<attribute>>... Attributes>
        void add_attributes(Attributes&&... attributes)
        {
            (add_attribute(std::forward<Attributes>(attributes)), ...);
        }

        void bind();

    private:
        resource_id m_id;
    };
} // namespace mope::gl
