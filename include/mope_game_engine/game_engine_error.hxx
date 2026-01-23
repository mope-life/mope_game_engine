#pragma once

#include <stdexcept>

namespace mope
{
    class game_engine_error : public std::runtime_error
    {
        using runtime_error::runtime_error;
    };
}
