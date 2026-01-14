#include "mope_game_engine/game_engine.hxx"
#include "pong.hxx"

#include <iostream>
#include <memory>

namespace
{
    class logger : public mope::I_logger
    {
        void log(std::string_view message, log_level level) const override
        {
            std::cout << "[" << level_string(level) << "] " << message << '\n';
        }
    };
}

int main(int, char* [])
{
    return app_main(std::make_shared<logger>());
}
