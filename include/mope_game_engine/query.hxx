#pragma once

#include "mope_game_engine/component_manager.hxx"

#include <ranges>
#include <optional>

namespace mope::detail
{
    template <derived_from_entity_component Component>
    auto get_from_manager(component_manager& manager, entity_id entity)
    {
        return manager.get_component<Component>(entity);
    }

    template <derived_from_singleton_component Component>
    auto get_from_manager(component_manager& manager, entity_id)
    {
        return manager.get_component<Component>();
    }

    template <typename T>
    auto is_not_null(T const& t) -> bool
    {
        if constexpr (std::is_pointer_v<T>) {
            return t != nullptr;
        }
        else {
            return true;
        }
    }

    template <typename T>
    decltype(auto) deref(T&& t)
    {
        if constexpr (std::is_pointer_v<std::remove_reference_t<T>>) {
            return *t;
        }
        else {
            return std::forward<T>(t);
        }
    }
}

namespace mope
{
    template <component PrimaryComponent, component... Components>
    struct query_components
    {
        query_components(component_manager& manager)
            : m_manager{ manager }
            , m_view{ make_view(manager) }
        { }

        auto begin()
        {
            return m_view.begin();
        }

        auto end()
        {
            return m_view.end();
        }

    private:
        static auto make_view(component_manager& manager)
        {
            return manager.get_components<PrimaryComponent>()
                | std::views::transform([&manager](auto&& primary_component)
                    {
                        /// TODO: This is re-querying singleton components for each primary component.
                        /// We should find a way to collect the singleton components first so that they
                        /// only need to be retrieved once.
                        if constexpr (derived_from_entity_component<PrimaryComponent>) {
                            return std::make_tuple(
                                std::ref(primary_component),
                                detail::get_from_manager<Components>(manager, primary_component.entity)...
                            );
                        }
                        else {
                            static_assert(!(derived_from_entity_component<Components> || ...),
                                "Entity components may not be used in a query where the primary component is a singleton. "
                                "Reorder your components such that an entity component is the initial query.");

                            return std::make_tuple(
                                std::ref(primary_component),
                                manager.get_component<Components>()...
                            );
                        }
                    })
                | std::views::filter([](auto const& tup)
                    {
                        return std::apply([](auto const&... m)
                            {
                                return (detail::is_not_null(m) && ...);
                            },
                            tup);
                    })
                | std::views::transform([](auto&& tup) -> decltype(auto)
                    {
                        if constexpr (0 == sizeof...(Components)) {
                            return std::get<0>(std::forward<decltype(tup)>(tup));
                        }
                        else {
                            return std::apply([](auto&&... ms)
                                {
                                    return std::forward_as_tuple(detail::deref(ms)...);
                                },
                                std::forward<decltype(tup)>(tup));
                        }
                    });
        }

        component_manager& m_manager;
        using view_type = decltype(make_view(std::declval<component_manager&>()));
        view_type m_view;
    };
}
