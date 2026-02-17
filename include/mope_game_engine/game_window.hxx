#pragma once

#include "mope_vec/mope_vec.hxx"

#include <memory>
#include <bitset>

namespace mope
{
    struct gl_context
    {
        virtual ~gl_context() = default;
    };

    class I_game_window
    {
    public:
        virtual ~I_game_window() = default;

        virtual auto get_context() -> std::unique_ptr<gl_context> = 0;
        virtual void process_inputs() = 0;
        virtual void swap() = 0;

        /// Return whether the window is trying to close.
        ///
        /// This can return `true` after the user clicks the "close" button on
        /// the window (if present), or if the @ref I_game_engine has told the
        /// window to close with the `close()` method.
        virtual auto wants_to_close() const -> bool = 0;

        /// Indicate that this window should or shouldn't close.
        ///
        /// The @ref I_game_engine will call this method with @p should_close true
        /// when there are no more scenes and the @ref I_game_engine is done.
        ///
        /// The @ref I_game_engine will also call this method with @p should_close
        /// false in the event that the window has said it wants to close, but
        /// we want to prevent that from happening (because the game needs to
        /// perform some kind of cleanup first).
        ///
        /// Note that the window should not actually be destroyed, as there will
        /// be an outstanding OpenGL context when this is called. A typical
        /// implementation will cause @ref wants_to_close() to return `true`.
        /// Destruction of the window is beyond the purview of @ref I_game_engine.
        virtual void close(bool should_close = true) = 0;

        /// Return a bitset representing which keys are currently pressed.
        virtual auto key_states() const -> std::bitset<256> = 0;

        /// Return the positionhn of the cursor relative to the top-left of the window.
        virtual auto cursor_pos() const -> vec2f = 0;

        /// Return the change in cursor position since this function was last called.
        ///
        /// Immediately calling this method again without another call to process_inputs()
        /// should return { 0, 0 }.
        virtual auto cursor_deltas() -> vec2f = 0;

        /// Return the size in pixels of the client area of the window.
        virtual auto client_size() const -> vec2i = 0;
    };
} // namespace mope
