target_sources(
    mope_game_engine

    PRIVATE
        "glad/glad.c"
        "buffer_object.hxx" "buffer_object.cxx"
        "collisions.cxx"
        "ecs_manager.cxx"
        "game_engine.cxx"
        "game_scene.cxx"
        "resource_id.cxx"
        "shader.hxx" "shader.cxx"
        "sprite_renderer.hxx" "sprite_renderer.cxx"
        "texture.cxx"
        "vao.hxx" "vao.cxx"
)
