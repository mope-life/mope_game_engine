#pragma once

#include "mope_game_engine/components/component.hxx"
#include "mope_game_engine/iterable_box.hxx"

#include <memory>
#include <ranges>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#if defined(__GNUC__)
#define PRAGMA_GCC(x) _Pragma(#x)
#else
#define PRAGMA_GCC(x)
#endif

namespace mope::detail
{
    PRAGMA_GCC(GCC diagnostic push)
    PRAGMA_GCC(GCC diagnostic ignored "-Woverloaded-virtual")
    class component_storage_base
    {
    public:
        virtual ~component_storage_base() = default;

        /// Remove the component managed by this from the given entity.
        ///
        /// This method is virtual because we need to be able to call it without
        /// naming the component type.
        ///
        /// The base implementation is a no-op to support singleton components
        /// without requiring a dynamic_cast.
        ///
        /// @param entity The entity whose component to remove.
        virtual void remove(entity_id) {}
    };
    PRAGMA_GCC(GCC diagnostic pop)

    template <component Component>
    class component_storage;

    template <derived_from_singleton_component Component>
    class component_storage<Component> final : public component_storage_base
    {
    public:
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

        void remove()
        {
            m_data = std::monostate{};
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
        using Variant = std::conditional_t<
            std::is_abstract_v<Component>,
            std::variant<std::monostate, Component*>,
            std::variant<std::monostate, Component, Component*>>;

        Variant m_data;
    };

    template <derived_from_entity_component Component>
    class component_storage<Component> final : public component_storage_base
    {
    public:
        template <typename T>
            requires std::same_as<std::remove_cvref_t<T>, Component>
        auto add_or_set(T&& t) -> Component*
        {
            entity_id entity = t.entity;
            if (auto iter = m_index_map.find(entity); m_index_map.end() != iter) {
                m_data[iter->second] = std::forward<T>(t);
                return &m_data[iter->second];
            }
            else {
                m_data.push_back(std::forward<T>(t));
                m_index_map.insert({ entity, m_data.size() - 1 });
                return &m_data.back();
            }
        }

        void remove(entity_id entity) override
        {
            if (auto iter = m_index_map.find(entity); m_index_map.end() != iter) {
                using std::swap;
                swap(m_data[iter->second], m_data.back());
                m_index_map[m_data.back().entity] = iter->second;
                m_data.pop_back();
                m_index_map.erase(iter);
            }
        }

        auto get(entity_id entity) -> Component*
        {
            if (auto iter = m_index_map.find(entity); m_index_map.end() != iter) {
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
        std::unordered_map<entity_id, std::size_t> m_index_map;
    };
}

namespace mope
{
    class component_manager
    {
    public:
        template <
            typename ComponentRef,
            component Component = std::remove_cvref_t<ComponentRef>
        >
        auto set_component(ComponentRef&& c) -> Component*
        {
            return ensure_storage<Component>()
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
            return ensure_storage<Component>().add_or_set(component);
        }

        template <derived_from_singleton_component Component>
        auto get_component() -> Component*
        {
            return ensure_storage<Component>().get();
        }

        template <derived_from_entity_component Component>
        auto get_component(entity_id entity) -> Component*
        {
            return ensure_storage<Component>().get(entity);
        }

        template <component Component>
        auto get_components()
        {
            return ensure_storage<Component>().all();
        }

        template <derived_from_entity_component Component>
        void remove_component(entity_id entity)
        {
            ensure_storage<Component>().remove(entity);
        }

        template <derived_from_singleton_component Component>
        void remove_component()
        {
            ensure_storage<Component>().remove();
        }

    private:
        template <component Component>
        auto ensure_storage() -> detail::component_storage<Component>&
        {
            auto type_idx = std::type_index{ typeid(Component) };
            auto iter = m_component_stores.find(type_idx);
            if (m_component_stores.end() == iter) {
                iter = m_component_stores.insert(
                    { type_idx, std::make_unique<detail::component_storage<Component>>() }
                ).first;
            }
            // Other code may leave empty unique_ptrs in the map by using the
            // subscript operator, so we want to check for both missing AND
            // nullptr.
            else if (!iter->second) {
                iter->second = std::make_unique<detail::component_storage<Component>>();
            }
            return static_cast<detail::component_storage<Component>&>(*iter->second);
        }

    protected:
        std::unordered_map<std::type_index, std::unique_ptr<detail::component_storage_base>>
            m_component_stores;
    };
}

#undef PRAGMA_GCC
