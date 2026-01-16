cmake_minimum_required(VERSION 3.28.3)
project(mope_game_engine VERSION 0.1.0)

option(
    MOPE_GAME_ENGINE_SHARED_LIB
    "Build as a shared library."
)

if(${MOPE_GAME_ENGINE_SHARED_LIB})
    set(MOPE_GAME_ENGINE_LIBRARY_TYPE "SHARED")
endif()

add_library(mope_game_engine "${MOPE_GAME_ENGINE_LIBRARY_TYPE}")

set_target_properties(
    mope_game_engine

    PROPERTIES
        CXX_STANDARD                23
        CXX_STANDARD_REQUIRED       TRUE
        CXX_VISIBILITY_PRESET       "hidden"
        VISIBILITY_INLINES_HIDDEN   TRUE
)

include(GenerateExportHeader)
generate_export_header(
    mope_game_engine

    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/mope_game_engine/mope_game_engine_export.hxx"
)

target_sources(
    mope_game_engine

    PUBLIC
    FILE_SET    generated_headers
    TYPE        HEADERS
    BASE_DIRS   "${CMAKE_CURRENT_BINARY_DIR}"
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/mope_game_engine/mope_game_engine_export.hxx"
)

target_compile_definitions(
    mope_game_engine

    PUBLIC
        GLAD_GLAPI_EXPORT

    PRIVATE
        GLAD_GLAPI_EXPORT_BUILD
        $<$<CONFIG:Debug>:DEBUG=1>
)

add_subdirectory("include")
add_subdirectory("src")
add_subdirectory("examples")
