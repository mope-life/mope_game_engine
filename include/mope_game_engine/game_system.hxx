#pragma once

#include <concepts>
#include <type_traits>
#include <utility>

namespace mope::detail
{
    struct game_system_base
    {
        virtual ~game_system_base() = default;
    };
}

namespace mope
{
    class game_scene;

    template <typename Event>
    struct game_system : public detail::game_system_base
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
        {
        }

        void operator()(game_scene& scene, Event const& event) override
        {
            f(scene, event);
        }

    private:
        F f;
    };
}
