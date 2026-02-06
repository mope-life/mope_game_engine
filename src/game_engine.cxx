#include "mope_game_engine/game_engine.hxx"

#include "freetype.hxx"
#include "glad/glad.h"
#include "mope_game_engine/components/logger.hxx"
#include "mope_game_engine/events/tick.hxx"
#include "mope_game_engine/font.hxx"
#include "mope_game_engine/game_engine_error.hxx"
#include "mope_game_engine/game_scene.hxx"
#include "mope_game_engine/game_window.hxx"
#include "mope_game_engine/resource_id.hxx"
#include "mope_game_engine/texture.hxx"

#include <algorithm>
#include <bitset>
#include <chrono>
#include <concepts>
#include <iterator>
#include <memory>
#include <ranges>
#include <ratio>
#include <string>
#include <utility>
#include <vector>

#define LOG_FPS true

namespace mope
{
    class game_engine final : public I_game_engine
    {
    public:
        game_engine();
        ~game_engine();
        void destroy() override;
        void set_tick_rate(double hz_rate) override;
        void add_scene(std::unique_ptr<game_scene> scene) override;
        void run(I_game_window& window, I_logger* logger) override;
        auto make_font(char const* ttf_path, int face_index, int instance_index = 0) -> font override;
        auto get_default_texture() const -> gl::texture const& override;

        void prepare_gl_resources(I_logger* logger);
        void release_gl_resources();
        void load_scenes(I_logger* logger);
        void unload_scenes();
        bool keep_alive(I_game_window& window);
        void draw(I_game_window& window, double alpha);

        std::vector<std::unique_ptr<game_scene>> m_new_scenes;
        std::vector<std::unique_ptr<game_scene>> m_scenes;
        double m_tick_time;
        input_state m_input_state;
        gl::texture m_default_texture;
        FT_Library m_ft_library;
    };
}

auto mope_game_engine_create() -> mope::I_game_engine*
{
    return new mope::game_engine{ };
}

namespace
{
    [[maybe_unused]]
    void GLAPIENTRY on_debug_message(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei length,
        GLchar const* message,
        void const* user_param
    );

    template <std::invocable<> F>
    struct finally
    {
        finally(F f)
            : cleaned_up{ false }
            , f{ std::move(f) }
        {
        }

        ~finally()
        {
            cleanup();
        }

        void cleanup()
        {
            if (!cleaned_up) {
                std::invoke(f);
                cleaned_up = true;
            }
        }

        bool cleaned_up;
        F f;
    };
}

mope::game_engine::game_engine()
    : m_new_scenes{ }
    , m_scenes{ }
    , m_tick_time{ 0.0 }
    , m_default_texture{ }
    , m_ft_library{ nullptr }
{
}

mope::game_engine::~game_engine()
{
    if (nullptr != m_ft_library) {
        FT_Done_FreeType(m_ft_library);
        m_ft_library = nullptr;
    }
}

void mope::game_engine::destroy()
{
    delete this;
}

void mope::game_engine::set_tick_rate(double hz_rate)
{
    m_tick_time = hz_rate > 0.0 ? 1.0 / hz_rate : 0.0;
}

void mope::game_engine::add_scene(std::unique_ptr<game_scene> scene)
{
    m_new_scenes.push_back(std::move(scene));
}

void mope::game_engine::run(I_game_window& window, I_logger* logger)
{
    auto context = window.get_context();
    if (!context) {
        throw game_engine_error{ "Window returned null OpenGL context." };
    }

    // We will try to ensure that our OpenGL resources are disposed of even if we end on an error.
    auto resources = finally {
        [this]() {
            for (auto&& scene : m_scenes) {
                scene->on_unload(*this);
            }
            m_scenes.clear();
            release_gl_resources();
        }
    };

    prepare_gl_resources(logger);

    auto inputs = input_state{};

    // Set the initial client and input state.
    auto previous_key_states = std::bitset<256>{};
    inputs.held_keys = previous_key_states;
    inputs.pressed_keys = previous_key_states;
    inputs.released_keys = previous_key_states;
    inputs.cursor_deltas = { 0, 0 };

    window.process_inputs();
    inputs.cursor_position = window.cursor_pos();
    inputs.client_size = window.client_size();

    using seconds = std::chrono::duration<double, std::ratio<1, 1>>;
    using clock = std::chrono::steady_clock;

    auto t0 = clock::now();
    auto accumulator = 0.0;

#if defined(LOG_FPS) && LOG_FPS
    auto fps_t0 = clock::now();
    auto frame_counter = 0;
    auto tick_counter = 0;
#endif

    // We will stay in the loop until the window confirms that it's time to
    // close. This likely but not necessarily comes just after we call
    // window.close().
    // We want to make sure to load any new scenes before we do this check,
    // because they get an opportunity to reject closing.
    while (load_scenes(logger), keep_alive(window)) {
        // Process inputs from the window as frequently as possible.
        window.process_inputs();

        inputs.held_keys = window.key_states();
        inputs.pressed_keys |= inputs.held_keys & ~previous_key_states;
        inputs.released_keys |= ~inputs.held_keys & previous_key_states;

        auto t = clock::now();
        auto delta_seconds = std::chrono::duration_cast<seconds>(t - t0).count();
        accumulator += delta_seconds;
        t0 = t;

        auto dt = m_tick_time > 0.0 ? m_tick_time : accumulator;

        if (accumulator >= dt) {
            // Cache these since they are virtual function calls that have to
            // perform an unknown amount of work, and won't change in between
            // ticks / scenes.
            inputs.cursor_position = window.cursor_pos();
            inputs.cursor_deltas = window.cursor_deltas();
            inputs.client_size = window.client_size();

            // Tick each window for each time step that has passed.
            // TODO: Avoid death spiral in case a game update takes longer than m_ticktime.
            do {
                for (auto&& scene : m_scenes) {
                    scene->tick(dt, inputs);
                }

                // Only send "pressed" and "released" states once, even if we
                // are processing multiple ticks.
                inputs.pressed_keys.reset();
                inputs.released_keys.reset();

                accumulator -= dt;

#if defined(LOG_FPS) && LOG_FPS
                ++tick_counter;
#endif
            } while (accumulator >= dt);

            previous_key_states = inputs.held_keys;

            // Erase any scenes reporting that they are done. We only do this
            // check after process tick because that is the only place where the
            // scene is expected to implement any logic.
            unload_scenes();

            // If there are no more scenes left, we should tell the window to close.
            if (m_scenes.empty() && m_new_scenes.empty()) {
                window.close();
            }
        }

#if defined(LOG_FPS) && LOG_FPS
        ++frame_counter;
        if ((t - fps_t0) > seconds{ 1.0 }) {
            auto delta = std::chrono::duration_cast<seconds>(t - fps_t0).count();
            fps_t0 = t;

            auto fps = frame_counter / delta;
            auto tps = tick_counter / delta;
            frame_counter = 0;
            tick_counter = 0;
            if (nullptr != logger) {
                logger->log(
                    ("fps: " + std::to_string(fps) + " / ticks: " + std::to_string(tps)).c_str(),
                    I_logger::log_level::debug
                );
            }
        }
#endif

        // If the steptime is non-zero, we can use it to compute alpha for interpolation.
        // If it is zero, then alpha will also be zero and there will be no interpolation.
        auto alpha = accumulator / dt;

        // Finally, tell all the scenes to render.
        draw(window, alpha);
    }

    resources.cleanup();

    if (nullptr != logger) {
        if (auto count = gl::resource_id::outstanding_count(); 0 != count) {
            logger->log(
                (std::to_string(count) + " OpenGL resources left outstanding.").c_str(),
                I_logger::log_level::warning
            );
        }
        else {
            logger->log(
                "All OpenGL resources were cleaned up.",
                I_logger::log_level::debug
            );
        }
    }
}

auto mope::game_engine::make_font(char const* ttf_path, int face_index, int instance_index) -> font
{
    if (nullptr == m_ft_library) {
        check_ft_error(
            FT_Init_FreeType(&m_ft_library),
            "initializing FreeType library"
        );
    }

    FT_Face face = nullptr;

    /// TODO: We definitely shouldn't actually throw here, since we're taking a
    /// path from the user.
    check_ft_error(FT_New_Face(
        m_ft_library,
        ttf_path,
        static_cast<FT_Long>(face_index) | (static_cast<FT_Long>(instance_index) << 16),
        &face),
        "creating font face"
    );

    return font{face};
}

auto mope::game_engine::get_default_texture() const -> gl::texture const&
{
    return m_default_texture;
}

void mope::game_engine::prepare_gl_resources(I_logger* logger)
{
    constexpr unsigned char bytes[]{ 0xffu, 0xffu, 0xffu, 0xffu };
    m_default_texture.make(bytes, gl::pixel_format::rgba, 1, 1);

#if !defined(NDEBUG)
    ::glEnable(GL_DEBUG_OUTPUT);
    ::glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    ::glDebugMessageCallback(on_debug_message, logger);
#endif // !defined(NDEBUG)
}

void mope::game_engine::release_gl_resources()
{
    m_default_texture = gl::texture{};
    ::glDebugMessageCallback(NULL, NULL);
}

void mope::game_engine::load_scenes(I_logger* logger)
{
    if (auto new_scenes = m_new_scenes.size(); new_scenes > 0) {
        m_scenes.reserve(m_scenes.size() + new_scenes);

        auto range = std::ranges::subrange{
            std::make_move_iterator(m_new_scenes.begin()),
            std::make_move_iterator(m_new_scenes.end())
        };

        for (auto&& scene : range) {
            // Give the scene access to the external components that we control.
            scene->set_external_component(logger);

            scene->on_load(*this);
            m_scenes.push_back(std::move(scene));
        }

        m_new_scenes.clear();
    }
}

void mope::game_engine::unload_scenes()
{
    m_scenes.erase(
        std::stable_partition(
            m_scenes.begin(),
            m_scenes.end(),
            [this](auto&& scene) {
                bool done = scene->is_done();
                if (done) {
                    scene->on_unload(*this);
                }
                return !done;
            }
        ),
        m_scenes.end()
    );
}

bool mope::game_engine::keep_alive(I_game_window& window)
{
    bool wants_to_close = window.wants_to_close();

    bool rejected = false;
    if (wants_to_close) {
        // We want to call on_close() on every scene, so that the user doesn't
        // have to worry about one scene rejecting the close before another can
        // see it.
        for (auto&& scene : m_scenes) {
            rejected = !scene->on_close() || rejected;
        }
    }

    if (wants_to_close && rejected) {
        // Tell the window not to close after all.
        window.close(false);
    }

    return !wants_to_close || rejected;
}

void mope::game_engine::draw(I_game_window& window, double alpha)
{
    // Clear everything previously on the screen.
    ::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render all scenes.
    for (auto&& scene : m_scenes) {
        scene->render(alpha);
    }

    // Tell the window that the next frame is ready.
    window.swap();
}

namespace
{
    auto debug_source(GLenum source) -> char const*
    {
        switch (source) {
        case GL_DEBUG_SOURCE_API:             return "API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   return "WINDOW_SYSTEM";
        case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER_COMPILER";
        case GL_DEBUG_SOURCE_THIRD_PARTY:     return "THIRD_PARTY";
        case GL_DEBUG_SOURCE_APPLICATION:     return "APPLICATION";
        case GL_DEBUG_SOURCE_OTHER:           return "OTHER";
        default:                              return "UNKNOWN";
        }
    }

    auto debug_type(GLenum type) -> char const*
    {
        switch (type) {
        case GL_DEBUG_TYPE_ERROR:               return "ERROR";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED_BEHAVIOR";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "UNDEFINED_BEHAVIOR";
        case GL_DEBUG_TYPE_PORTABILITY:         return "PORTABILITY";
        case GL_DEBUG_TYPE_PERFORMANCE:         return "PERFORMANCE";
        case GL_DEBUG_TYPE_MARKER:              return "MARKER";
        case GL_DEBUG_TYPE_PUSH_GROUP:          return "PUSH_GROUP";
        case GL_DEBUG_TYPE_POP_GROUP:           return "POP_GROUP";
        case GL_DEBUG_TYPE_OTHER:               return "OTHER";
        default:                                return "UNKNOWN";
        }
    }

    auto debug_severity(GLenum severity) -> char const*
    {
        switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH:         return "HIGH";
        case GL_DEBUG_SEVERITY_MEDIUM:       return "MEDIUM";
        case GL_DEBUG_SEVERITY_LOW:          return "LOW";
        case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
        default:                             return "UNKNOWN";
        }
    }

    void GLAPIENTRY on_debug_message(
        GLenum source,
        GLenum type,
        GLuint id,
        GLenum severity,
        GLsizei /* length */,
        GLchar const* message,
        void const* user_param
    )
    {
        if (auto logger = static_cast<mope::I_logger const*>(user_param)) {
            auto msg = std::string{ "OpenGL message:\n" }
                + "    Id:       " + std::to_string(id) + '\n'
                + "    Source:   " + debug_source(source) + '\n'
                + "    Type:     " + debug_type(type) + '\n'
                + "    Severity: " + debug_severity(severity) + '\n'
                + "    ----------" + '\n'
                + message;

            using log_level = mope::I_logger::log_level;

            auto level = log_level::error;
            switch (severity) {
            case GL_DEBUG_SEVERITY_LOW:          level = log_level::warning; break;
            case GL_DEBUG_SEVERITY_NOTIFICATION: level = log_level::notification; break;
            }

            logger->log(msg.c_str(), level);
        }
    }
}
