add_library(glfw_game_window STATIC)

set(GLFW_INSTALL FALSE)

add_subdirectory("external")
add_subdirectory("include")
add_subdirectory("src")

target_link_libraries(
    glfw_game_window

    PUBLIC
        mope_game_engine

    PRIVATE
        glfw
)

target_compile_options(
    glfw_game_window

    PRIVATE
        $<IF:$<CXX_COMPILER_ID:MSVC>,/W4 /WX,-Wall -Wextra -Werror>
)
