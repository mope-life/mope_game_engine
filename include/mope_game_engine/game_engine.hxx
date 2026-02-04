#pragma once

#include "mope_game_engine/texture.hxx"

#include <memory>
#include <vector>

namespace mope
{
    struct I_logger;
    class I_game_window;
    class game_scene;
} // namespace mope

namespace mope
{
    class game_engine final
    {
    public:
        game_engine();
        ~game_engine();
        game_engine(game_engine const&) = delete;
        game_engine& operator=(game_engine const&) = delete;

        void set_tick_rate(double hz_rate);
        void add_scene(std::unique_ptr<game_scene> scene);
        void run(I_game_window& window, I_logger* logger = nullptr);

        auto get_default_texture() const -> gl::texture const&;

    private:
        void prepare_gl_resources(I_logger* logger);
        void release_gl_resources();
        void load_scenes(I_logger* logger);
        void unload_scenes();
        bool keep_alive(I_game_window& window);
        void draw(I_game_window& window, double alpha);

        std::vector<std::unique_ptr<game_scene>> m_new_scenes;
        std::vector<std::unique_ptr<game_scene>> m_scenes;
        double m_tick_time;

        gl::texture m_default_texture;
    };
} // namespace mope
