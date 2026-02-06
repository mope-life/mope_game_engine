#pragma once

#include "mope_game_engine/texture.hxx"

#include <utility>

namespace mope
{
    struct glyph final
    {
        unsigned int width;
        unsigned int height;
        long horizontal_advance;
        long vertical_advance;
        gl::texture texture;
    };

    struct font final
    {
        font();
        font(void* imp);
        font(font const&);
        font(font&&) noexcept;
        ~font() noexcept;

        auto operator=(font) noexcept -> font&;

        void set_px(unsigned int px_size);
        auto make_glyph(unsigned long character_code) -> glyph;

        friend void swap(font& a, font& b) noexcept
        {
            using std::swap;
            swap(a.m_imp, b.m_imp);
        }

    private:
        void* m_imp;
    };
}
