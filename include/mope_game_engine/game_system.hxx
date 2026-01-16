#pragma once

#include "mope_game_engine/component.hxx"
#include "mope_game_engine/ecs_manager.hxx"
#include "mope_game_engine/iterable_box.hxx"
#include "mope_game_engine/mope_game_engine_export.hxx"

#include <concepts>
#include <span>
#include <ranges>
#include <utility>

namespace mope::detail
{
    template <component Component, component C0>
    auto get_single_component_helper(ecs_manager& ecs, C0 const& c0) -> Component*
    {
        if constexpr (derived_from_singleton_component<Component>) {
            return ecs.get_component<Component>();
        }
        else {
            static_assert(
                derived_from_entity_component<C0>,
                "Entity components may not be used in a system where the primary component is a singleton. "
                "Reorder your components such that an entity component is the first parameter."
            );

            return ecs.get_component<Component>(c0.en);
        }
    }

    template <component PrimaryComponent, component_or_relationship... AdditionalComponents>
    auto get_all_components_helper(ecs_manager& ecs);

    template <component_or_relationship ComponentOrRelationship>
    struct additional_component_view;

    template <component Component>
    struct additional_component_view<Component>
    {
        template <component C0>
        static auto get(ecs_manager& ecs, C0 const& c0)
        {
            return iterable_box{ get_single_component_helper<Component>(ecs, c0) };
        }
    };

    template <
        derived_from_entity_component PrimaryComponent,
        derived_from_entity_component... AdditionalComponents>
    struct additional_component_view<relationship<PrimaryComponent, AdditionalComponents...>>
    {
        template <component Ignored>
        static auto get(ecs_manager& ecs, Ignored const&)
        {
            // It's weird, but we're wrapping a view in a view. This is what
            // allows the sub-views from relationships to surface as views in
            // the output tuple: otherwise, the cartesian_product_view would
            // expand this view out.
            // But see the HACK below, this ultimately shouldn't be needed.
            return std::ranges::single_view{
                get_all_components_helper<PrimaryComponent, AdditionalComponents...>(ecs)
            };
        }
    };

    template <component PrimaryComponent, component_or_relationship... AdditionalComponents>
    auto get_all_components_helper(ecs_manager& ecs)
    {
        if constexpr (0 == sizeof...(AdditionalComponents)) {
            return ecs.get_components<PrimaryComponent>();
        }
        else {
            // We need to handle the "primary" component seperately because we
            // need the entity id from that component in order to capture the
            // remaining entity components.
            return ecs.get_components<PrimaryComponent>()
                | std::views::transform([&ecs](auto&& c0)
                    {
                        // HACK alert: This is essentially a hacky way to check
                        // every @ref iterable_box for a component, filtering out
                        // those entities (which we grabbed from `c0`) that result
                        // in one or more @ref iterable_box's to be empty.
                        // I consider this a hack because we really only need a
                        // filter (to filter out `nullptr`s) followed by a
                        // transform (to dereference the pointers afterward).
                        // I couldn't get it to work, so leaving this for now.
                        return std::ranges::cartesian_product_view{
                            std::span<PrimaryComponent, 1>{&c0, 1},
                            additional_component_view<AdditionalComponents>::get(ecs, c0)...
                        };
                    })
                | std::views::join;
        }
    }

    template <component PrimaryComponent, component_or_relationship... AdditionalComponents>
    using component_view = decltype(
        get_all_components_helper<PrimaryComponent, AdditionalComponents...>(
            std::declval<ecs_manager&>()));
} // namespace mope::detail

namespace mope
{
    class MOPE_GAME_ENGINE_EXPORT game_system_base
    {
    public:
        virtual ~game_system_base() = default;

        virtual void tick(ecs_manager& manager_map, double time_step) = 0;
    };

    template <component PrimaryComponent, component_or_relationship... AdditionalComponents>
    class game_system : public game_system_base
    {
    public:
        using component_view = detail::component_view<PrimaryComponent, AdditionalComponents...>;

    private:
        virtual void process_tick(double time_step, component_view components) = 0;

        void tick(ecs_manager& ecs, double time_step) override
        {
            process_tick(
                time_step,
                detail::get_all_components_helper<PrimaryComponent, AdditionalComponents...>(ecs)
            );
        }
    };
}
