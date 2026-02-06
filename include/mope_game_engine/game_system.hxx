#pragma once

#include <concepts>
#include <functional>
#include <tuple>
#include <type_traits>
#include <utility>

namespace mope::detail
{
    template <typename T>
    struct is_const_lvalue_reference
    {
        static constexpr auto value =
            std::is_lvalue_reference_v<T>
            && std::is_const_v<std::remove_reference_t<T>>;
    };

    template <typename T>
    constexpr auto is_const_value_reference_v = is_const_lvalue_reference<T>::value;

    template <typename... Ts>
    struct indexable_types
    {
        template <size_t N>
        using type = std::tuple_element_t<N, std::tuple<Ts...>>;
    };

    template <typename T>
    struct parameters_of;

    template <typename R, typename... Args>
    struct parameters_of<R(*)(Args...)> : public indexable_types<Args...>
    {
    };

    template <typename C, typename R, typename... Args>
    struct parameters_of<R(C::*)(Args...)> : public indexable_types<Args...>
    {
    };

    template <typename C, typename R, typename... Args>
    struct parameters_of<R(C::*)(Args...) const> : public indexable_types<Args...>
    {
    };

    template <typename C, typename R, typename... Args>
    struct parameters_of<R(C::*)(Args...) volatile> : public indexable_types<Args...>
    {
    };

    // You know, for all the `const volatile` methods we see floating around
    // these days...
    template <typename C, typename R, typename... Args>
    struct parameters_of<R(C::*)(Args...) const volatile> : public indexable_types<Args...>
    {
    };

    template <typename Callable>
        requires requires { &Callable::operator(); }
    struct parameters_of<Callable> : public parameters_of<decltype(&Callable::operator())>
    {
    };
}

namespace mope
{
    class game_scene;

    template <typename Event>
    struct virtual_event_handler
    {
        virtual ~virtual_event_handler() = default;
        virtual void operator()(game_scene&, Event const&) = 0;
    };

    template <typename... Events>
    struct game_system : public virtual_event_handler<Events>...
    {
    };

    template <typename Event, std::invocable<game_scene&, Event const&> F>
    struct game_system_proxy : public game_system<Event>
    {
        template <typename G>
            requires std::same_as<F, std::decay_t<G>>
        game_system_proxy(G&& g)
            : f{ std::forward<G>(g) }
        {
        }

        void operator()(game_scene& scene, Event const& event) override
        {
            std::invoke(f, scene, event);
        }

    private:
        F f;
    };

    template <typename F>
    concept proxyable_game_system = requires
    {
        // Given a `game_scene& g` and any other object `p`, `F f` is invocable as `f(g, p)`.
        requires std::invocable<F, game_scene&, typename detail::parameters_of<std::decay_t<F>>::template type<1>>;
        // The second parameter to F must be a const reference.
        // (Please don't edit your events in the middle of being processed.)
        requires detail::is_const_value_reference_v<typename detail::parameters_of<std::decay_t<F>>::template type<1>>;
    };
}
