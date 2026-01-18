#include "mope_game_engine/game_engine.hxx"

#include "glad/glad.h"
#include "mope_game_engine/components/input_state.hxx"
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

namespace
{
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

mope::game_engine::game_engine(std::shared_ptr<I_logger> logger)
    : m_new_scenes{ }
    , m_scenes{ }
    , m_tick_time{ 0.0 }
    , m_logger{ std::move(logger) }
    , m_input_state{ }
    , m_default_texture{ }
{
}

mope::game_engine::~game_engine() = default;

void mope::game_engine::set_tick_rate(double hz_rate)
{
    m_tick_time = hz_rate > 0.0 ? 1.0 / hz_rate : 0.0;
}

void mope::game_engine::add_scene(std::unique_ptr<game_scene> scene)
{
    m_new_scenes.push_back(std::move(scene));
}

void mope::game_engine::run(I_game_window& window)
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

    prepare_gl_resources();

    // Set the initial client and input state.
    auto previous_key_states = std::bitset<256>{};
    m_input_state.held_keys = previous_key_states;
    m_input_state.pressed_keys = previous_key_states;
    m_input_state.released_keys = previous_key_states;
    m_input_state.cursor_deltas = { 0, 0 };

    window.process_inputs();
    m_input_state.cursor_position = window.cursor_pos();
    m_input_state.client_size = window.client_size();

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
    while (!window.wants_to_close()) {
        if (auto new_scenes = m_new_scenes.size(); new_scenes > 0) {
            m_scenes.reserve(m_scenes.size() + new_scenes);

            auto range = std::ranges::subrange{
                std::make_move_iterator(m_new_scenes.begin()),
                std::make_move_iterator(m_new_scenes.end())
            };

            for (auto&& scene : range) {
                // Give the scene access to the external components that we control.
                scene->set_external_component(&m_input_state);
                scene->set_external_component(m_logger.get());

                scene->on_load(*this);
                m_scenes.push_back(std::move(scene));
            }

            m_new_scenes.clear();
        }

        // Process inputs from the window as frequently as possible.
        window.process_inputs();

        m_input_state.held_keys = window.key_states();
        m_input_state.pressed_keys |= m_input_state.held_keys & ~previous_key_states;
        m_input_state.released_keys |= ~m_input_state.held_keys & previous_key_states;

        auto t = clock::now();
        auto delta_seconds = std::chrono::duration_cast<seconds>(t - t0).count();
        accumulator += delta_seconds;
        t0 = t;

        auto dt = m_tick_time > 0.0 ? m_tick_time : accumulator;

        if (accumulator >= dt) {
            // Cache these since they are virtual function calls that have to
            // perform an unknown amount of work, and won't change in between
            // ticks / scenes.
            m_input_state.cursor_position = window.cursor_pos();
            m_input_state.cursor_deltas = window.cursor_deltas();
            m_input_state.client_size = window.client_size();

            // Tick each window for each time step that has passed.
            // TODO: Avoid death spiral in case a game update takes longer than m_ticktime.
            do {
                for (auto&& scene : m_scenes) {
                    scene->tick(*this, dt);
                }

                // Only send "pressed" and "released" states once, even if we
                // are processing multiple ticks.
                m_input_state.pressed_keys.reset();
                m_input_state.released_keys.reset();

                accumulator -= dt;

#if defined(LOG_FPS) && LOG_FPS
                ++tick_counter;
#endif
            } while (accumulator >= dt);

            previous_key_states = m_input_state.held_keys;
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
            if (m_logger) {
                m_logger->log(
                    "fps: " + std::to_string(fps) + " / ticks: " + std::to_string(tps),
                    I_logger::log_level::debug
                );
            }
        }
#endif

        // Erase any scenes reporting that they are done. It might make sense to
        // only do this check after calling process_tick(), but we don't expect
        // there to be a large number of scenes, so it's probably fine.
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

        // If there are no more scenes left, we should close and quit.
        if (m_scenes.empty()) {
            window.close();
        }
        else {
            // If the steptime is non-zero, we can use it to compute alpha for interpolation.
            // If it is zero, then alpha will also be zero and there will be no interpolation.
            auto alpha = accumulator / dt;

            // TODO: Support FPS capping.

            // Clear everything previously on the screen.
            ::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            // Render all scenes.
            for (auto&& scene : m_scenes) {
                scene->render(alpha);
            }

            // Finally, tell the window that the next frame is ready.
            window.swap();
        }
    }

    resources.cleanup();

    if (m_logger) {
        if (auto count = gl::resource_id::outstanding_count(); 0 != count) {
            m_logger->log(
                std::to_string(count) + " OpenGL resources left outstanding.",
                I_logger::log_level::warning
            );
        }
        else {
            m_logger->log(
                "All OpenGL resources were cleaned up.",
                I_logger::log_level::debug
            );
        }
    }
}

auto mope::game_engine::get_default_texture() const -> gl::texture const&
{
    return m_default_texture;
}

auto mope::game_engine::get_logger() const -> I_logger const*
{
    return m_logger.get();
}

void mope::game_engine::prepare_gl_resources()
{
    constexpr unsigned char bytes[]{ 0xffu, 0xffu, 0xffu, 0xffu };
    m_default_texture.make(bytes, gl::pixel_format::rgba, 1, 1);

#if defined(DEBUG)
    ::glEnable(GL_DEBUG_OUTPUT);
    ::glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    ::glDebugMessageCallback(on_debug_message, this);
#endif // defined(DEBUG)
}

void mope::game_engine::release_gl_resources()
{
    m_default_texture = gl::texture{};
    ::glDebugMessageCallback(NULL, NULL);
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
        GLsizei length,
        GLchar const* message,
        void const* user_param
    )
    {
        if (nullptr != user_param) {
            auto engine = static_cast<mope::game_engine const*>(user_param);
            if (auto logger = engine->get_logger()) {
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

                logger->log(msg, level);
            }
        }
    }
}
