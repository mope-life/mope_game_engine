#pragma once

#include "mope_game_engine/components/input_state.hxx"
#include "mope_game_engine/mope_game_engine_export.hxx"
#include "mope_game_engine/texture.hxx"

#include <concepts>
#include <memory>
#include <string_view>
#include <vector>

namespace mope
{
    class I_game_window;
    class game_scene;
} // namespace mope

namespace mope
{
    class MOPE_GAME_ENGINE_EXPORT I_logger
    {
    public:
        virtual ~I_logger() = default;

        enum class log_level
        {
            error,
            warning,
            notification,
            debug,
        };

        virtual void log(std::string_view message, log_level level) const = 0;

        static auto level_string(log_level level) -> std::string_view
        {
            switch (level) {
            case log_level::error:
                return "ERROR";
            case log_level::warning:
                return "WARNING";
            case log_level::notification:
                return "NOTIFICATION";
            case log_level::debug:
                return "DEBUG";
            default:
                return "MISC";
            }
        }
    };

    class MOPE_GAME_ENGINE_EXPORT game_engine final
    {
    public:
        game_engine(std::shared_ptr<I_logger> logger = nullptr);
        ~game_engine();
        game_engine(game_engine const&) = delete;
        game_engine& operator=(game_engine const&) = delete;

        void set_tick_rate(double hz_rate);
        void add_scene(std::unique_ptr<game_scene> scene);
        void run(I_game_window& window);

        template <std::derived_from<game_scene>... Scenes>
        void run(
            I_game_window& window,
            std::unique_ptr<Scenes>... scenes
        )
        {
            (add_scene(std::move(scenes)), ...);
            run(window);
        }

        auto get_default_texture() const -> gl::texture const&;
        auto get_logger() const -> I_logger const*;

    private:
        void prepare_gl_resources();
        void release_gl_resources();

        std::shared_ptr<I_logger> m_logger;

        std::vector<std::unique_ptr<game_scene>> m_scenes;
        input_state m_input_state;
        double m_ticktime;

        gl::texture m_default_texture;
    };
} // namespace mope
