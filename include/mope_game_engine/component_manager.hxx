#pragma once

#include "mope_game_engine/components/component.hxx"
#include "mope_game_engine/iterable_box.hxx"

#include <memory>
#include <ranges>
#include <typeindex>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
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
        /// @param entity The entity whose component to remove.
        virtual void remove(entity_id) = 0;
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

        /// The entity_id parameter is ignored for singleton components.
        void remove(entity_id) override
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
        requires (!std::derived_from<Component, relationship>)
    class component_storage<Component> final : public component_storage_base
    {
    public:
        template <typename T>
            requires std::same_as<std::remove_cvref_t<T>, Component>
        auto add_or_set(T&& t) -> Component*
        {
            auto entity = t.entity;
            if (auto iter = m_index_map.find(entity); m_index_map.end() != iter) {
                m_data[iter->second] = std::forward<T>(t);
                return &m_data[iter->second];
            }
            else {
                m_data.push_back(std::forward<T>(t));
                m_index_map.emplace(entity, m_data.size() - 1);
                return &m_data.back();
            }
        }

        void remove(entity_id entity) override
        {
            if (auto iter = m_index_map.find(entity); m_index_map.end() != iter) {
                auto index = iter->second;

                // Swap the component we want to remove with the last component.
                using std::swap;
                swap(m_data[index], m_data.back());

                // Repoint the index map entry for the component we just swapped
                // to its new location (the former location of the component
                // that we are removing).
                // Note that we don't have to worry about an insert and rehash,
                // because we already know that this key (entity) is in the map
                // by virtue of the fact that this component is here.
                m_index_map[m_data[index].entity] = index;

                // Remove the component that is now at the end of the vector.
                m_data.pop_back();

                // Erase the index map entry for the component that we just
                // removed.
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
            return std::ranges::ref_view{ m_data };
        }

    private:
        std::vector<Component> m_data;
        std::unordered_map<entity_id, std::size_t> m_index_map;
    };

    template <std::derived_from<relationship> Relationship>
    class component_storage<Relationship> final : public component_storage_base
    {
    public:
        template <typename T>
            requires std::same_as<std::remove_cvref_t<T>, Relationship>
        auto add_or_set(T&& t) -> Relationship*
        {
            auto entity = t.entity;
            auto related_entity = t.related_entity;

            // It's okay if this makes an empty map; we're about to put
            // something there anyway.
            auto& inner_map = m_index_map[entity];

            if (auto iter = inner_map.find(related_entity); inner_map.end() != iter) {
                m_data[iter->second] = std::forward<T>(t);
                return &m_data[iter->second];
            }
            else {
                m_data.push_back(std::forward<T>(t));
                inner_map.emplace(related_entity, m_data.size() - 1);
                return &m_data.back();
            }
        }

        void remove(entity_id entity) override
        {
            if (auto&& iter = m_index_map.find(entity); m_index_map.end() != iter) {
                auto data_end = m_data.end();

                for (auto&& [related_entity, index] : iter->second) {
                    // Swap the component we want to remove with the last
                    // component.
                    using std::swap;
                    swap(m_data[index], *--data_end);

                    // Repoint the index map entry for the component we just
                    // swapped to its new location (the former location of the
                    // component that we are removing).
                    // Note that we don't have to worry about an insert and
                    // rehash, because we already know that these keys
                    // (entities) are in the maps by virtue of the fact that
                    // this component is here.
                    m_index_map[m_data[index].entity][m_data[index].related_entity] = index;
                }

                // Erase all the components that we just swapped to the back.
                m_data.erase(data_end, m_data.end());

                // Finally, we can clear all the indices for this entity. (We
                // tend not to want to deallocate a map that we already have,
                // since its probable that components will be added to this
                // entity again.)
                iter->second.clear();
            }
        }

        auto get(entity_id entity)
        {
            // We are doing in this in such a way to avoid adding another inner
            // map for every entity we query... At that point we may as well
            // index the vector directly.
            return std::ranges::single_view{ entity }
                | std::views::transform([this](auto&& entity) { return m_index_map.find(entity); })
                | std::views::filter([this](auto&& iter) { return m_index_map.end() != iter; })
                | std::views::transform([](auto&& iter) { return std::ranges::subrange{ iter->second }; })
                | std::views::join
                | std::views::transform([this](auto&& kvp) -> decltype(auto) { return m_data[kvp.second]; });
        }

        auto all()
        {
            return std::ranges::ref_view{ m_data };
        }

    private:
        std::vector<Relationship> m_data;
        std::unordered_map<entity_id, std::unordered_map<entity_id, std::size_t>>
            m_index_map;
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
        auto get_component()
        {
            return ensure_storage<Component>().get();
        }

        // TODO: do we want to use this for relationships? they have very different semantics
        template <derived_from_entity_component Component>
        auto get_component(entity_id entity)
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
            ensure_storage<Component>().remove(NoEntity);
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
