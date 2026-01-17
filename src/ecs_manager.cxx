#include "mope_game_engine/component.hxx"
#include "mope_game_engine/ecs_manager.hxx"
#include "mope_game_engine/game_scene.hxx"
#include "mope_game_engine/game_system.hxx"

#include <memory>
#include <utility>

mope::ecs_manager::ecs_manager()
    : m_next_entity{ 0 }
    , m_component_managers{ }
    , m_game_systems{ }
{
}

mope::ecs_manager::~ecs_manager() = default;
mope::ecs_manager::ecs_manager(ecs_manager&&) = default;
auto mope::ecs_manager::operator=(ecs_manager&&) -> ecs_manager& = default;

auto mope::ecs_manager::create_entity() -> entity
{
    // Leave 0 as an invalid entity for now.
    return ++m_next_entity;
}

void mope::ecs_manager::destroy_entity(entity e)
{
    for (auto&& manager : m_component_managers) {
        manager.second->remove(e);
    }
}

void mope::ecs_manager::add_game_system(std::unique_ptr<game_system_base> system)
{
    m_game_systems.push_back(std::move(system));
}

void mope::ecs_manager::run_systems(double time_step)
{
    for (auto&& system : m_game_systems) {
        system->process_tick(static_cast<game_scene&>(*this), time_step);
    }
}
