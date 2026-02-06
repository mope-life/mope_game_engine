#pragma once

#include <memory_resource>
#include <tuple>
#include <vector>

namespace mope
{
    class game_scene;
}

namespace mope::detail
{
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
}
