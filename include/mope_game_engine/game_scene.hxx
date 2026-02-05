#pragma once

#include "mope_game_engine/component.hxx"
#include "mope_game_engine/component_manager.hxx"
#include "mope_game_engine/event_pool.hxx"
#include "mope_game_engine/game_system.hxx"
#include "mope_game_engine/query.hxx"
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
    class sprite_renderer;
    struct input_state;
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
    class game_scene : public component_manager
    {
    public:
        // Customization points:

        /// Called when the @ref game_engine first sees your scene.
        ///
        /// Use this to add initial components / systems to the scene. By the
        /// time we get here, the graphics context is ready to use, and all
        /// engine-provided singleton components are available.
        virtual void on_load(game_engine&) { }

        /// Called after your scene returns `true` from `is_done()`, just before
        /// it is deleted.
        ///
        /// TODO: This doesn't really do anything at the moment, but is a likely
        /// place to allow scenes to perform serialization in the future.
        virtual void on_unload(game_engine&) { }

        /// Called when the @ref game_window has reported that it is ready to
        /// close.
        ///
        /// Any scene may return false from this method to prevent the window
        /// from closing, for instance to ask the user if they would like to
        /// save before closing.
        virtual bool on_close() { return true; }

        game_scene();
        virtual ~game_scene() = 0;

        game_scene(game_scene const&) = delete;
        auto operator=(game_scene const&) -> game_scene & = delete;

        void set_done(bool done = true);
        auto is_done() const -> bool;

        void set_projection_matrix(mat4f const& projection);

        auto create_entity() -> entity_id;
        void destroy_entity(entity_id entity);

        /// Used by the @ref game_engine to move the scene forward by one time step.
        void tick(double time_step, input_state const& inputs);

        /// Used by the @ref game_engine to tell the scene when it is time to render.
        void render(double alpha);

        /// Add an instance of an game system derived from @ref game_system<T>.
        ///
        /// Class-based game systems may be used when your system has need to
        /// hold on to some particular data or resource, and you would like to
        /// keep that within the class. Simply derive your class from
        /// game_system<Event> and add an override for
        /// `operator()(game_system&, Event const&)`.
        ///
        /// Note that this should be less common than the plain old invocable
        /// overload (below), since system state is usually better accessed
        /// through components.
        template <typename System>
            requires std::derived_from<System, game_system<typename System::event_type>>
        void add_game_system(std::unique_ptr<System> system)
        {
            m_game_systems[typeid(typename System::event_type)].push_back(std::move(system));
        }

        /// Add an invocable game system that is called whenever Event occurs.
        template <typename Event, std::invocable<game_scene&, Event const&> F>
        void add_game_system(F&& f)
        {
            add_game_system(
                std::make_unique<game_system_proxy<Event, std::decay_t<F>>>(
                    std::forward<F>(f)
                ));
        }

        template <typename Event, typename... Args>
        void emplace_event(Args&&... args)
        {
            m_event_pool.store<Event>(std::forward<Args>(args)...);
        }

        template <typename Event>
        void push_event(Event&& event)
        {
            m_event_pool.store<std::remove_cvref_t<Event>>(std::forward<Event>(event));
        }

        template <component... Components>
        auto query()
        {
            return query_components<Components...>{*this};
        }

    private:
        auto ensure_renderer() -> sprite_renderer&;

        entity_id m_last_entity;
        std::unordered_map<std::type_index, std::vector<std::unique_ptr<detail::game_system_base>>>
            m_game_systems;
        detail::event_pool m_event_pool;
        std::unique_ptr<sprite_renderer> m_sprite_renderer;
        bool m_done;

        template <typename Event>
        friend void detail::event_pool::process_event(game_scene& scene, void* ptr);
    };
}

#include "mope_game_engine/event_pool.inl"
