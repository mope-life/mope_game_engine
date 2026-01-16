#pragma once

#include "mope_game_engine/component.hxx"
#include "mope_vec/mope_vec.hxx"

#include <bitset>

namespace mope
{
    /// A singleton component representing the state of the client area and user
    /// input.
    struct input_state : public singleton_component
    {
        std::bitset<256> pressed_keys;  ///< Keys that were pressed this tick
        std::bitset<256> released_keys; ///< Keys that were released this tick
        std::bitset<256> held_keys;     ///< Keys that were still held by the end of this tick
        vec2f cursor_position;          ///< Position of the cursor relative to the upper left corner of the game window
        vec2f cursor_deltas;            ///< Motion of the cursor this tick
        vec2i client_size;              ///< Size in pixels of the game window
    };
}
