#pragma once

#include "mope_game_engine/component.hxx"
#include "mope_game_engine/iterable_box.hxx"
#include "mope_game_engine/mope_game_engine_export.hxx"

#include <concepts>
#include <memory>
#include <ranges>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace mope::detail
{
    class MOPE_GAME_ENGINE_EXPORT component_manager_base
    {
    public:
        virtual ~component_manager_base() = default;

        /// Remove the component managed by this from the given entity.
        ///
        /// This method is virtual because we need to be able to call it without
        /// naming the component type.
        ///
        /// The base implementation is a no-op to support singleton components
        /// without requiring a dynamic_cast.
        ///
        /// @param en The entity whose component to remove.
        virtual void remove(entity en) {}
    };

    template <component Component>
    class component_manager;

    template <derived_from_singleton_component Component>
    class component_manager<Component> final : public component_manager_base
    {
    public:
        component_manager()
            : m_data{ }
        {
        }

        component_manager(Component data)
            : m_data{ std::move(data) }
        {
        }

        component_manager(Component* external_data)
            : m_data{ external_data }
        {
        }

        template <typename T>
            requires std::same_as<std::remove_cvref_t<T>, Component>
        auto add_or_set(T&& t) -> Component*
        {
            m_data = std::forward<T>(t);
            return std::get_if<Component>(&m_data);
        }

        auto add_or_set(Component* external_data) -> Component*
        {
            m_data = external_data;
            return *std::get_if<Component*>(&m_data);
        }

        auto get() -> Component*
        {
            struct visitor
            {
                static auto operator()(std::monostate) -> Component*
                {
                    return nullptr;
                }

                static auto operator()(Component& data) -> Component*
                {
                    return &data;
                }

                static auto operator()(Component* data) -> Component*
                {
                    return data;
                }
            };

            return std::visit(visitor{}, m_data);
        }

        auto all()
        {
            return iterable_box{ get() };
        }

    private:
        std::variant<std::monostate, Component, Component*> m_data;
    };

    template <derived_from_entity_component Component>
    class component_manager<Component> final : public component_manager_base
    {
    public:
        template <typename T>
            requires std::same_as<std::remove_cvref_t<T>, Component>
        auto add_or_set(T&& t) -> Component*
        {
            entity en = t.en;
            if (auto iter = m_index_map.find(en); m_index_map.end() != iter) {
                m_data[iter->second] = std::forward<T>(t);
                return &m_data[iter->second];
            }
            else {
                m_data.push_back(std::forward<T>(t));
                m_index_map.insert({ en, m_data.size() - 1 });
                return &m_data.back();
            }
        }

        void remove(entity en) override
        {
            if (auto iter = m_index_map.find(en); m_index_map.end() != iter) {
                using std::swap;
                swap(m_data[iter->second], m_data.back());
                m_index_map[m_data.back().en] = iter->second;
                m_data.pop_back();
                m_index_map.erase(iter);
            }
        }

        auto get(entity en) -> Component*
        {
            if (auto iter = m_index_map.find(en); m_index_map.end() != iter) {
                return &m_data[iter->second];
            }
            else {
                return nullptr;
            }
        }

        auto all()
        {
            return m_data | std::views::all;
        }

    private:
        std::vector<Component> m_data;
        std::unordered_map<entity, std::size_t> m_index_map;
    };
}

namespace mope
{
    class game_system_base;

    class MOPE_GAME_ENGINE_EXPORT ecs_manager : public singleton_component
    {
    public:
        ecs_manager();
        virtual ~ecs_manager();
        ecs_manager(ecs_manager&&);
        auto operator=(ecs_manager&&) -> ecs_manager&;
        ecs_manager(ecs_manager const&) = delete;
        auto operator=(ecs_manager const&) -> ecs_manager & = delete;

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
        /// from the @ref ecs_manager.
        template <derived_from_singleton_component Component>
        auto set_external_component(Component* component) -> Component*
        {
            return ensure_component_manager<Component>().add_or_set(component);
        }

        template <derived_from_singleton_component Component>
        auto get_component() -> Component*
        {
            // If the requested singleton is a class derived from ecs_manager,
            // ensure_component_manager() will return an ecs_manager instead of
            // the derived class. Therefore, we have to cast the pointer
            // returned by component_manager::get() back to the derived class.
            return static_cast<Component*>(
                ensure_component_manager<Component>().get()
            );
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

    protected:
        void run_systems(double time_step);

    private:
        // Users are expected to derive their scenes from `game_scene`, which is
        // in turn derived from `ecs_manager`. We we will add the `ecs_manager`
        // component once; this facilitates returning that component as whatever
        // derived class the user requests.
        template <component Component>
        using ManagedComponent = std::conditional_t<std::derived_from<Component, ecs_manager>, ecs_manager, Component>;

        template <component Component>
        auto ensure_component_manager() -> detail::component_manager<ManagedComponent<Component>>&
        {
            using managed_component = ManagedComponent<Component>;

            auto type_idx = std::type_index{ typeid(managed_component) };
            auto iter = m_component_managers.find(type_idx);
            if (m_component_managers.end() == iter) {
                iter = m_component_managers.insert(
                    { type_idx, std::make_unique<detail::component_manager<managed_component>>() }
                ).first;
            }
            // Other code may leave empty unique_ptrs in the map by using the
            // subscript operator, so we want to check for both missing AND
            // nullptr.
            else if (!iter->second) {
                iter->second = std::make_unique<detail::component_manager<managed_component>>();
            }
            return static_cast<detail::component_manager<managed_component>&>(*iter->second);
        }

        entity m_next_entity;
        std::unordered_map<std::type_index, std::unique_ptr<detail::component_manager_base>>
            m_component_managers;
        std::vector<std::unique_ptr<game_system_base>> m_game_systems;
    };
}
