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

    template <typename T>
    struct wrap
    {
        template <typename U>
            requires std::same_as<T, std::remove_cvref_t<U>>
        static auto operator()(U&& u)
        {
            return std::forward_as_tuple(u);
        }
    };

    template <typename... T>
    struct wrap<std::tuple<T...>>
    {
        template <typename U>
            requires std::same_as<std::tuple<T...>, std::remove_cvref_t<U>>
        static decltype(auto) operator()(U&& u)
        {
            return std::forward<U>(u);
        }
    };

    template <typename... T>
    auto make_flat_tuple(T&&... ts)
    {
        return std::tuple_cat(wrap<std::remove_cvref_t<T>>{}(std::forward<T>(ts))...);
    }

    template <component PrimaryComponent, component... Components>
    auto gather_components(component_manager& manager)
    {
        if constexpr (0 == sizeof...(Components)) {
            return manager.get_components<PrimaryComponent>();
        }
        else {
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
                        return std::apply([](auto&&... ms)
                            {
                                return std::forward_as_tuple(detail::deref(ms)...);
                            },
                            std::forward<decltype(tup)>(tup));
                    });
        }
    }
}

namespace mope
{
    template <std::ranges::view View, component... Components>
    struct query_join;

    struct joinable_query_mixin
    {
    protected:
        template <std::ranges::view ViewT, component... JoinComponents>
        auto join_mixin(component_manager& manager, ViewT view)
            -> query_join<ViewT, JoinComponents...>;
    };

    template <std::ranges::view View, component... Components>
    struct query_join : public joinable_query_mixin
    {
        query_join(component_manager& manager, View previous_view)
            : m_manager{ manager }
            , m_view{ make_view(manager, std::move(previous_view)) }
        {
        }

        auto begin()
        {
            return m_view.begin();
        }

        auto end()
        {
            return m_view.end();
        }

        template <component... JoinComponents>
        auto join(this auto&& self)
        {
            return self.template join_mixin<view_type, JoinComponents...>(
                std::forward<decltype(self)>(self).m_manager,
                std::forward<decltype(self)>(self).m_view
            );
        }

    private:
        static auto make_view(component_manager& manager, View&& previous_view)
        {
            return std::ranges::cartesian_product_view{
                std::move(previous_view),
                detail::gather_components<Components...>(manager)
            }
                | std::views::transform([](auto&& pairs)
                    {
                        return detail::make_flat_tuple(
                            std::get<0>(std::forward<decltype(pairs)>(pairs)),
                            std::get<1>(std::forward<decltype(pairs)>(pairs)));
                    });
        }

        component_manager& m_manager;
        using view_type = decltype(make_view(
            std::declval<component_manager&>(), std::declval<View>()
        ));
        view_type m_view;
    };

    template <component... Components>
    struct query_components : public joinable_query_mixin
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

        template <component... JoinComponents>
        auto join(this auto&& self)
        {
            return self.template join_mixin<view_type, JoinComponents...>(
                std::forward<decltype(self)>(self).m_manager,
                std::forward<decltype(self)>(self).m_view
            );
        }

    private:
        static auto make_view(component_manager& manager)
        {
            return detail::gather_components<Components...>(manager);
        }

        component_manager& m_manager;
        using view_type = decltype(make_view(std::declval<component_manager&>()));
        view_type m_view;
    };

    template <std::ranges::view ViewT, component... JoinComponents>
    auto joinable_query_mixin::join_mixin(component_manager& manager, ViewT view)
        -> query_join<ViewT, JoinComponents...>
    {
        return query_join<ViewT, JoinComponents...>{manager, std::move(view)};
    }
}
