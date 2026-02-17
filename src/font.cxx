#include "mope_game_engine/font.hxx"

#include "freetype.hxx"
#include "mope_game_engine/texture.hxx"
#include "mope_vec/mope_vec.hxx"

#include <cstddef>
#include <utility>

mope::font::font()
    : font{ nullptr }
{
}

mope::font::font(void* imp)
    : m_imp{ imp }
{
}

mope::font::font(font const& that)
    : font{ that.m_imp }
{
    if (nullptr != m_imp) {
        check_ft_error(
            FT_Reference_Face(static_cast<FT_Face>(m_imp)),
            "creating reference to font face"
        );
    }
}

mope::font::font(font&& that) noexcept
    : font{ that.m_imp }
{
    that.m_imp = nullptr;
}

auto mope::font::operator=(font that) noexcept -> font&
{
    swap(*this, that);
    return *this;
}

mope::font::~font() noexcept
{
    if (nullptr != m_imp) {
        FT_Done_Face(static_cast<FT_Face>(m_imp));
        m_imp = nullptr;
    }
}

void mope::font::set_px(unsigned int px_size)
{
    check_ft_error(
        FT_Set_Pixel_Sizes(
            static_cast<FT_Face>(m_imp),
            px_size,
            0),
        "setting glyph size"
    );
}

auto mope::font::make_glyph(unsigned long character_code) const -> glyph
{
    auto face = static_cast<FT_Face>(m_imp);
    check_ft_error(
        FT_Load_Char(face, character_code, FT_LOAD_RENDER),
        "loading character and rendering glyph"
    );

    auto buffer = reinterpret_cast<std::byte const*>(face->glyph->bitmap.buffer);
    auto size = vec2i{ vec2ui{ face->glyph->bitmap.width , face->glyph->bitmap.rows } };
    auto advance = vec2i{
        static_cast<int>(face->glyph->advance.x) >> 6,
        static_cast<int>(face->glyph->advance.y) >> 6
    };
    auto bearing = vec2i{
        face->glyph->bitmap_left, face->glyph->bitmap_top - size.y()
    };

    auto tex = gl::texture{}
        .make(
            buffer,
            size,
            mope::gl::pixel_format::r,
            gl::texture_extra_options{ .row_alignment = 1 })
        .swizzle({
            gl::color_component::one,
            gl::color_component::one,
            gl::color_component::one,
            gl::color_component::red });

    return glyph{
        .size = size,
        .advance = advance,
        .bearing = bearing,
        .texture = std::move(tex)
    };
}
