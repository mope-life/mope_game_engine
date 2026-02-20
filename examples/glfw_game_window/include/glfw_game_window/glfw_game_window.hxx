#pragma once

#include "mope_game_engine/game_engine.hxx"
#include "mope_game_engine/game_window.hxx"
#include "mope_vec/mope_vec.hxx"

#include <bitset>
#include <memory>
#include <stdexcept>

namespace mope
{
    class glfw_error : public std::runtime_error
    {
        using runtime_error::runtime_error;
    };
} // namespace mope

namespace mope::glfw
{
    enum key : int
    {
        UNKNOWN,
        SPACE, APOSTROPHE, COMMA, MINUS,
        PERIOD, SLASH, SEMICOLON, EQUAL,
        R1, R2, R3, R4, R5, R6, R7, R8, R9, R0,
        A, B, C, D, E, F, G, H, I, J, K, L, M,
        N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        LEFT_BRACKET, BACKSLASH, RIGHT_BRACKET,
        GRAVE_ACCENT,
        WORLD_1, WORLD_2,
        ESCAPE, ENTER, TAB, BACKSPACE,
        INSERT, DELETE,
        RIGHT, LEFT, DOWN, UP,
        PAGE_UP, PAGE_DOWN,
        HOME, END,
        CAPS_LOCK, SCROLL_LOCK, NUM_LOCK,
        PRINT_SCREEN, PAUSE,
        F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13,
        F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25,
        KP_0, KP_1, KP_2, KP_3, KP_4, KP_5, KP_6, KP_7, KP_8, KP_9,
        KP_DECIMAL, KP_DIVIDE, KP_MULTIPLY, KP_SUBTRACT, KP_ADD,
        KP_ENTER, KP_EQUAL,
        LEFT_SHIFT, LEFT_CONTROL, LEFT_ALT, LEFT_SUPER,
        RIGHT_SHIFT, RIGHT_CONTROL, RIGHT_ALT, RIGHT_SUPER,
        MENU,
    };

    enum class window_mode
    {
        windowed,
        fullscreen,
    };

    enum class cursor_mode
    {
        normal,
        hidden,
        disabled,
    };

    class window : public I_game_window
    {
    public:
        window(
            char const* title,
            vec2i dimensions,
            window_mode mode,
            gl::version_and_profile profile = I_game_engine::opengl_version_and_profile()
        );
        ~window() noexcept = default;
        window(window&&) noexcept;
        window& operator=(window&&) noexcept;
        window(window const&) = delete;
        window& operator=(window const&) = delete;

        void swap(window&);

        friend void swap(window& a, window& b) noexcept
        {
            a.swap(b);
        }

        void set_cursor_mode(cursor_mode mode);

        // Implementation of mope::I_game_window.
        auto get_context() -> std::unique_ptr<gl_context> override;
        void process_inputs() override;
        void swap() override;
        auto wants_to_close() const -> bool override;
        void close(bool should_close) override;

        auto key_states() const -> std::bitset<256> override;
        auto cursor_pos() const -> vec2f override;
        auto cursor_deltas() -> vec2f override;
        auto client_size() const -> vec2i override;

    private:
        void handle_key(int k, int action);
        void handle_resize(int width, int height);
        void handle_cursor_pos(double xpos, double ypos);

        struct imp;
        std::shared_ptr<imp> m_imp;

        vec2i m_client_size;
        vec2f m_cursor_pos;
        vec2f m_cursor_deltas;
        std::bitset<256> m_key_states;
    };
} // namespace mope::glfw
