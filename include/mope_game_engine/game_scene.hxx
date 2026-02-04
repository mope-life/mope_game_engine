#pragma once

#include "mope_game_engine/component.hxx"
#include "mope_vec/mope_vec.hxx"

#include <bitset>
#include <concepts>
#include <memory>
#include <memory_resource>
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
    class game_scene;

    class event_pool
    {
        std::vector<std::tuple<void*, void(*)(game_scene&, void*)>> m_events;
        std::pmr::unsynchronized_pool_resource m_pool;

    public:
        template <typename Event>
        static void process_event(game_scene& scene, void* ptr);

        template <typename Event, typename... Args>
        void store(Args&&... args)
        {
            auto ptr = m_pool.allocate(sizeof(Event), alignof(Event));
            auto event = new (ptr) Event(std::forward<Args>(args)...);
            m_events.emplace_back(static_cast<void*>(event), &process_event<Event>);
        }

        auto events() -> decltype(m_events) const&;
        void clear();
    };

    struct game_system_base
    {
        virtual ~game_system_base() = default;
    };

    template <typename Event>
    struct game_system : public game_system_base
    {
        using event_type = Event;

        virtual void operator()(game_scene&, Event const&) = 0;
    };

    template <typename Event, std::invocable<game_scene&, Event const&> F>
    struct game_system_proxy : public game_system<Event>
    {
        template <typename G>
            requires std::same_as<F, std::decay_t<G>>
        game_system_proxy(G&& g)
            : f{ std::forward<G>(g) }
        { }

        void operator()(game_scene& scene, Event const& event) override
        {
            f(scene, event);
        }

    private:
        F f;
    };
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
    class game_scene
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

        /// Called when the `game_window` has reported that it is ready to
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

        auto create_entity() -> entity;
        void destroy_entity(entity e);

        /// Used by the @ref game_engine to move the scene forward by one time step.
        void tick(double time_step, input_state const& inputs);

        /// Used by the @ref game_engine to tell the scene when it is time to render.
        void render(double alpha);

        template <
            typename ComponentRef,
            component Component = std::remove_cvref_t<ComponentRef>
        >
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

        template <derived_from_entity_component Component>
        void remove_component(entity en)
        {
            ensure_component_manager<Component>().remove(en);
        }

        template <derived_from_singleton_component Component>
        void remove_component()
        {
            ensure_component_manager<Component>().remove();
        }

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
            m_game_systems[typeid(System::event_type)].push_back(std::move(system));
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

        entity m_next_entity;
        std::unordered_map<std::type_index, std::unique_ptr<detail::component_manager_base>>
            m_component_managers;
        std::unordered_map<std::type_index, std::vector<std::unique_ptr<game_system_base>>>
            m_game_systems;
        event_pool m_event_pool;
        std::unique_ptr<sprite_renderer> m_sprite_renderer;
        bool m_done;

        template <typename Event>
        friend void event_pool::process_event(game_scene& scene, void* ptr);
    };

    template <typename Event>
    void event_pool::process_event(game_scene& scene, void* ptr)
    {
        auto event = static_cast<Event*>(ptr);

        for (auto&& system_base_ptr : scene.m_game_systems[typeid(Event)]) {
            auto& system = static_cast<game_system<Event>&>(*system_base_ptr);
            system(scene, *event);
        }

        if constexpr (!std::is_trivially_destructible_v<Event>) {
            event->~Event();
        }

        scene.m_event_pool.m_pool.deallocate(ptr, sizeof(Event), alignof(Event));
    }
}
