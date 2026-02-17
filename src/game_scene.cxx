#include "mope_game_engine/game_scene.hxx"

#include "mope_game_engine/components/component.hxx"
#include "mope_game_engine/components/logger.hxx"
#include "mope_game_engine/events/tick.hxx"
#include "mope_vec/mope_vec.hxx"
#include "sprite_renderer.hxx"

#include <memory>
#include <utility>

mope::game_scene::game_scene()
    : m_last_entity{ NoEntity }
    , m_game_systems{ }
    , m_event_pool{ }
    , m_events{ }
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
    m_sprite_renderer->set_projection(projection);
}

auto mope::game_scene::create_entity() -> entity_id
{
    // Don't return the special NoEntity (0).
    return ++m_last_entity;
}

void mope::game_scene::destroy_entity(entity_id entity)
{
    for (auto&& manager : m_entity_component_stores) {
        manager.second->remove(entity);
    }
}

auto mope::game_scene::logger() -> I_logger*
{
    return get_component<I_logger>();
}

void mope::game_scene::tick(double time_step, input_state const& inputs)
{
    m_sprite_renderer->pre_tick(*this);

    emplace_event<tick_event>(time_step, inputs);
    for (auto i = 0uz; i < m_events.size(); ++i) {
        // Processing events potentially pushes more events, which potentially
        // invalidates any references we take here. Since this seems like a path
        // to madness, we are intentionally copying here.
        auto [event, process_event] = m_events[i];
        process_event(*this, event);
    }
    m_events.clear();
}

void mope::game_scene::render(double alpha)
{
    m_sprite_renderer->render(*this, alpha);
}

void mope::game_scene::load(I_game_engine& engine)
{
    m_sprite_renderer = std::make_unique<sprite_renderer>();
    on_load(engine);
}

void mope::game_scene::unload(I_game_engine& engine)
{
    on_unload(engine);
}

bool mope::game_scene::close()
{
    return on_close();
}
