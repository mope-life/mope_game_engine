#pragma once

#include "mope_game_engine/component.hxx"
#include "mope_game_engine/game_scene.hxx"

#include <concepts>
#include <ranges>
#include <tuple>
#include <utility>
#include <type_traits>

namespace mope::detail
{
    template <component PrimaryComponent, component_or_relationship... AdditionalComponents>
    struct component_gatherer
    {
        static auto gather(game_scene& scene);
    };

    template <component_or_relationship ComponentOrRelationship>
    struct additional_component_gatherer;

    template <component Component>
    struct additional_component_gatherer<Component>
    {
        template <component PrimaryComponent>
        static auto gather(game_scene& scene, PrimaryComponent const& c0)
        {
            if constexpr (derived_from_singleton_component<Component>) {
                return scene.get_component<Component>();
            }
            else {
                static_assert(
                    derived_from_entity_component<PrimaryComponent>,
                    "Entity components may not be used in a system where the primary component is a singleton. "
                    "Reorder your components such that an entity component is the first parameter.");

                return scene.get_component<Component>(c0.en);
            }
        }
    };

    template <derived_from_entity_component... AdditionalComponents>
    struct additional_component_gatherer<relationship<AdditionalComponents...>>
    {
        template <component PrimaryComponent>
        static auto gather(game_scene& scene, PrimaryComponent const&)
        {
            return component_gatherer<AdditionalComponents...>::gather(scene);
        }
    };

    /// Return true if @p t is a `nullptr_t` or a pointer equal to `nullptr`.
    template <typename T>
    auto is_nullptr_or_null_pointer(T const& t) -> bool
    {
        if constexpr (std::is_null_pointer_v<T>) {
            return true;
        }
        else if constexpr (std::is_pointer_v<T>) {
            return nullptr == t;
        }
        else {
            return false;
        }
    }

    /// The type of each element returned by @ref dereference_if_pointer(...).
    ///
    /// Unfortunately, there is no way to get anything in the STL (as far as I
    /// know) to deduce the types we need. `std::make_tuple()` on its own will
    /// fail to forward lvalue references, and `std::forward_as_tuple()` won't
    /// work either, because it will forward our rvalues (the sub-views for
    /// relationships) as rvalue references, that then become dangling. What
    /// we want is to leave the derefed pointers as references, while actually
    /// materializing the rvalue views as tuple members.
    template <typename T>
    using DeferencedTupleElement = std::conditional_t<
        std::is_pointer_v<std::remove_reference_t<T>>,
        std::add_lvalue_reference_t<std::remove_pointer_t<std::remove_reference_t<T>>>,
        std::remove_reference_t<T>
    >;

    /// Dereference a possibly cv-qualified T*, returning the cv-qualified T&.
    ///
    /// All other types are returned as they were found.
    template <typename T>
    decltype(auto) dereference_if_pointer(T&& t)
    {
        if constexpr (std::is_pointer_v<std::remove_reference_t<T>>) {
            return *t;
        }
        else {
            return t;
        }
    }

    /// Dereference all T* to T&, then return all parameters in a tuple.
    ///
    /// As a special case, if only one item is provided, don't bother wrapping
    /// it in a tuple; just return that item by itself (still dereferencing it
    /// if it is a pointer).
    template <typename T, typename... Ts>
    decltype(auto) make_dereferenced_tuple(T&& t, Ts&&... ts)
    {
        if constexpr (0 == sizeof...(ts)) {
            return dereference_if_pointer(std::forward<T>(t));
        }
        else {
            return std::tuple<DeferencedTupleElement<T>, DeferencedTupleElement<Ts>...>{
                dereference_if_pointer(std::forward<T>(t)),
                    dereference_if_pointer(std::forward<Ts>(ts))...
            };
        }
    }

    template <component PrimaryComponent, component_or_relationship... AdditionalComponents>
    auto component_gatherer<PrimaryComponent, AdditionalComponents...>::gather(game_scene& scene)
    {
        // We need to handle the "primary" component separately because we need
        // the entity id from that component in order to capture the remaining
        // entity components.
        return scene.get_components<PrimaryComponent>()
            | std::views::transform([&scene](auto&& c0)
                {
                    return std::make_tuple(
                        &c0, additional_component_gatherer<AdditionalComponents>::gather(scene, c0)...
                    );
                })
            | std::views::filter([](auto const& tup)
                {
                    // Filter out any tuples containing nullptr. This would mean
                    // that an entity had the first ("primary") component, but
                    // not at least one of the other requested components.
                    return std::apply([](auto const&... elements)
                        {
                            return (!is_nullptr_or_null_pointer(elements) && ...);
                        },
                        tup
                    );
                })
            | std::views::transform([](auto&& tup) -> decltype(auto)
                {
                    // Use `decltype(auto)` here because
                    // `make_dereferenced_tuple()` will either return a tuple
                    // by value OR a reference to a component in case only one
                    // component is in the query.
                    return std::apply([](auto&&... elements) -> decltype(auto)
                        {
                            return make_dereferenced_tuple(std::forward<decltype(elements)>(elements)...);
                        },
                        tup
                    );
                });
    }
} // namespace mope::detail

namespace mope
{
    template <component PrimaryComponent, component_or_relationship... AdditionalComponents>
    auto component_query(game_scene& scene)
    {
        return detail::component_gatherer<PrimaryComponent, AdditionalComponents...>::gather(scene);
    }
}
