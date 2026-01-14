#pragma once

#include "mope_game_engine/mope_game_engine_export.hxx"

#include <stdexcept>

namespace mope
{
    class MOPE_GAME_ENGINE_EXPORT game_engine_error : public std::runtime_error
    {
        using runtime_error::runtime_error;
    };
}
