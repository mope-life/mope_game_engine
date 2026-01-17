target_sources(
    mope_game_engine

    PUBLIC
    FILE_SET    HEADERS
    BASE_DIRS   "${CMAKE_CURRENT_SOURCE_DIR}"
    FILES
        "glad/glad.h"
        "KHR/khrplatform.h"
        "mope_game_engine/components/input_state.hxx"
        "mope_game_engine/components/sprite.hxx"
        "mope_game_engine/components/transform.hxx"
        "mope_game_engine/collisions.hxx"
        "mope_game_engine/component.hxx"
        "mope_game_engine/game_engine.hxx"
        "mope_game_engine/game_scene.hxx"
        "mope_game_engine/game_system.hxx"
        "mope_game_engine/game_window.hxx"
        "mope_game_engine/resource_id.hxx"
        "mope_game_engine/texture.hxx"
        "mope_game_engine/transforms.hxx"
        "mope_vec/mope_vec.hxx"
)
