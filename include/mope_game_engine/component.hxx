#pragma once

#include <concepts>
#include <cstdint>
#include <memory>
#include <ranges>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

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
    constexpr bool is_specialization_v = is_specialization<T, Template>::value;

    template <typename T, template <typename...> typename Template>
    concept specialization
        = is_specialization_v<std::remove_cvref_t<T>, Template>;
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
        entity_component(entity_id entity)
            : entity{ entity }
        {
        }

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

    /// A subsystem that causes nested queries for matching entities.
    ///
    /// A specialization of this struct can be passed instead of a component to
    /// define @ref game_system queries. This will cause the query to return a
    /// view of a subquery for entities with the subsystem components, unrelated
    /// to the main query. This allows one to nest iterations over entities with
    /// disparate sets of components.
    ///
    /// Subsystems cannot be nested, as this would be functionally the same as
    /// adding the nested subsystems to the top-level @ref game_system query.
    /// (iteration is associative).
    template <derived_from_entity_component... SubComponents>
    struct subsystem final
    {
    };

    template <derived_from_entity_component... RelatedComponents>
    struct relationship : public entity_component
    {
        relationship(entity_id entity, std::vector<entity_id> related)
            : entity_component{ entity }
            , related_entities{ std::move(related) }
        {
        }

        std::vector<entity_id> related_entities;
    };

    /// Concept describing what can be requested in @ref game_system queries.
    template <typename T>
    concept component_or_subsystem =
        component<T>
        || detail::specialization<T, subsystem>;
} // namespace mope
