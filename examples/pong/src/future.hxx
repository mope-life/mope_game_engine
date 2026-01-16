#pragma once

#include <ranges>

namespace future
{
    /// TODO: Pending Ubuntu 26.04 LTS for GCC >= 15.1.
    template <typename T, std::ranges::input_range R>
    void assign_range(std::vector<T>& v, R&& r)
    {
#if defined(__cpp_lib_containers_ranges)
        v.assign_range(std::forward<R>(r));
#else
        if constexpr (!std::ranges::common_range<std::remove_cvref_t<R>>) {
            assign_range(v, std::forward<R>(r) | std::views::common);
        }
        else {
            v.assign(r.begin(), r.end());
        }
#endif
    }
}
