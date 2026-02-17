#pragma once

#include "mope_game_engine/game_engine_error.hxx"

#include "ft2build.h"
#include FT_FREETYPE_H

#include <string>

namespace mope
{
    template <typename T>
    concept appendable_to_string = requires (std::string s, T t)
    {
        s += t;
    };

    void check_ft_error(
        FT_Error err,
        appendable_to_string auto&& task,
        appendable_to_string auto&&... more)
    {
        if (FT_Err_Ok != err) {
            auto msg = std::string{ "[FreeType] Failed " };
            (msg += std::forward<decltype(task)>(task));
            if constexpr (sizeof...(more) > 0) {
                (msg += ... += std::forward<decltype(more)>(more));
            }
            msg += ". Error: ";
            msg += std::to_string(err);
            throw mope::game_engine_error{ msg };
        }
    }
}
