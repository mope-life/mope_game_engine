#pragma once

#include <concepts>
#include <cstdint>
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
    template <typename T, template <typename...> typename Template>
    struct is_specialization : public std::false_type
    {
    };

    template <template <typename...> typename Template, typename... Parameters>
    struct is_specialization<Template<Parameters...>, Template> : public std::true_type
    {
    };

    template <typename T, template <typename...> typename Template>
    concept specialization
        = is_specialization<std::remove_cvref_t<T>, Template>::value;
}

namespace mope
{
    using entity = uint64_t;

    /// A component attached to an entity.
    ///
    /// These components can be requested by a @ref game_system in the
    /// @ref game_system::process_tick() override.
    struct entity_component
    {
        entity_component(entity en)
            : en{ en }
        {
        }

        entity en;
    };

    /// A component not attached to any entity.
    ///
    /// A singleton component is meant to hold data accessible to any system,
    /// that doesn't make sense to attach to a particular entity. This can
    /// include per-tick data, such as mouse/keyboard input. It can be requested
    /// in the process_tick method of a user system just as with an
    /// @ref entity_component.
    struct singleton_component
    {
    };

    template <typename T>
    concept derived_from_singleton_component
        = std::derived_from<T, singleton_component>
        && !std::derived_from<T, entity_component>;

    template <typename T>
    concept derived_from_entity_component
        = std::derived_from<T, entity_component>
        && !std::derived_from<T, singleton_component>;

    /// Concept describing a component in the ECS architecture.
    ///
    /// Every component is either a singleton XOR an entity component.
    ///
    /// NOTE: Don't try to simplify this. MSVC accepts
    /// ```
    ///     std::derived_from<T, singleton_component> != std::derived_from<T, entity_component>
    /// ```
    /// but GCC does not. (Rather, GCC complains that `std::derived_from<...>` is
    /// not more constrained than the logical XOR.)
    template <typename T>
    concept component
        = derived_from_singleton_component<T> || derived_from_entity_component<T>;

    /// Generic representation of a relationship between components.
    ///
    /// This struct doesn't really mean anything on its own. It can be used as
    /// template arguments in a @ref game_system to query for a sub-view of
    /// components, not tied to the same entity as the primary
    /// @ref entity_component in the query.
    template <derived_from_entity_component... RelatedComponents>
    struct relationship final
    {
    };

    /// Concept describing what can be requested in @ref game_system queries.
    template <typename T>
    concept component_or_relationship =
        component<T>
        || detail::specialization<T, relationship>;
} // namespace mope

namespace mope::detail
{
    class component_manager_base
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
        { }

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
            if (auto ptr = get(); nullptr != ptr) {
                return std::span{ ptr, 1 };
            }
            else {
                return std::span<Component>{ };
            }
        }

    private:
        using Variant = std::conditional_t<
            std::is_abstract_v<Component>,
            std::variant<std::monostate, Component*>,
            std::variant<std::monostate, Component, Component*>>;

        Variant m_data;
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
