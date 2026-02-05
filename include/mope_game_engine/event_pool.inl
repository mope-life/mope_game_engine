// Included by game_scene.hxx

template <typename Event>
void mope::detail::event_pool::process_event(game_scene& scene, void* ptr)
{
    auto event = static_cast<Event*>(ptr);

    for (auto&& system_base_ptr : scene.m_game_systems[typeid(Event)]) {
        auto& system = *static_cast<virtual_event_handler<Event>*>(system_base_ptr.get());
        system(scene, *event);
    }

    if constexpr (!std::is_trivially_destructible_v<Event>) {
        event->~Event();
    }

    scene.m_event_pool.m_pool.deallocate(ptr, sizeof(Event), alignof(Event));
}
