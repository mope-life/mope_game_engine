#include "glfw_game_window/glfw_game_window.hxx"

// clang-format off
// These headers must be in order
#include "glad/glad.h"
#include "GLFW/glfw3.h"
// clang-format on

#include "mope_game_engine/game_window.hxx"
#include "mope_vec/mope_vec.hxx"

#include <bitset>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace
{
    void init_glfw();
    void deinit_glfw();
    auto remap_glfw_key(int key) -> std::optional<mope::glfw::key>;
    void throw_glfw_error(int code, const char* description);
} // namespace

namespace mope::glfw
{
    struct context : public gl_context
    {
        context(GLFWwindow* glfw_window);
        ~context();

        GLFWwindow* m_previous_context;
    };

    struct window::imp
    {
        imp(
            char const* title,
            vec2i dimensions,
            glfw::window_mode mode,
            gl::version_and_profile profile
        );
        ~imp();

        imp(imp const&) = delete;
        auto operator=(imp const&) -> imp& = delete;

        operator GLFWwindow*();

        GLFWwindow* m_glfw_window;
        static int s_glfw_use_count;
    };

    int window::imp::s_glfw_use_count = 0;
} // namespace mope::glfw

mope::glfw::context::context(GLFWwindow* glfw_window)
    : m_previous_context{ glfwGetCurrentContext() }
{
    ::glfwMakeContextCurrent(glfw_window);

    // Now that the context is current on this thread, we can load GL procs.
    if (!::gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw glfw_error{ "Failed to load GL proc addresses." };
    }
}

mope::glfw::context::~context()
{
    ::glfwMakeContextCurrent(m_previous_context);
}

mope::glfw::window::imp::imp(
    char const* title,
    vec2i dimensions,
    glfw::window_mode mode,
    gl::version_and_profile profile)
{
    if (++s_glfw_use_count == 1) {
        init_glfw();
    }

    auto monitor
        = mope::glfw::window_mode::fullscreen == mode ? ::glfwGetPrimaryMonitor() : nullptr;

    ::glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, profile.major_version);
    ::glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, profile.minor_version);

    if (profile.major_version < 3 || profile.major_version == 3 && profile.minor_version < 2) {
        ::glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_ANY_PROFILE);
    }
    else if (profile.profile == profile.core) {
        ::glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    }
    else if (profile.profile == profile.compat) {
        ::glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_COMPAT_PROFILE);
    }

#if defined(DEBUG)
    ::glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif // defined(DEBUG)

    if (!(m_glfw_window
        = ::glfwCreateWindow(dimensions.x(), dimensions.y(), title, monitor, nullptr)))
    {
        throw glfw_error{ "Failed to create a GLFW window." };
    }
}

mope::glfw::window::imp::~imp()
{
    ::glfwDestroyWindow(m_glfw_window);

    if (--s_glfw_use_count == 0) {
        deinit_glfw();
    }
}

mope::glfw::window::imp::operator GLFWwindow* ()
{
    return m_glfw_window;
}

mope::glfw::window::window(
    char const* title,
    vec2i dimensions,
    window_mode mode,
    gl::version_and_profile profile
)
    : m_imp{ std::make_shared<imp>(title, dimensions, mode, profile) }
    , m_client_size{ }
    , m_cursor_pos{ }
    , m_cursor_deltas{ }
    , m_key_states{ }
{
    ::glfwSetWindowUserPointer(*m_imp, this);

    ::glfwSetKeyCallback(
        *m_imp,
        [](GLFWwindow* glfw_window, int k, int, int action, int) {
            auto user_ptr = static_cast<window*>(::glfwGetWindowUserPointer(glfw_window));
            user_ptr->handle_key(k, action);
        });

    ::glfwSetFramebufferSizeCallback(
        *m_imp,
        [](GLFWwindow* glfw_window, int width, int height) {
            auto user_ptr = static_cast<window*>(::glfwGetWindowUserPointer(glfw_window));
            user_ptr->handle_resize(width, height);
        });

    ::glfwSetCursorPosCallback(
        *m_imp,
        [](GLFWwindow* glfw_window, double xpos, double ypos) {
            auto user_ptr = static_cast<window*>(::glfwGetWindowUserPointer(glfw_window));
            user_ptr->handle_cursor_pos(xpos, ypos);
        });

    // Get the initial framebuffer dimensions
    int initial_width = 0;
    int initial_height = 0;
    ::glfwGetFramebufferSize(*m_imp, &initial_width, &initial_height);
    handle_resize(initial_width, initial_height);

    ::glfwPollEvents();
}

mope::glfw::window::window(window&& that) noexcept
    : m_imp{ }
    , m_client_size{ }
    , m_cursor_pos{ }
    , m_cursor_deltas{ }
    , m_key_states{ }
{
    swap(that);
}

auto mope::glfw::window::operator=(window&& that) noexcept -> window&
{
    swap(that);
    return *this;
}

void mope::glfw::window::swap(window& that)
{
    using std::swap;
    swap(m_imp, that.m_imp);
    swap(m_client_size, that.m_client_size);
    swap(m_cursor_pos, that.m_cursor_pos);
    swap(m_cursor_deltas, that.m_cursor_deltas);
    swap(m_key_states, that.m_key_states);

    if (m_imp) {
        ::glfwSetWindowUserPointer(*m_imp, this);
    }

    if (that.m_imp) {
        ::glfwSetWindowUserPointer(*that.m_imp, &that);
    }
}

void mope::glfw::window::set_cursor_mode(cursor_mode mode)
{
    switch (mode) {
    case cursor_mode::normal:
        ::glfwSetInputMode(*m_imp, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        break;
    case cursor_mode::hidden:
        ::glfwSetInputMode(*m_imp, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        break;
    case cursor_mode::disabled:
        ::glfwSetInputMode(*m_imp, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        break;
    }
}

auto mope::glfw::window::get_context() -> std::unique_ptr<gl_context>
{
    return std::make_unique<context>(*m_imp);
}

void mope::glfw::window::process_inputs()
{
    ::glfwPollEvents();
}

void mope::glfw::window::swap()
{
    ::glfwSwapBuffers(*m_imp);
}

auto mope::glfw::window::wants_to_close() const -> bool
{
    return ::glfwWindowShouldClose(*m_imp);
}

void mope::glfw::window::close(bool should_close)
{
    ::glfwSetWindowShouldClose(*m_imp, should_close ? GLFW_TRUE : GLFW_FALSE);
}

auto mope::glfw::window::key_states() const -> std::bitset<256>
{
    return m_key_states;
}

auto mope::glfw::window::cursor_pos() const -> vec2f
{
    return m_cursor_pos;
}

auto mope::glfw::window::cursor_deltas() -> vec2f
{
    auto deltas = m_cursor_deltas;
    m_cursor_deltas = { 0, 0 };
    return deltas;
}

auto mope::glfw::window::client_size() const -> vec2i
{
    return m_client_size;
}

void mope::glfw::window::handle_key(int k, int action)
{
    if (auto index = remap_glfw_key(k)) {
        switch (action) {
        case GLFW_PRESS:   m_key_states.set(*index); break;
        case GLFW_RELEASE: m_key_states.reset(*index); break;
        default:           break;
        }
    }
}

void mope::glfw::window::handle_resize(int width, int height)
{
    m_client_size = { width, height };
}

void mope::glfw::window::handle_cursor_pos(double xpos, double ypos)
{
    vec2f new_pos{ static_cast<float>(xpos), static_cast<float>(ypos) };
    m_cursor_deltas += new_pos - m_cursor_pos;
    m_cursor_pos = new_pos;
}

namespace
{
    void init_glfw()
    {
        ::glfwSetErrorCallback(throw_glfw_error);
        if (GLFW_TRUE != glfwInit()) {
            throw mope::glfw_error{ "Failed to initialize GLFW." };
        }
    }

    void deinit_glfw()
    {
        ::glfwTerminate();
    }

    void throw_glfw_error(int code, const char* description)
    {
        std::string what = "Error code " + std::to_string(code) + ": " + description;
        throw mope::glfw_error{ what };
    }

    auto remap_glfw_key(int glfw_key) -> std::optional<mope::glfw::key>
    {
        switch (glfw_key)
        {
        case GLFW_KEY_UNKNOWN:       return mope::glfw::key::UNKNOWN;
        case GLFW_KEY_SPACE:         return mope::glfw::key::SPACE;
        case GLFW_KEY_APOSTROPHE:    return mope::glfw::key::APOSTROPHE;
        case GLFW_KEY_COMMA:         return mope::glfw::key::COMMA;
        case GLFW_KEY_MINUS:         return mope::glfw::key::MINUS;
        case GLFW_KEY_PERIOD:        return mope::glfw::key::PERIOD;
        case GLFW_KEY_SLASH:         return mope::glfw::key::SLASH;
        case GLFW_KEY_0:             return mope::glfw::key::R0;
        case GLFW_KEY_1:             return mope::glfw::key::R1;
        case GLFW_KEY_2:             return mope::glfw::key::R2;
        case GLFW_KEY_3:             return mope::glfw::key::R3;
        case GLFW_KEY_4:             return mope::glfw::key::R4;
        case GLFW_KEY_5:             return mope::glfw::key::R5;
        case GLFW_KEY_6:             return mope::glfw::key::R6;
        case GLFW_KEY_7:             return mope::glfw::key::R7;
        case GLFW_KEY_8:             return mope::glfw::key::R8;
        case GLFW_KEY_9:             return mope::glfw::key::R9;
        case GLFW_KEY_SEMICOLON:     return mope::glfw::key::SEMICOLON;
        case GLFW_KEY_EQUAL:         return mope::glfw::key::EQUAL;
        case GLFW_KEY_A:             return mope::glfw::key::A;
        case GLFW_KEY_B:             return mope::glfw::key::B;
        case GLFW_KEY_C:             return mope::glfw::key::C;
        case GLFW_KEY_D:             return mope::glfw::key::D;
        case GLFW_KEY_E:             return mope::glfw::key::E;
        case GLFW_KEY_F:             return mope::glfw::key::F;
        case GLFW_KEY_G:             return mope::glfw::key::G;
        case GLFW_KEY_H:             return mope::glfw::key::H;
        case GLFW_KEY_I:             return mope::glfw::key::I;
        case GLFW_KEY_J:             return mope::glfw::key::J;
        case GLFW_KEY_K:             return mope::glfw::key::K;
        case GLFW_KEY_L:             return mope::glfw::key::L;
        case GLFW_KEY_M:             return mope::glfw::key::M;
        case GLFW_KEY_N:             return mope::glfw::key::N;
        case GLFW_KEY_O:             return mope::glfw::key::O;
        case GLFW_KEY_P:             return mope::glfw::key::P;
        case GLFW_KEY_Q:             return mope::glfw::key::Q;
        case GLFW_KEY_R:             return mope::glfw::key::R;
        case GLFW_KEY_S:             return mope::glfw::key::S;
        case GLFW_KEY_T:             return mope::glfw::key::T;
        case GLFW_KEY_U:             return mope::glfw::key::U;
        case GLFW_KEY_V:             return mope::glfw::key::V;
        case GLFW_KEY_W:             return mope::glfw::key::W;
        case GLFW_KEY_X:             return mope::glfw::key::X;
        case GLFW_KEY_Y:             return mope::glfw::key::Y;
        case GLFW_KEY_Z:             return mope::glfw::key::Z;
        case GLFW_KEY_LEFT_BRACKET:  return mope::glfw::key::LEFT_BRACKET;
        case GLFW_KEY_BACKSLASH:     return mope::glfw::key::BACKSLASH;
        case GLFW_KEY_RIGHT_BRACKET: return mope::glfw::key::RIGHT_BRACKET;
        case GLFW_KEY_GRAVE_ACCENT:  return mope::glfw::key::GRAVE_ACCENT;
        case GLFW_KEY_WORLD_1:       return mope::glfw::key::WORLD_1;
        case GLFW_KEY_WORLD_2:       return mope::glfw::key::WORLD_2;
        case GLFW_KEY_ESCAPE:        return mope::glfw::key::ESCAPE;
        case GLFW_KEY_ENTER:         return mope::glfw::key::ENTER;
        case GLFW_KEY_TAB:           return mope::glfw::key::TAB;
        case GLFW_KEY_BACKSPACE:     return mope::glfw::key::BACKSPACE;
        case GLFW_KEY_INSERT:        return mope::glfw::key::INSERT;
        case GLFW_KEY_DELETE:        return mope::glfw::key::DELETE;
        case GLFW_KEY_RIGHT:         return mope::glfw::key::RIGHT;
        case GLFW_KEY_LEFT:          return mope::glfw::key::LEFT;
        case GLFW_KEY_DOWN:          return mope::glfw::key::DOWN;
        case GLFW_KEY_UP:            return mope::glfw::key::UP;
        case GLFW_KEY_PAGE_UP:       return mope::glfw::key::PAGE_UP;
        case GLFW_KEY_PAGE_DOWN:     return mope::glfw::key::PAGE_DOWN;
        case GLFW_KEY_HOME:          return mope::glfw::key::HOME;
        case GLFW_KEY_END:           return mope::glfw::key::END;
        case GLFW_KEY_CAPS_LOCK:     return mope::glfw::key::CAPS_LOCK;
        case GLFW_KEY_SCROLL_LOCK:   return mope::glfw::key::SCROLL_LOCK;
        case GLFW_KEY_NUM_LOCK:      return mope::glfw::key::NUM_LOCK;
        case GLFW_KEY_PRINT_SCREEN:  return mope::glfw::key::PRINT_SCREEN;
        case GLFW_KEY_PAUSE:         return mope::glfw::key::PAUSE;
        case GLFW_KEY_F1:            return mope::glfw::key::F1;
        case GLFW_KEY_F2:            return mope::glfw::key::F2;
        case GLFW_KEY_F3:            return mope::glfw::key::F3;
        case GLFW_KEY_F4:            return mope::glfw::key::F4;
        case GLFW_KEY_F5:            return mope::glfw::key::F5;
        case GLFW_KEY_F6:            return mope::glfw::key::F6;
        case GLFW_KEY_F7:            return mope::glfw::key::F7;
        case GLFW_KEY_F8:            return mope::glfw::key::F8;
        case GLFW_KEY_F9:            return mope::glfw::key::F9;
        case GLFW_KEY_F10:           return mope::glfw::key::F10;
        case GLFW_KEY_F11:           return mope::glfw::key::F11;
        case GLFW_KEY_F12:           return mope::glfw::key::F12;
        case GLFW_KEY_F13:           return mope::glfw::key::F13;
        case GLFW_KEY_F14:           return mope::glfw::key::F14;
        case GLFW_KEY_F15:           return mope::glfw::key::F15;
        case GLFW_KEY_F16:           return mope::glfw::key::F16;
        case GLFW_KEY_F17:           return mope::glfw::key::F17;
        case GLFW_KEY_F18:           return mope::glfw::key::F18;
        case GLFW_KEY_F19:           return mope::glfw::key::F19;
        case GLFW_KEY_F20:           return mope::glfw::key::F20;
        case GLFW_KEY_F21:           return mope::glfw::key::F21;
        case GLFW_KEY_F22:           return mope::glfw::key::F22;
        case GLFW_KEY_F23:           return mope::glfw::key::F23;
        case GLFW_KEY_F24:           return mope::glfw::key::F24;
        case GLFW_KEY_F25:           return mope::glfw::key::F25;
        case GLFW_KEY_KP_0:          return mope::glfw::key::KP_0;
        case GLFW_KEY_KP_1:          return mope::glfw::key::KP_1;
        case GLFW_KEY_KP_2:          return mope::glfw::key::KP_2;
        case GLFW_KEY_KP_3:          return mope::glfw::key::KP_3;
        case GLFW_KEY_KP_4:          return mope::glfw::key::KP_4;
        case GLFW_KEY_KP_5:          return mope::glfw::key::KP_5;
        case GLFW_KEY_KP_6:          return mope::glfw::key::KP_6;
        case GLFW_KEY_KP_7:          return mope::glfw::key::KP_7;
        case GLFW_KEY_KP_8:          return mope::glfw::key::KP_8;
        case GLFW_KEY_KP_9:          return mope::glfw::key::KP_9;
        case GLFW_KEY_KP_DECIMAL:    return mope::glfw::key::KP_DECIMAL;
        case GLFW_KEY_KP_DIVIDE:     return mope::glfw::key::KP_DIVIDE;
        case GLFW_KEY_KP_MULTIPLY:   return mope::glfw::key::KP_MULTIPLY;
        case GLFW_KEY_KP_SUBTRACT:   return mope::glfw::key::KP_SUBTRACT;
        case GLFW_KEY_KP_ADD:        return mope::glfw::key::KP_ADD;
        case GLFW_KEY_KP_ENTER:      return mope::glfw::key::KP_ENTER;
        case GLFW_KEY_KP_EQUAL:      return mope::glfw::key::KP_EQUAL;
        case GLFW_KEY_LEFT_SHIFT:    return mope::glfw::key::LEFT_SHIFT;
        case GLFW_KEY_LEFT_CONTROL:  return mope::glfw::key::LEFT_CONTROL;
        case GLFW_KEY_LEFT_ALT:      return mope::glfw::key::LEFT_ALT;
        case GLFW_KEY_LEFT_SUPER:    return mope::glfw::key::LEFT_SUPER;
        case GLFW_KEY_RIGHT_SHIFT:   return mope::glfw::key::RIGHT_SHIFT;
        case GLFW_KEY_RIGHT_CONTROL: return mope::glfw::key::RIGHT_CONTROL;
        case GLFW_KEY_RIGHT_ALT:     return mope::glfw::key::RIGHT_ALT;
        case GLFW_KEY_RIGHT_SUPER:   return mope::glfw::key::RIGHT_SUPER;
        case GLFW_KEY_MENU:          return mope::glfw::key::MENU;
        default:                     return std::nullopt;
        }
    }
} // namespace
