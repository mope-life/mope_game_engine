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
            FT_Reference_Face(static_cast<FT_Face>(m_imp))
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

auto mope::font::make_glyph(unsigned long character_code) -> glyph
{
    auto face = static_cast<FT_Face>(m_imp);
    check_ft_error(
        FT_Load_Char(face, character_code, FT_LOAD_RENDER),
        "loading character and rendering glyph"
    );

    auto tex = gl::texture{}
        .make(
            reinterpret_cast<std::byte const*>(face->glyph->bitmap.buffer),
            vec2i{
                static_cast<int>(face->glyph->bitmap.width),
                static_cast<int>(face->glyph->bitmap.rows)
            },
            mope::gl::pixel_format::r,
            gl::texture_extra_options{
                .row_alignment = 1,
            })
        .swizzle({
            gl::color_component::one,
            gl::color_component::one,
            gl::color_component::one,
            gl::color_component::red });

    return glyph{
        .width = face->glyph->bitmap.width,
        .height = face->glyph->bitmap.rows,
        .horizontal_advance = face->glyph->advance.x,
        .vertical_advance = face->glyph->advance.y,
        .texture = std::move(tex)
    };
}
