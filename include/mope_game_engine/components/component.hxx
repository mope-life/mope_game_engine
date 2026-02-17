#pragma once

#include <concepts>
#include <cstdint>

namespace mope::detail
{
    template <typename, template <typename...> typename>
    struct is_specialization_of : std::false_type
    {
    };

    template <template <typename...> typename Template, typename... Ts>
    struct is_specialization_of<Template<Ts...>, Template> : public std::true_type
    {
    };

    template <typename T, template <typename...> typename Template>
    concept specialization = is_specialization_of<T, Template>::value;
}

namespace mope
{
    using entity_id = uint64_t;
    constexpr auto NoEntity = entity_id{ 0 };

    /// A component attached to an entity.
    ///
    /// These components can be requested by a @ref game_system in the
    /// @ref game_system::process_tick() override.
    struct entity_component
    {
        entity_id entity;
    };

    /// A component not attached to any entity.
    ///
    /// A singleton component is meant to hold data accessible to any system,
    /// that doesn't make sense to attach to a particular entity. This can
    /// include per-tick data, such as mouse/keyboard input. It can be requested
    /// in the process_tick method of a user system just as with an
    /// @ref entity_component.
    struct singleton_component
    {
    };

    /// A relationship between two entities.
    ///
    /// Relationships are attached to entities like components. However, unlike
    /// components, more than one relationship can be attached to an entity: one
    /// for each other entity it is related to.
    ///
    /// Relationships are asymmetric: if A is related to B, B isn't necessarily
    /// related to A.
    struct relationship : public entity_component
    {
        /// The other entity to which the entity owning this component is
        /// related.
        entity_id related_entity;
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
    /// but GCC does not. (Rather, GCC complains that `std::derived_from<...>` is
    /// not more constrained than the logical XOR.)
    template <typename T>
    concept component
        = derived_from_singleton_component<T> || derived_from_entity_component<T>;
} // namespace mope
