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
    class I_game_engine
    {
    protected:
        ~I_game_engine() = default;

    public:
        virtual void destroy() = 0;

        virtual void set_tick_rate(double hz_rate) = 0;

        virtual void add_scene(std::unique_ptr<game_scene> scene) = 0;

        // The game engine does NOT take ownership of the logger pointer. You
        // are resposible for freeing it after run() has returned.
        virtual void run(I_game_window& window, I_logger* = nullptr) = 0;
        virtual auto get_default_texture() const -> gl::texture const& = 0;
    };
} // namespace mope

auto mope_game_engine_create() -> mope::I_game_engine*;

namespace mope
{
    inline auto game_engine_create() -> std::unique_ptr<I_game_engine, void(*)(I_game_engine*)>
    {
        return std::unique_ptr<I_game_engine, void(*)(I_game_engine*)>{
            mope_game_engine_create(),
                // A more elegant way to achieve this would be to use an
                // override of delete(..., std::destroying_delete_t), but MSVC
                // still requires an accessible destructor for that. q.v.
                // https://developercommunity.visualstudio.com/t/Class-with-destroying-operator-delete-/11037237
                [](I_game_engine* engine)
                {
                    if (nullptr != engine) {
                        engine->destroy();
                    }
                }
        };
    }
}
