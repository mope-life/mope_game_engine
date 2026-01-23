add_executable(pong WIN32)
add_subdirectory("src")

target_link_libraries(
    pong

    PRIVATE
        glfw_game_window
        mope_game_engine
)

target_compile_options(
    pong

    PRIVATE
        $<IF:$<CXX_COMPILER_ID:MSVC>,/W4,-Wall -Wextra>
)

install(TARGETS pong)
install(FILES $<TARGET_RUNTIME_DLLS:pong> TYPE BIN)
