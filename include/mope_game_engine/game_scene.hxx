#pragma once

#include "mope_game_engine/ecs_manager.hxx"
#include "mope_game_engine/mope_game_engine_export.hxx"
#include "mope_vec/mope_vec.hxx"

#include <memory>
#include <bitset>

namespace mope
{
    class game_engine;
    class sprite_renderer;
}

namespace mope
{
    class MOPE_GAME_ENGINE_EXPORT game_scene : public ecs_manager
    {
        // Customization points:
        virtual void on_initial_tick(game_engine& engine);

    public:
        game_scene();
        virtual ~game_scene() = 0;

        void set_done(bool done = true);
        auto is_done() const -> bool;

        void set_projection_matrix(mat4f const& projection);

        std::bitset<256> pressed_keys;  ///< Keys that were pressed this tick
        std::bitset<256> released_keys; ///< Keys that were released this tick
        std::bitset<256> held_keys;     ///< Keys that were still held by the end of this tick
        vec2f cursor_position;          ///< Position of the cursor relative to the upper left corner of the game window
        vec2f cursor_deltas;            ///< Motion of the cursor this tick
        vec2i client_size;              ///< Size in pixels of the game window

    private:
        friend class game_engine;
        void tick(game_engine& engine, double time_step);
        void render(double alpha);
        auto ensure_renderer() -> sprite_renderer&;

        std::unique_ptr<sprite_renderer> m_sprite_renderer;
        bool m_initialized;
        bool m_done;
    };
}
