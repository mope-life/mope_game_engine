#pragma once

#include "mope_game_engine/component.hxx"
#include "mope_game_engine/game_engine.hxx"
#include "mope_game_engine/game_scene.hxx"

#include <concepts>
#include <optional>
#include <ranges>
#include <tuple>
#include <utility>
#include <type_traits>

namespace mope::detail
{
    template <typename T>
    struct component_resolver;

    template <typename T>
    struct component_resolver_tuple
    {
    };

    template <typename... Ts>
    struct component_resolver_tuple<std::tuple<component_resolver<Ts>...>>
        : std::type_identity<std::tuple<component_resolver<Ts>...>>
    {
    };

    template <
        std::ranges::viewable_range R,
        typename Tuple = component_resolver_tuple<std::ranges::range_value_t<R>>::type>
    auto cull_missing_components(R&& rng)
    {
        return rng
            | std::views::filter([](auto const& tup)
                {
                    return std::apply([](auto const&... m)
                        {
                            return (m.okay() && ...);
                        },
                        tup);
                })
            | std::views::transform([](auto&& tup) -> decltype(auto)
                {
                    auto result = std::apply([](auto&&... ms)
                        {
                            return std::make_tuple(std::forward<decltype(ms)>(ms).extract()...);
                        },
                        std::forward<decltype(tup)>(tup));

                    if constexpr (1 == std::tuple_size_v<Tuple>) {
                        return std::get<0>(std::move(result));
                    }
                    else {
                        return result;
                    }
                });
    }

    template <component PrimaryComponent, component_or_subsystem... AdditionalComponents>
    auto gather_components(game_scene& scene)
    {
        return cull_missing_components(
            scene.get_components<PrimaryComponent>()
            | std::views::transform([&scene](auto&& c0)
                {
                    if constexpr (derived_from_entity_component<PrimaryComponent>) {
                        return std::make_tuple(
                            component_resolver<PrimaryComponent&>{ c0 },
                            component_resolver<AdditionalComponents>{ scene, c0.entity }...
                        );
                    }
                    else {
                        static_assert(!(derived_from_entity_component<AdditionalComponents> || ...),
                            "Entity components may not be used in a system where the primary component is a singleton. "
                            "Reorder your components such that an entity component is the first parameter.");

                        return std::make_tuple(
                            component_resolver<PrimaryComponent&>{ c0 },
                            component_resolver<AdditionalComponents>{ scene, NoEntity }...
                        );
                    }
                }));
    }

    template <derived_from_entity_component... Components>
    auto gather_related_components(game_scene& scene, entity_id entity)
    {
        using Relationship = relationship<Components...>;

        // TODO: Since there can be more than one of this type of component attached to a single entity,
        // This actually needs to return a view.
        auto c = scene.get_component<Relationship>(entity);

        auto opt = std::optional<std::span<Relationship>>{};

        if (nullptr != c) {
            // TODO: We're just faking the view for now. In reality this span will probably be a ref_view
            // over a vector (of relationship components with a common primary entity).
            opt = std::span{ c, 1 };
        }

        return opt.transform([&scene](auto&& sp)
            {
                return cull_missing_components(
                    sp
                    | std::views::transform([&scene](auto&& c)
                        {
                            return std::make_tuple(component_resolver<Components>{scene, c.related_entity}...);
                        })
                );
            }
        );
    }

    template <derived_from_singleton_component Component>
    struct component_resolver<Component>
    {
        Component* c;

        component_resolver(game_scene& scene, entity_id)
            : c{ scene.get_component<Component>() }
        {
        }

        auto okay() const -> bool
        {
            return nullptr != c;
        }

        auto extract()
        {
            return std::ref(*c);
        }
    };

    template <derived_from_entity_component Component>
    struct component_resolver<Component>
    {
        Component* c;

        component_resolver(game_scene& scene, entity_id entity)
            : c{ scene.get_component<Component>(entity) }
        {
        }

        auto okay() const -> bool
        {
            return nullptr != c;
        }

        auto extract()
        {
            return std::ref(*c);
        }
    };

    template <component Component>
    struct component_resolver<Component&>
    {
        Component& c;

        component_resolver(Component& c)
            : c{ c }
        {
        }

        auto okay() const -> bool
        {
            return true;
        }

        auto extract()
        {
            return std::ref(c);
        }
    };

    template <derived_from_entity_component... Components>
    struct component_resolver<subsystem<Components...>>
    {
        decltype(gather_components<Components...>(std::declval<game_scene&>())) v;

        component_resolver(game_scene& scene, entity_id)
            : v{ gather_components<Components...>(scene) }
        {
        }

        auto okay() const -> bool
        {
            return true;
        }

        auto extract() &&
        {
            return std::move(v);
        }
    };

    template <derived_from_entity_component... Components>
    struct component_resolver<relationship<Components...>>
    {
        decltype(gather_related_components<Components...>(std::declval<game_scene&>(), std::declval<entity_id>())) v;

        component_resolver(game_scene& scene, entity_id entity)
            : v{ gather_related_components<Components...>(scene, entity) }
        {
        }

        auto okay() const -> bool
        {
            return v.has_value();
        }

        auto extract() &&
        {
            return *std::move(v);
        }
    };
}

namespace mope
{
    class game_scene;

    class game_system_base
    {
    public:
        virtual ~game_system_base() = default;
        virtual void process_tick(game_scene& scene, double time_step) = 0;
    };

    template <component_or_subsystem... Components>
    class game_system;

    /// A game system that acts on entities requiring the provided set of components.
    template <component PrimaryComponent, component_or_subsystem... AdditionalComponents>
    class game_system<PrimaryComponent, AdditionalComponents...> : public game_system_base
    {
    public:
        static auto components(game_scene& scene)
        {
            return detail::gather_components<PrimaryComponent, AdditionalComponents...>(scene);
        }
    };

    /// A game system that does not need to query components.
    template <>
    class game_system<> : public game_system_base
    {
    };
}
