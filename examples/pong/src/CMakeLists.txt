target_sources(
    pong

    PRIVATE
        "glfw.hxx" "glfw.cxx"
        "pong.hxx" "pong.cxx"
)

if (WIN32)
    target_sources(pong PRIVATE "winmain.cxx")
else()
    target_sources(pong PRIVATE "main.cxx")
endif()
