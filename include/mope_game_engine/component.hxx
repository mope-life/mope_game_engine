#pragma once

#include "mope_game_engine/mope_game_engine_export.hxx"

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace mope::detail
{
    template <typename T, template <typename...> typename Template>
    struct is_specialization : public std::false_type
    {
    };

    template <template <typename...> typename Template, typename... Parameters>
    struct is_specialization<Template<Parameters...>, Template> : public std::true_type
    {
    };

    template <typename T, template <typename...> typename Template>
    concept specialization
        = is_specialization<std::remove_cvref_t<T>, Template>::value;
}

namespace mope
{
    using entity = uint64_t;

    /// A component attached to an entity.
    ///
    /// These components can be requested by a @ref game_system in the
    /// @ref game_system::process_tick() override.
    struct MOPE_GAME_ENGINE_EXPORT entity_component
    {
        entity_component(entity en)
            : en{ en }
        {
        }

        entity en;
    };

    /// A component not attached to any entity.
    ///
    /// A singleton component is meant to hold data accessible to any system,
    /// that doesn't make sense to attach to a particular entity. This can
    /// include per-tick data, such as mouse/keyboard input. It can be requested
    /// in the process_tick method of a user system just as with an
    /// @ref entity_component.
    struct MOPE_GAME_ENGINE_EXPORT singleton_component
    {
    };

    template <typename T>
    concept derived_from_singleton_component
        = std::derived_from<T, singleton_component>
        && !std::derived_from<T, entity_component>;

    template <typename T>
    concept derived_from_entity_component
        = std::derived_from<T, entity_component>
        && !std::derived_from<T, singleton_component>;

    /// Concept describing a component in the ECS architecture.
    ///
    /// Every component is either a singleton XOR an entity component.
    ///
    /// NOTE: Don't try to simplify this. MSVC accepts
    /// ```
    ///     std::derived_from<T, singleton_component> != std::derived_from<T, entity_component>
    /// ```
    /// but GCC does not.
    template <typename T>
    concept component
        = derived_from_singleton_component<T> || derived_from_entity_component<T>;

    /// Generic representation of a relationship between components.
    ///
    /// This struct doesn't really mean anything on its own. It can be used as
    /// template arguments in a @ref game_system to query for a sub-view of
    /// components, not tied to the same entity as the other
    /// @ref entity_components in the query.
    template <derived_from_entity_component... RelatedComponents>
    struct relationship final
    {
    };

    /// Concept describing what can be requested in @ref game_system queries.
    template <typename T>
    concept component_or_relationship =
        component<T>
        || detail::specialization<T, relationship>;
} // namespace mope
