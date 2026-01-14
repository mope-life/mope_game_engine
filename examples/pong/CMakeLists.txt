add_executable(pong WIN32)
add_subdirectory("src")
set(GLFW_INSTALL FALSE)
add_subdirectory("external/glfw")

target_link_libraries(
    pong

    PRIVATE
        glfw
        mope_game_engine
)

set_target_properties(
    pong

    PROPERTIES
        CXX_STANDARD            23
        CXX_STANDARD_REQUIRED   TRUE
)

target_compile_definitions(
    pong

    PRIVATE
        $<$<CONFIG:Debug>:DEBUG=1>
)

install(TARGETS pong)
install(FILES $<TARGET_RUNTIME_DLLS:pong> TYPE BIN)

if (WIN32)
    get_target_property(MOPE_GAME_ENGINE_TYPE mope_game_engine TYPE)
    get_target_property(GLFW_TYPE glfw TYPE)

    if (${MOPE_GAME_ENGINE_TYPE} STREQUAL SHARED_LIBRARY OR ${GLFW_TYPE} STREQUAL SHARED_LIBRARY)
        # For debugging, copy the DLLs and PDBs to the binary output directory.
        # This is only necessary for Windows.
        add_custom_target(
            copy_runtime_dependencies
            COMMENT "Copying runtime dependencies to output directory"
            COMMAND
                ${CMAKE_COMMAND} -E copy_if_different
                    $<TARGET_RUNTIME_DLLS:pong>
                    $<$<AND:$<STREQUAL:$<TARGET_PROPERTY:mope_game_engine,TYPE>,SHARED_LIBRARY>,$<CONFIG:Debug>>:$<TARGET_PDB_FILE:mope_game_engine>>
                    $<$<AND:$<STREQUAL:$<TARGET_PROPERTY:glfw,TYPE>,SHARED_LIBRARY>,$<CONFIG:Debug>>:$<TARGET_PDB_FILE:glfw>>
                    $<TARGET_FILE_DIR:pong>
            COMMAND_EXPAND_LISTS
            VERBATIM
        )
        add_dependencies(pong copy_runtime_dependencies)
    endif()
endif()
