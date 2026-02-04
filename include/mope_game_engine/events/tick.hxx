#pragma once

#include "mope_vec/mope_vec.hxx"

#include <bitset>

namespace mope
{
    struct input_state
    {
        std::bitset<256> pressed_keys;  ///< Keys that were pressed this tick
        std::bitset<256> released_keys; ///< Keys that were released this tick
        std::bitset<256> held_keys;     ///< Keys that were still held by the end of this tick
        vec2f cursor_position;          ///< Position of the cursor relative to the upper left corner of the game window
        vec2f cursor_deltas;            ///< Motion of the cursor this tick
        vec2i client_size;              ///< Size in pixels of the game window
    };

    struct tick_event
    {
        double time_step;               ///< The amount of time, in seconds, since the last frame_update.
        input_state const& inputs;
    };
}
