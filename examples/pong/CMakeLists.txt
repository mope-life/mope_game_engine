add_executable(pong WIN32)

target_link_libraries(
    pong

    PRIVATE
        glfw_game_window
        mope_game_engine
)

target_compile_options(
    pong

    PRIVATE
        $<IF:$<CXX_COMPILER_ID:MSVC>,/W4 /WX,-Wall -Wextra -Werror>
)

add_subdirectory("src")
