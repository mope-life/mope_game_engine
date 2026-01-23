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
#include <unordered_map>

namespace
{
    auto remap_glfw_key(int key) -> std::optional<mope::glfw::key>;
    void throw_glfw_error(int code, const char* description);
    static auto create_glfw_window(int width, int height, char const* title, mope::glfw::window::mode mode) -> GLFWwindow*;
} // namespace

namespace mope::glfw
{
    class library_lifetime
    {
        struct lock {};

    public:
        library_lifetime(lock);
        ~library_lifetime();

        static auto get() -> std::shared_ptr<library_lifetime>;
    };

    struct context : public gl_context
    {
        context(GLFWwindow* glfw_window);
        ~context();

        GLFWwindow* m_glfw_window;
        GLFWwindow* m_previous_context;
    };
} // namespace mope::glfw

mope::glfw::library_lifetime::library_lifetime(lock)
{
    ::glfwSetErrorCallback(throw_glfw_error);

    if (GLFW_TRUE == glfwInit()) {
        ::glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
        ::glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
        ::glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#if defined(DEBUG)
        ::glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif // defined(DEBUG)
    }
    else {
        throw glfw_error{ "Failed to initialize." };
    }
}

mope::glfw::library_lifetime::~library_lifetime()
{
    ::glfwTerminate();
}

auto mope::glfw::library_lifetime::get() -> std::shared_ptr<library_lifetime>
{
    static auto s_this = std::weak_ptr<library_lifetime>{};
    auto ptr = s_this.lock();
    if (!ptr) {
        ptr = std::make_shared<library_lifetime>(lock{});
        s_this = ptr;
    }
    return ptr;
}

mope::glfw::context::context(GLFWwindow* glfw_window)
    : m_glfw_window{ glfw_window }
    , m_previous_context{ glfwGetCurrentContext() }
{
    ::glfwMakeContextCurrent(m_glfw_window);

    // Now that the context is current on this thread, we can load GL procs.
    if (0 == ::gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw glfw_error{ "Failed to load GL proc addresses." };
    }
}

mope::glfw::context::~context()
{
    ::glfwMakeContextCurrent(m_previous_context);
}

mope::glfw::window::window(int width, int height, char const* title, mode mode)
    : m_glfw{ library_lifetime::get() }
    , m_impl{ create_glfw_window(width, height, title, mode) }
    , m_client_size{ }
    , m_cursor_pos{ }
    , m_cursor_deltas{ }
    , m_key_states{ }
{
    auto glfw_window = static_cast<GLFWwindow*>(m_impl);

    if (!glfw_window) {
        throw glfw_error{ "Failed to create a GLFW window." };
    }

    ::glfwSetWindowUserPointer(glfw_window, this);

    ::glfwSetKeyCallback(
        glfw_window,
        [](GLFWwindow* glfw_window, int k, int, int action, int) {
            auto user_ptr = static_cast<window*>(::glfwGetWindowUserPointer(glfw_window));
            user_ptr->handle_key(k, action);
        });

    ::glfwSetFramebufferSizeCallback(
        glfw_window,
        [](GLFWwindow* glfw_window, int width, int height) {
            auto user_ptr = static_cast<window*>(::glfwGetWindowUserPointer(glfw_window));
            user_ptr->handle_resize(width, height);
        });

    ::glfwSetCursorPosCallback(
        glfw_window,
        [](GLFWwindow* glfw_window, double xpos, double ypos) {
            auto user_ptr = static_cast<window*>(::glfwGetWindowUserPointer(glfw_window));
            user_ptr->handle_cursor_pos(xpos, ypos);
        });

    // Get the initial framebuffer dimensions
    int initial_width = 0;
    int initial_height = 0;
    ::glfwGetFramebufferSize(glfw_window, &initial_width, &initial_height);
    handle_resize(initial_width, initial_height);

    ::glfwPollEvents();
}

mope::glfw::window::~window()
{
    if (nullptr != m_impl) {
        auto glfw_window = static_cast<GLFWwindow*>(m_impl);
        ::glfwDestroyWindow(glfw_window);
    }
}

mope::glfw::window::window(window&& that) noexcept
    : m_glfw{ }
    , m_impl{ nullptr }
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
    swap(m_glfw, that.m_glfw);
    swap(m_impl, that.m_impl);
    swap(m_client_size, that.m_client_size);
    swap(m_cursor_pos, that.m_cursor_pos);
    swap(m_cursor_deltas, that.m_cursor_deltas);
    swap(m_key_states, that.m_key_states);

    if (nullptr != m_impl) {
        ::glfwSetWindowUserPointer(static_cast<GLFWwindow*>(m_impl), this);
    }

    if (nullptr != that.m_impl) {
        ::glfwSetWindowUserPointer(static_cast<GLFWwindow*>(that.m_impl), &that);
    }
}

void mope::glfw::window::set_cursor_mode(cursor_mode mode)
{
    auto glfw_window = static_cast<GLFWwindow*>(m_impl);

    switch (mode) {
    case cursor_mode::normal:
        ::glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        break;
    case cursor_mode::hidden:
        ::glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
        break;
    case cursor_mode::disabled:
        ::glfwSetInputMode(glfw_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        break;
    }
}

auto mope::glfw::window::get_context() -> std::unique_ptr<gl_context>
{
    auto glfw_window = static_cast<GLFWwindow*>(m_impl);
    return std::make_unique<context>(glfw_window);
}

void mope::glfw::window::process_inputs()
{
    ::glfwPollEvents();
}

void mope::glfw::window::swap()
{
    auto glfw_window = static_cast<GLFWwindow*>(m_impl);
    ::glfwSwapBuffers(glfw_window);
}

auto mope::glfw::window::wants_to_close() const -> bool
{
    auto glfw_window = static_cast<GLFWwindow*>(m_impl);
    return ::glfwWindowShouldClose(glfw_window);
}

void mope::glfw::window::close(bool should_close)
{
    auto glfw_window = static_cast<GLFWwindow*>(m_impl);
    ::glfwSetWindowShouldClose(glfw_window, should_close ? GLFW_TRUE : GLFW_FALSE);
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
    const std::unordered_map<int, mope::glfw::key> REMAPPED_KEYS{
        {GLFW_KEY_UNKNOWN,       mope::glfw::key::UNKNOWN      },
        {GLFW_KEY_SPACE,         mope::glfw::key::SPACE        },
        {GLFW_KEY_APOSTROPHE,    mope::glfw::key::APOSTROPHE   },
        {GLFW_KEY_COMMA,         mope::glfw::key::COMMA        },
        {GLFW_KEY_MINUS,         mope::glfw::key::MINUS        },
        {GLFW_KEY_PERIOD,        mope::glfw::key::PERIOD       },
        {GLFW_KEY_SLASH,         mope::glfw::key::SLASH        },
        {GLFW_KEY_0,             mope::glfw::key::R0           },
        {GLFW_KEY_1,             mope::glfw::key::R1           },
        {GLFW_KEY_2,             mope::glfw::key::R2           },
        {GLFW_KEY_3,             mope::glfw::key::R3           },
        {GLFW_KEY_4,             mope::glfw::key::R4           },
        {GLFW_KEY_5,             mope::glfw::key::R5           },
        {GLFW_KEY_6,             mope::glfw::key::R6           },
        {GLFW_KEY_7,             mope::glfw::key::R7           },
        {GLFW_KEY_8,             mope::glfw::key::R8           },
        {GLFW_KEY_9,             mope::glfw::key::R9           },
        {GLFW_KEY_SEMICOLON,     mope::glfw::key::SEMICOLON    },
        {GLFW_KEY_EQUAL,         mope::glfw::key::EQUAL        },
        {GLFW_KEY_A,             mope::glfw::key::A            },
        {GLFW_KEY_B,             mope::glfw::key::B            },
        {GLFW_KEY_C,             mope::glfw::key::C            },
        {GLFW_KEY_D,             mope::glfw::key::D            },
        {GLFW_KEY_E,             mope::glfw::key::E            },
        {GLFW_KEY_F,             mope::glfw::key::F            },
        {GLFW_KEY_G,             mope::glfw::key::G            },
        {GLFW_KEY_H,             mope::glfw::key::H            },
        {GLFW_KEY_I,             mope::glfw::key::I            },
        {GLFW_KEY_J,             mope::glfw::key::J            },
        {GLFW_KEY_K,             mope::glfw::key::K            },
        {GLFW_KEY_L,             mope::glfw::key::L            },
        {GLFW_KEY_M,             mope::glfw::key::M            },
        {GLFW_KEY_N,             mope::glfw::key::N            },
        {GLFW_KEY_O,             mope::glfw::key::O            },
        {GLFW_KEY_P,             mope::glfw::key::P            },
        {GLFW_KEY_Q,             mope::glfw::key::Q            },
        {GLFW_KEY_R,             mope::glfw::key::R            },
        {GLFW_KEY_S,             mope::glfw::key::S            },
        {GLFW_KEY_T,             mope::glfw::key::T            },
        {GLFW_KEY_U,             mope::glfw::key::U            },
        {GLFW_KEY_V,             mope::glfw::key::V            },
        {GLFW_KEY_W,             mope::glfw::key::W            },
        {GLFW_KEY_X,             mope::glfw::key::X            },
        {GLFW_KEY_Y,             mope::glfw::key::Y            },
        {GLFW_KEY_Z,             mope::glfw::key::Z            },
        {GLFW_KEY_LEFT_BRACKET,  mope::glfw::key::LEFT_BRACKET },
        {GLFW_KEY_BACKSLASH,     mope::glfw::key::BACKSLASH    },
        {GLFW_KEY_RIGHT_BRACKET, mope::glfw::key::RIGHT_BRACKET},
        {GLFW_KEY_GRAVE_ACCENT,  mope::glfw::key::GRAVE_ACCENT },
        {GLFW_KEY_WORLD_1,       mope::glfw::key::WORLD_1      },
        {GLFW_KEY_WORLD_2,       mope::glfw::key::WORLD_2      },
        {GLFW_KEY_ESCAPE,        mope::glfw::key::ESCAPE       },
        {GLFW_KEY_ENTER,         mope::glfw::key::ENTER        },
        {GLFW_KEY_TAB,           mope::glfw::key::TAB          },
        {GLFW_KEY_BACKSPACE,     mope::glfw::key::BACKSPACE    },
        {GLFW_KEY_INSERT,        mope::glfw::key::INSERT       },
        {GLFW_KEY_DELETE,        mope::glfw::key::DELETE       },
        {GLFW_KEY_RIGHT,         mope::glfw::key::RIGHT        },
        {GLFW_KEY_LEFT,          mope::glfw::key::LEFT         },
        {GLFW_KEY_DOWN,          mope::glfw::key::DOWN         },
        {GLFW_KEY_UP,            mope::glfw::key::UP           },
        {GLFW_KEY_PAGE_UP,       mope::glfw::key::PAGE_UP      },
        {GLFW_KEY_PAGE_DOWN,     mope::glfw::key::PAGE_DOWN    },
        {GLFW_KEY_HOME,          mope::glfw::key::HOME         },
        {GLFW_KEY_END,           mope::glfw::key::END          },
        {GLFW_KEY_CAPS_LOCK,     mope::glfw::key::CAPS_LOCK    },
        {GLFW_KEY_SCROLL_LOCK,   mope::glfw::key::SCROLL_LOCK  },
        {GLFW_KEY_NUM_LOCK,      mope::glfw::key::NUM_LOCK     },
        {GLFW_KEY_PRINT_SCREEN,  mope::glfw::key::PRINT_SCREEN },
        {GLFW_KEY_PAUSE,         mope::glfw::key::PAUSE        },
        {GLFW_KEY_F1,            mope::glfw::key::F1           },
        {GLFW_KEY_F2,            mope::glfw::key::F2           },
        {GLFW_KEY_F3,            mope::glfw::key::F3           },
        {GLFW_KEY_F4,            mope::glfw::key::F4           },
        {GLFW_KEY_F5,            mope::glfw::key::F5           },
        {GLFW_KEY_F6,            mope::glfw::key::F6           },
        {GLFW_KEY_F7,            mope::glfw::key::F7           },
        {GLFW_KEY_F8,            mope::glfw::key::F8           },
        {GLFW_KEY_F9,            mope::glfw::key::F9           },
        {GLFW_KEY_F10,           mope::glfw::key::F10          },
        {GLFW_KEY_F11,           mope::glfw::key::F11          },
        {GLFW_KEY_F12,           mope::glfw::key::F12          },
        {GLFW_KEY_F13,           mope::glfw::key::F13          },
        {GLFW_KEY_F14,           mope::glfw::key::F14          },
        {GLFW_KEY_F15,           mope::glfw::key::F15          },
        {GLFW_KEY_F16,           mope::glfw::key::F16          },
        {GLFW_KEY_F17,           mope::glfw::key::F17          },
        {GLFW_KEY_F18,           mope::glfw::key::F18          },
        {GLFW_KEY_F19,           mope::glfw::key::F19          },
        {GLFW_KEY_F20,           mope::glfw::key::F20          },
        {GLFW_KEY_F21,           mope::glfw::key::F21          },
        {GLFW_KEY_F22,           mope::glfw::key::F22          },
        {GLFW_KEY_F23,           mope::glfw::key::F23          },
        {GLFW_KEY_F24,           mope::glfw::key::F24          },
        {GLFW_KEY_F25,           mope::glfw::key::F25          },
        {GLFW_KEY_KP_0,          mope::glfw::key::KP_0         },
        {GLFW_KEY_KP_1,          mope::glfw::key::KP_1         },
        {GLFW_KEY_KP_2,          mope::glfw::key::KP_2         },
        {GLFW_KEY_KP_3,          mope::glfw::key::KP_3         },
        {GLFW_KEY_KP_4,          mope::glfw::key::KP_4         },
        {GLFW_KEY_KP_5,          mope::glfw::key::KP_5         },
        {GLFW_KEY_KP_6,          mope::glfw::key::KP_6         },
        {GLFW_KEY_KP_7,          mope::glfw::key::KP_7         },
        {GLFW_KEY_KP_8,          mope::glfw::key::KP_8         },
        {GLFW_KEY_KP_9,          mope::glfw::key::KP_9         },
        {GLFW_KEY_KP_DECIMAL,    mope::glfw::key::KP_DECIMAL   },
        {GLFW_KEY_KP_DIVIDE,     mope::glfw::key::KP_DIVIDE    },
        {GLFW_KEY_KP_MULTIPLY,   mope::glfw::key::KP_MULTIPLY  },
        {GLFW_KEY_KP_SUBTRACT,   mope::glfw::key::KP_SUBTRACT  },
        {GLFW_KEY_KP_ADD,        mope::glfw::key::KP_ADD       },
        {GLFW_KEY_KP_ENTER,      mope::glfw::key::KP_ENTER     },
        {GLFW_KEY_KP_EQUAL,      mope::glfw::key::KP_EQUAL     },
        {GLFW_KEY_LEFT_SHIFT,    mope::glfw::key::LEFT_SHIFT   },
        {GLFW_KEY_LEFT_CONTROL,  mope::glfw::key::LEFT_CONTROL },
        {GLFW_KEY_LEFT_ALT,      mope::glfw::key::LEFT_ALT     },
        {GLFW_KEY_LEFT_SUPER,    mope::glfw::key::LEFT_SUPER   },
        {GLFW_KEY_RIGHT_SHIFT,   mope::glfw::key::RIGHT_SHIFT  },
        {GLFW_KEY_RIGHT_CONTROL, mope::glfw::key::RIGHT_CONTROL},
        {GLFW_KEY_RIGHT_ALT,     mope::glfw::key::RIGHT_ALT    },
        {GLFW_KEY_RIGHT_SUPER,   mope::glfw::key::RIGHT_SUPER  },
        {GLFW_KEY_MENU,          mope::glfw::key::MENU         },
    };

    auto remap_glfw_key(int key) -> std::optional<mope::glfw::key>
    {
        auto iter = REMAPPED_KEYS.find(key);
        return REMAPPED_KEYS.end() != iter
            ? std::make_optional(static_cast<mope::glfw::key>(iter->second))
            : std::nullopt;
    }

    void throw_glfw_error(int code, const char* description)
    {
        std::string what = "Error code " + std::to_string(code) + ": " + description;
        throw mope::glfw_error{ what };
    }

    auto create_glfw_window(int width, int height, char const* title, mope::glfw::window::mode mode) -> GLFWwindow*
    {
        auto monitor
            = mope::glfw::window::mode::fullscreen == mode ? ::glfwGetPrimaryMonitor() : nullptr;
        return ::glfwCreateWindow(width, height, title, monitor, nullptr);
    }

} // namespace
