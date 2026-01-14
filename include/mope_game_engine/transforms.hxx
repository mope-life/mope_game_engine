#pragma once

#include "mope_game_engine/mope_game_engine_export.hxx"
#include "mope_vec/mope_vec.hxx"

namespace mope::gl
{
    constexpr auto scale_matrix(vec3f scale) -> mat4f
    {
        mope::mat4f m = mope::mat4f::identity();
        for (std::size_t i = 0; i < 3; ++i) {
            m[i][i] = scale[i];
        }
        return m;
    }

    constexpr auto translation_matrix(vec3f offsets) -> mat4f
    {
        mat4f m = mat4f::identity();
        for (std::size_t i = 0; i < 3; ++i) {
            m[3][i] = offsets[i];
        }
        return m;
    }

    constexpr auto orthographic_projection_matrix(
        float left, float right, float top, float bottom, float near, float far
    ) -> mat4f
    {
        mat4f s = scale_matrix(
            { 2.f / (right - left), -2.f / (bottom - top), -2.f / (far - near) }
        );
        mat4f t = translation_matrix(
            { -(right + left) / 2.f, -(bottom + top) / 2.f, -(far + near) / 2.f }
        );
        return s * t;
    }
} // namespace mope::gl
