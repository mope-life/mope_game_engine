#include "mope_game_engine/game_scene.hxx"

#include "mope_game_engine/component.hxx"
#include "mope_game_engine/events/tick.hxx"
#include "mope_vec/mope_vec.hxx"

#include "shader.hxx"
#include "sprite_renderer.hxx"

#include <memory>
#include <utility>

mope::game_scene::game_scene()
    : m_next_entity{ 0 }
    , m_component_managers{ }
    , m_game_systems{ }
    , m_sprite_renderer{ }
    , m_done{ false }
{
}

mope::game_scene::~game_scene() = default;

void mope::game_scene::set_done(bool done)
{
    m_done = done;
}

auto mope::game_scene::is_done() const -> bool
{
    return m_done;
}

void mope::game_scene::set_projection_matrix(mat4f const& projection)
{
    ensure_renderer().m_shader.set_uniform("u_projection", projection);
}

auto mope::game_scene::create_entity() -> entity
{
    // Leave 0 as an invalid entity for now.
    return ++m_next_entity;
}

void mope::game_scene::destroy_entity(entity e)
{
    for (auto&& manager : m_component_managers) {
        manager.second->remove(e);
    }
}

auto mope::game_scene::ensure_renderer() -> sprite_renderer&
{
    if (!m_sprite_renderer) {
        m_sprite_renderer = std::make_unique<sprite_renderer>();
    }
    return *m_sprite_renderer;
}

void mope::game_scene::tick(double time_step, input_state const& inputs)
{
    auto& renderer = ensure_renderer();
    renderer.pre_tick(*this);

    emplace_event<tick_event>(time_step, inputs);
    auto& events = m_event_pool.events();
    for (auto i = 0uz; i < events.size(); ++i) {
        // Processing events potentially pushes more events, which potentially
        // invalidates any references we take here. Since this seems like a path
        // to madness, we are intentionally copying here.
        auto [event, process_event] = events[i];
        process_event(*this, event);
    }
    m_event_pool.clear();
}

void mope::game_scene::render(double alpha)
{
    ensure_renderer().render(*this, alpha);
}

auto mope::event_pool::events() -> decltype(m_events) const&
{
    return m_events;
}

void mope::event_pool::clear()
{
    m_events.clear();
}
