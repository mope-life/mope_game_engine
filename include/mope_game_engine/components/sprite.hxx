#pragma once

#include "mope_game_engine/component.hxx"
#include "mope_game_engine/texture.hxx"

#include <utility>

namespace mope
{
    struct sprite_component : public entity_component
    {
        sprite_component(entity en, gl::texture texture)
            : entity_component{ en }
            , texture{ std::move(texture) }
        { }

        gl::texture texture;
    };
} // namespace mope
