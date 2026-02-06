#pragma once

#include "mope_game_engine/game_engine_error.hxx"

#include "ft2build.h"
#include FT_FREETYPE_H

#include <string>

namespace mope
{
    template <typename... TaskDescription>
    void check_ft_error(FT_Error err, TaskDescription&&... task)
    {
        if (FT_Err_Ok != err) {
            auto msg = std::string{ "[FreeType] Failed " };
            (msg += ... += task);
            msg += ". Error: ";
            msg += std::to_string(err);
            throw mope::game_engine_error{ msg };
        }
    }
}
