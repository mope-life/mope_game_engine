#pragma once

#include "mope_game_engine/components/component.hxx"
#include "mope_game_engine/component_manager.hxx"

#include <algorithm>
#include <concepts>
#include <functional>
#include <optional>
#include <ranges>
#include <tuple>
#include <type_traits>
#include <utility>

namespace mope
{
    template <typename, typename...>
    struct related;

    template <typename T>
    concept entity_queryable =
        detail::specialization<T, related> || derived_from_entity_component<T>;

    /// A query for a relationship on an entity, optionally including additional
    /// components that the related entity should possess.
    template<
        std::derived_from<relationship> Relationship,
        entity_queryable... Queryables>
    struct related<Relationship, Queryables...> {};

    /// A query for a group of components belonging to a single entity.
    template <
        entity_queryable Queryable,
        entity_queryable... Queryables>
    struct entity_has {};

    /// A query for one or more singleton components.
    template <
        derived_from_singleton_component Component,
        derived_from_singleton_component... Components>
    struct singletons {};

    template <typename T>
    concept queryable =
        detail::specialization<T, entity_has>
        || detail::specialization<T, singletons>;

    template <queryable... Queryables>
    struct query;
}

namespace mope::detail
{
    template <typename T>
    struct tuplify
    {
        static auto operator()(T& t)
        {
            return std::forward_as_tuple(t);
        }

        static auto operator()(T&& t)
        {
            return std::make_tuple(t);
        }
    };

    template <typename... T>
    struct tuplify<std::tuple<T...>>
    {
        template <typename U>
            requires std::same_as<std::tuple<T...>, std::remove_cvref_t<U>>
        static decltype(auto) operator()(U&& u)
        {
            return std::forward<U>(u);
        }
    };

    auto make_flat_tuple(auto&&... elements)
    {
        return std::tuple_cat(
            tuplify<std::remove_cvref_t<decltype(elements)>>{}(std::forward<decltype(elements)>(elements))...
        );
    }

    template <typename T>
        requires (detail::specialization<std::remove_cvref_t<T>, std::tuple>
            || detail::specialization<std::remove_cvref_t<T>, std::pair>)
    auto make_flat_tuple(T&& tup)
    {
        return std::apply([](auto&&... elements)
            {
                return make_flat_tuple(std::forward<decltype(elements)>(elements)...);
            }, std::forward<T>(tup));
    }

    template <typename T>
    auto is_not_null(T const& t) -> bool
    {
        if constexpr (std::is_null_pointer_v<T>) {
            return false;
        }
        else if constexpr (std::is_pointer_v<T>) {
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
            return std::ref(*t);
        }
        else {
            return std::forward<T>(t);
        }
    }

    template <entity_queryable... Queryables>
    auto get_queryables_for_entity(component_manager& manager, entity_id entity);

    template <entity_queryable Queryable>
    struct resolve_entity_queryable;

    template <derived_from_entity_component Component>
    struct resolve_entity_queryable<Component>
    {
        static auto one(component_manager& manager, entity_id entity)
        {
            return manager.get_component<Component>(entity);
        }

        static auto all(component_manager& manager)
        {
            return manager.get_components<Component>();
        }
    };

    template <
        std::derived_from<relationship> Relationship,
        entity_queryable... Queryables>
    struct resolve_entity_queryable<related<Relationship, Queryables...>>
    {
        static auto impl(component_manager& manager, auto&& relationship_view)
        {
            if constexpr (0 == sizeof...(Queryables)) {
                return std::forward<decltype(relationship_view)>(relationship_view);
            }
            else {
                return std::forward<decltype(relationship_view)>(relationship_view)
                    | std::views::transform([&manager](auto& rel_component)
                        {
                            return get_queryables_for_entity<Queryables...>(manager, rel_component.related_entity)
                                .transform([&rel_component](auto&& queryables)
                                    {
                                        return make_flat_tuple(
                                            rel_component,
                                            std::forward<decltype(queryables)>(queryables)
                                        );
                                    });
                        })
                    | std::views::filter([](auto const& opt)
                        {
                            return opt.has_value();
                        })
                    | std::views::transform([](auto&& opt)
                        {
                            return *std::forward<decltype(opt)>(opt);
                        });
            }
        }

        static auto one(component_manager& manager, entity_id entity)
        {
            return impl(manager, manager.get_component<Relationship>(entity));
        }

        static auto all(component_manager& manager)
        {
            return impl(manager, manager.get_components<Relationship>());
        }
    };

    // Return all of the given components if owned by the given entity, or nullopt.
    template <entity_queryable... Queryables>
    auto get_queryables_for_entity(component_manager& manager, entity_id entity)
    {
        auto results = std::make_tuple(
            resolve_entity_queryable<Queryables>{}.one(manager, entity)...
        );

        return std::apply([](auto const&... elements) { return (is_not_null(elements) && ...); }, results)
            ? std::make_optional(std::apply(
                [](auto&&... elements)
                {
                    return std::make_tuple(deref(elements)...);
                }, std::move(results)))
            : std::nullopt;
    }

    template <
        entity_queryable Queryable,
        entity_queryable... Queryables>
    auto get_entity_queryables(component_manager& manager)
    {
        if constexpr (0 == sizeof...(Queryables)) {
            return resolve_entity_queryable<Queryable>{}.all(manager);
        }
        else {
            return resolve_entity_queryable<Queryable>{}.all(manager)
                | std::views::transform([&manager](auto&& resolved)
                    {
                        if constexpr (specialization<Queryable, related>) {
                            return get_queryables_for_entity<Queryables...>(manager, std::get<0>(resolved).entity)
                                .transform([resolved = std::move(resolved)](auto&& queryables)
                                    {
                                        return make_flat_tuple(
                                            std::move(resolved),
                                            std::forward<decltype(queryables)>(queryables)
                                        );
                                    });
                        }
                        else if constexpr (derived_from_entity_component<Queryable>) {
                            return get_queryables_for_entity<Queryables...>(manager, resolved.entity)
                                .transform([&resolved](auto&& queryables)
                                    {
                                        return make_flat_tuple(
                                            std::forward<decltype(resolved)>(resolved),
                                            std::forward<decltype(queryables)>(queryables)
                                        );
                                    });
                        }
                    })
                | std::views::filter([](auto const& opt)
                    {
                        return opt.has_value();
                    })
                | std::views::transform([](auto&& opt)
                    {
                        return *std::forward<decltype(opt)>(opt);
                    });
        }
    }

    template <queryable>
    struct queryable_view_wrapper;

    template <
        entity_queryable Queryable,
        entity_queryable... Queryables>
    struct queryable_view_wrapper<entity_has<Queryable, Queryables...>>
    {
        queryable_view_wrapper(component_manager& manager)
            : view{ get_entity_queryables<Queryable, Queryables...>(manager) }
        {
        }

        decltype(get_entity_queryables<Queryable, Queryables...>(std::declval<component_manager&>()))
            view;
    };

    template <
        derived_from_singleton_component Component,
        derived_from_singleton_component... Components>
    struct queryable_view_wrapper<singletons<Component, Components...>>
    {
        /// TODO:
        /*
        queryable_view_wrapper(component_manager& manager)
            : view{ }
        {
        }

        decltype()
            view;
        */
    };
}

namespace mope
{
    template <queryable... Queryables>
    struct query
    {
        query(component_manager& manager)
            : m_manager{ manager }
        { }

        /// Cross this query (by way of cartesian product) with an unrelated
        /// outside query.
        template <entity_queryable... CrossQueryables>
        auto cross() const
        {
            return query<Queryables..., entity_has<CrossQueryables...>>{ m_manager };
        }

        auto exec() const
        {
            static_assert(
                sizeof...(Queryables) > 0,
                "Can't execute a query with no queryables."
            );

            if constexpr (1 == sizeof...(Queryables)) {
                using Q0 = std::tuple_element_t<0, std::tuple<Queryables...>>;
                return detail::queryable_view_wrapper<Q0>{ m_manager }.view;
            }
            else {
                return std::ranges::cartesian_product_view{
                    detail::queryable_view_wrapper<Queryables>{ m_manager }.view...
                }
                    | std::views::transform([](auto&& tup)
                        {
                            return detail::make_flat_tuple(std::forward<decltype(tup)>(tup));
                        });
            }
        }

    private:
        component_manager& m_manager;
    };

    template <entity_queryable... Queryables>
    struct query_entity
    {
        query_entity(component_manager& manager, entity_id entity)
            : m_manager{ manager }
            , m_entity{ entity }
        {
        }

        auto exec() const
        {
            if constexpr (1 == sizeof...(Queryables)) {
                using Q0 = std::tuple_element_t<0, std::tuple<Queryables...>>;
                return detail::resolve_entity_queryable<Q0>{ }.one(m_manager, m_entity);
            }
            else {
                return detail::get_queryables_for_entity<Queryables...>(m_manager, m_entity);
            }
        }

    private:
        component_manager& m_manager;
        entity_id m_entity;
    };
}
