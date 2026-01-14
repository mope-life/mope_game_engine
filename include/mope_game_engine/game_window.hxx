#pragma once

#include "mope_game_engine/mope_game_engine_export.hxx"

#include <mope_vec/mope_vec.hxx>
#include <memory>

#include <bitset>

namespace mope
{
    struct MOPE_GAME_ENGINE_EXPORT gl_context
    {
        virtual ~gl_context() = default;
    };

    class MOPE_GAME_ENGINE_EXPORT I_game_window
    {
    public:
        virtual ~I_game_window() = default;

        virtual auto get_context() -> std::unique_ptr<gl_context> = 0;
        virtual void process_inputs() = 0;
        virtual void swap() = 0;
        virtual auto wants_to_close() const -> bool = 0;

        /// Indicate that this window should close.
        ///
        /// Note that the window should not actually be destroyed, as there will
        /// be an outstanding OpenGL context when this is called. A typical
        /// implementation will cause @ref wants_to_close() to return `true`.
        /// Destruction of the window is beyond the purview of @ref game_engine.
        virtual void close() = 0;

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
