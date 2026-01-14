#include "mope_game_engine/game_engine.hxx"
#include "pong.hxx"

#include <Windows.h>

#include <memory>
#include <sstream>
#include <string_view>

namespace
{
    class win_logger : public mope::I_logger
    {
        void log(std::string_view message, log_level level) const override
        {
            std::ostringstream output{};
            output << "[" << level_string(level) << "] " << message << '\n';
            OutputDebugStringA(output.str().c_str());
        }
    };
}

int WINAPI WinMain(
    _In_ HINSTANCE /*hInstance*/,
    _In_opt_ HINSTANCE /*hPrevInstance*/,
    _In_ LPSTR /*lpCmdLine*/,
    _In_ int /*nShowCmd*/
)
{
    return app_main(std::make_shared<win_logger>());
}
