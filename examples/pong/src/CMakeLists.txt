target_sources(
    pong

    PRIVATE
        "future.hxx"
        "glfw.hxx" "glfw.cxx"
        "pong.hxx" "pong.cxx"
)

if (WIN32)
    target_sources(pong PRIVATE "winmain.cxx")
else()
    target_sources(pong PRIVATE "main.cxx")
endif()
