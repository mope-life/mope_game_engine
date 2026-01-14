#pragma once

#include "mope_game_engine/mope_game_engine_export.hxx"
#include "mope_game_engine/resource_id.hxx"

namespace mope::gl
{
    enum class MOPE_GAME_ENGINE_EXPORT pixel_format
    {
        r,
        rg,
        rgb,
        bgr,
        rgba,
        bgra,
    };

    class MOPE_GAME_ENGINE_EXPORT texture
    {
    public:
        void make(
            unsigned char const* bytes, pixel_format format, std::size_t width, std::size_t height
        );
        void bind();

    private:
        resource_id m_id;
    };
} // namespace mope::gl
