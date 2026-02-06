#pragma once

#include "mope_game_engine/components/component.hxx"
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
    struct I_logger;
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

        /// Same as `get_component<I_logger>()`.
        auto logger() -> I_logger*;

        /// Used by the @ref game_engine to move the scene forward by one time step.
        void tick(double time_step, input_state const& inputs);

        /// Used by the @ref game_engine to tell the scene when it is time to render.
        void render(double alpha);

        /// Add a game system that is invoked when certain events occur.
        ///
        /// `f` shall be a callable function or object with the signature:
        /// ```
        ///     R (game_scene&, Event const&)
        /// ```
        /// where `R` and `Event` are any type. `f` will be invoked for every
        /// event of type `Event` that occurs.
        template <proxyable_game_system F>
        void add_game_system(F&& f)
        {
            // Determine the type of event to which this system responds.
            using Event = std::remove_cvref_t<
                typename detail::parameters_of<std::decay_t<F>>::template type<1>
            >;
            add_game_system_imp(new game_system_proxy<Event, std::decay_t<F>>{ std::forward<F>(f) });
        }

        /// Add an instance of a game system derived from @ref game_system<T...>.
        ///
        /// There are two possible advantages of using this overload over the
        /// above:
        ///   - `System` isn't copied or moved during construction, and
        ///   - `System` can respond to any number of event types, by
        ///      overloading `operator()` for each one (and providing the event
        ///      types to the base @ref game_system). This is not possible using
        ///      the functor method.
        template <typename System>
        void add_game_system(std::unique_ptr<System>&& system)
        {
            // This ensures that the system type can be implicitly cast to
            // `game_system<Events...>`, i.e., it publicly derives from
            // `game_system<Events...>`. Using a concept (`std::derived_from<>`)
            // isn't possible here, as we as we don't know the event types yet.
            // And it is vitally important that `System` does in fact derive
            // from @ref virtual_event_handler for each event.
            add_game_system_imp(system.release());
        }

        /// Like the `std::unique_ptr` overload of `add_game_system()`, but we
        /// don't actually need the intermediate `std::unique_ptr<>`.
        template <typename System, typename... Args>
        void emplace_game_system(Args&&... args)
        {
            add_game_system_imp(new System(std::forward<Args>(args)...));
        }

        template <typename Event>
        void push_event(Event&& event)
        {
            m_event_pool.store<std::remove_cvref_t<Event>>(std::forward<Event>(event));
        }

        template <typename Event, typename... Args>
        void emplace_event(Args&&... args)
        {
            m_event_pool.store<Event>(std::forward<Args>(args)...);
        }

        template <component... Components>
        auto query()
        {
            return query_components<Components...>{*this};
        }

    private:
        auto ensure_renderer() -> sprite_renderer&;

        template <typename... Events>
        void add_game_system_imp(game_system<Events...>* system)
        {
            auto shared = std::shared_ptr<game_system<Events...>>{ system };

            (m_game_systems[typeid(Events)].emplace_back(
                // This is the ***aliasing constructor*** of std::shared_ptr<T>.
                // The `shared_ptr`s will dereference to the bases, which are
                // `virtual_event_handler<Event>`s, but will share ownership and
                // lifetime of the derived class, `game_system<Events...>`.
                shared, static_cast<virtual_event_handler<Events>*>(system)
            ), ...);
        }

        entity_id m_last_entity;
        std::unordered_map<std::type_index, std::vector<std::shared_ptr<void>>>
            m_game_systems;
        detail::event_pool m_event_pool;
        std::unique_ptr<sprite_renderer> m_sprite_renderer;
        bool m_done;

        template <typename Event>
        friend void detail::event_pool::process_event(game_scene& scene, void* ptr);
    };
}

#include "mope_game_engine/event_pool.inl"
