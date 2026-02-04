#pragma once

#include "mope_game_engine/component.hxx"

namespace mope
{
    struct I_logger : public singleton_component
    {
        virtual ~I_logger() = default;

        enum class log_level
        {
            error,
            warning,
            notification,
            debug,
        };

        virtual void log(char const* message, log_level level) const = 0;

        static constexpr auto level_string(log_level level) -> char const*
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
}