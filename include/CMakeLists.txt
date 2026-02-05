target_sources(
    mope_game_engine

    PRIVATE
        "mope_game_engine/event_pool.inl"

    PUBLIC
    FILE_SET    HEADERS
    BASE_DIRS   "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        "glad/glad.h"
        "KHR/khrplatform.h"
        "mope_game_engine/components/component.hxx"
        "mope_game_engine/components/logger.hxx"
        "mope_game_engine/components/sprite.hxx"
        "mope_game_engine/components/transform.hxx"
        "mope_game_engine/collisions.hxx"
        "mope_game_engine/component_manager.hxx"
        "mope_game_engine/event_pool.hxx"
        "mope_game_engine/events/tick.hxx"
        "mope_game_engine/game_engine.hxx"
        "mope_game_engine/game_scene.hxx"
        "mope_game_engine/game_system.hxx"
        "mope_game_engine/game_window.hxx"
        "mope_game_engine/query.hxx"
        "mope_game_engine/resource_id.hxx"
        "mope_game_engine/texture.hxx"
        "mope_game_engine/transforms.hxx"
        "mope_vec/mope_vec.hxx"
)
