add_library(glad STATIC)

add_subdirectory("src")
add_subdirectory("include")

target_compile_definitions(
    glad

    PUBLIC
        GLAD_GLAPI_EXPORT_BUILD

    # NOTE: If ever switching this to a shared library, we need to change this to:
    # PUBLIC GLAD_GLAPI_EXPORT
    # PRIVATE GLAD_GLAPI_EXPORT_BUILD
)
