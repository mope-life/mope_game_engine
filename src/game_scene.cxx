#include "mope_game_engine/game_scene.hxx"

#include "mope_game_engine/component.hxx"
#include "mope_game_engine/game_system.hxx"
#include "mope_vec/mope_vec.hxx"

#include "shader.hxx"
#include "sprite_renderer.hxx"

#include <utility>
#include <memory>

mope::game_scene::game_scene()
    : m_last_entity{ NoEntity }
    , m_component_managers{ }
    , m_game_systems{ }
    , m_sprite_renderer{ }
    , m_done{ false }
{
}

mope::game_scene::~game_scene() = default;
mope::game_scene::game_scene(game_scene&&) = default;
auto mope::game_scene::operator=(game_scene&&) -> game_scene& = default;

void mope::game_scene::add_game_system(std::unique_ptr<game_system_base> system)
{
    m_game_systems.push_back(std::move(system));
}

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

auto mope::game_scene::create_entity() -> entity_id
{
    // Don't return the special NoEntity (0).
    return ++m_last_entity;
}

void mope::game_scene::destroy_entity(entity_id entity)
{
    for (auto&& manager : m_component_managers) {
        manager.second->remove(entity);
    }
}

auto mope::game_scene::ensure_renderer() -> sprite_renderer&
{
    if (!m_sprite_renderer) {
        m_sprite_renderer = std::make_unique<sprite_renderer>();
    }
    return *m_sprite_renderer;
}

void mope::game_scene::tick(double time_step)
{
    ensure_renderer().pre_tick(*this);

    for (auto&& system : m_game_systems) {
        system->process_tick(*this, time_step);
    }
}

void mope::game_scene::render(double alpha)
{
    ensure_renderer().render(*this, alpha);
}
