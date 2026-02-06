cmake_minimum_required(VERSION 3.28.3)
project(mope_game_engine VERSION 0.1.0)

set(
    CMAKE_CXX_STANDARD              23
    CMAKE_CXX_STANDARD_REQUIRED     TRUE
)

add_library(mope_game_engine STATIC)

target_compile_definitions(
    mope_game_engine

    # Export GLAD symbols for consumers.
    # We expect the `I_window` implementation to come up with its own GL context,
    # so at the very least it has to call `gladLoadGLLoader()`.
    # It's probably possible to abstract this away in the future.
    PUBLIC
        GLAD_GLAPI_EXPORT

    PRIVATE
        GLAD_GLAPI_EXPORT_BUILD
)

target_compile_options(
    mope_game_engine

    PRIVATE
        $<IF:$<CXX_COMPILER_ID:MSVC>,/W4 /WX,-Wall -Wextra -Werror>
)

target_link_libraries(
    mope_game_engine

    PUBLIC
        freetype
)

add_subdirectory("external")
add_subdirectory("include")
add_subdirectory("src")
add_subdirectory("examples")
