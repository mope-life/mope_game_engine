#include "mope_game_engine/game_scene.hxx"

#include "mope_game_engine/ecs_manager.hxx"
#include "mope_game_engine/game_engine.hxx"
#include "mope_vec/mope_vec.hxx"

#include "shader.hxx"
#include "sprite_renderer.hxx"

#include <memory>

mope::game_scene::game_scene()
    : ecs_manager{}
    , pressed_keys{ }
    , released_keys{ }
    , held_keys{ }
    , cursor_position{ }
    , cursor_deltas{ }
    , client_size{ }
    , m_sprite_renderer{ }
    , m_initialized{ false }
    , m_done{ false }
{ }

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

void mope::game_scene::tick(game_engine& engine, double time_step)
{
    if (!m_initialized) {
        m_initialized = true;
        on_initial_tick(engine);
    }
    ensure_renderer().pre_tick(*this);
    run_systems(time_step);
}

void mope::game_scene::render(double alpha)
{
    ensure_renderer().render(*this, alpha);
}

auto mope::game_scene::ensure_renderer() -> sprite_renderer&
{
    if (!m_sprite_renderer) {
        m_sprite_renderer = std::make_unique<sprite_renderer>();
    }
    return *m_sprite_renderer;
}

void mope::game_scene::on_initial_tick(game_engine&)
{ }
