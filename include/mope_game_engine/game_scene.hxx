#pragma once

#include "mope_game_engine/component.hxx"
#include "mope_game_engine/mope_game_engine_export.hxx"
#include "mope_vec/mope_vec.hxx"

#include <bitset>
#include <concepts>
#include <memory>
#include <ranges>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace mope
{
    class game_engine;
    class game_system_base;
    class sprite_renderer;
}

namespace mope
{
    /// A scene in a game.
    ///
    /// The `game_scene` is the main entry-point into the mope_game_engine. The
    /// scene should be overriden and passed off to be driven by the
    /// `game_engine`.
    ///
    /// The scene also acts as the top-level ECS manager. Entites are doled out
    /// by the scene, components added to the scene with reference to those
    /// entities, and systems are added to the scene that act on the components.
    class MOPE_GAME_ENGINE_EXPORT game_scene
    {
        // Customization points:
        virtual void on_initial_tick(game_engine& engine);

    public:
        game_scene();
        virtual ~game_scene() = 0;

        game_scene(game_scene&&);
        auto operator=(game_scene&&) -> game_scene&;
        game_scene(game_scene const&) = delete;
        auto operator=(game_scene const&) -> game_scene & = delete;

        void set_done(bool done = true);
        auto is_done() const -> bool;

        void set_projection_matrix(mat4f const& projection);

        auto create_entity() -> entity;
        void destroy_entity(entity e);

        template <
            typename ComponentRef,
            component Component = std::remove_cvref_t<ComponentRef>>
            auto set_component(ComponentRef&& c) -> Component*
        {
            return ensure_component_manager<Component>()
                .add_or_set(std::forward<ComponentRef>(c));
        }

        template <typename... ComponentRef>
        auto set_components(ComponentRef&&... cs)
        {
            (set_component(std::forward<ComponentRef>(cs)), ...);
        }

        /// Add a singleton component that has a lifetime managed separately
        /// from the @ref game_scene.
        template <derived_from_singleton_component Component>
        auto set_external_component(Component* component) -> Component*
        {
            return ensure_component_manager<Component>().add_or_set(component);
        }

        template <derived_from_singleton_component Component>
        auto get_component() -> Component*
        {
            return ensure_component_manager<Component>().get();
        }

        template <derived_from_entity_component Component>
        auto get_component(entity en) -> Component*
        {
            return ensure_component_manager<Component>().get(en);
        }

        template <component Component>
        auto get_components()
        {
            return ensure_component_manager<Component>().all();
        }

        void add_game_system(std::unique_ptr<game_system_base> system);

        template <typename System, typename... Args>
        void emplace_game_system(Args&&... args)
        {
            add_game_system(std::make_unique<System>(std::forward<Args>(args)...));
        }

    private:
        template <component Component>
        auto ensure_component_manager() -> detail::component_manager<Component>&
        {
            auto type_idx = std::type_index{ typeid(Component) };
            auto iter = m_component_managers.find(type_idx);
            if (m_component_managers.end() == iter) {
                iter = m_component_managers.insert(
                    { type_idx, std::make_unique<detail::component_manager<Component>>() }
                ).first;
            }
            // Other code may leave empty unique_ptrs in the map by using the
            // subscript operator, so we want to check for both missing AND
            // nullptr.
            else if (!iter->second) {
                iter->second = std::make_unique<detail::component_manager<Component>>();
            }
            return static_cast<detail::component_manager<Component>&>(*iter->second);
        }
        auto ensure_renderer() -> sprite_renderer&;

        friend class mope::game_engine;
        /// Used by the `game_engine` to move the scene forward by one time step.
        void tick(game_engine& engine, double time_step);
        /// Used by the `game_engine` to tell the scene when it is time to render.
        void render(double alpha);

        entity m_next_entity;
        std::unordered_map<std::type_index, std::unique_ptr<detail::component_manager_base>>
            m_component_managers;
        std::vector<std::unique_ptr<game_system_base>> m_game_systems;

        std::unique_ptr<sprite_renderer> m_sprite_renderer;
        bool m_initialized;
        bool m_done;
    };
}
