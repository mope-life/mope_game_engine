#pragma once

#include <ranges>
#include <iterator>

namespace mope
{
    /// A viewable, non-owning range over a single item.
    ///
    /// If the pointed to item is null, the range is empty.
    ///
    /// This is spiritually the same as this (given `T* ptr`):
    /// ```cpp
    ///     nullptr == ptr
    ///         ? std::span<T>{ }
    ///         : std::span<T>{ ptr, 1 };
    /// ```
    ///
    /// with the notable exception that MSVC refuses to compile the above for abstract classes.
    /// This could be a compiler bug, but I need to look into it
    template <typename T>
    class iterable_box : public std::ranges::view_interface<iterable_box<T>>
    {
    public:
        iterable_box(T* t)
            : t{ t }
        {
        }

        struct iterator
        {
            using difference_type = std::ptrdiff_t;
            using value_type = T;

            iterator()
                : t{ nullptr }
            {
            }

            iterator(T* t)
                : t{ t }
            {
            }

            auto operator*() const -> T&
            {
                return *t;
            }

            auto operator->() const -> T*
            {
                return t;
            }

            auto operator++() -> iterator&
            {
                // After one increment, the box is empty.
                t = nullptr;
                return *this;
            }

            auto operator++(int) -> iterator
            {
                auto prev = *this;
                ++*this;
                return prev;
            }

            auto operator-(iterator const& that) const -> difference_type
            {
                // We're allowed to assume that both iterators come from the same box:
                // therefore they are either both `t`, both nullptr, or 1 apart.
                return t == that.t ? 0 : t == nullptr ? -1 : 1;

                // here's a fun expression that it's probably better not to use for sanity.
                // return std::ptrdiff_t{ t != that.t } * (1 - 2 * std::ptrdiff_t{ nullptr == t });
            }

            auto operator==(iterator const& that) const -> bool
            {
                return t == that.t;
            }

            T* t;
        };

        auto begin() -> iterator
        {
            return iterator{ t };
        }

        auto end() -> iterator
        {
            return iterator{ nullptr };
        }

    private:
        T* t;

        static_assert(std::forward_iterator<iterable_box<T>::iterator>);
        static_assert(std::ranges::forward_range<iterable_box<T>>);
        static_assert(std::ranges::borrowed_range<iterable_box<T>>);
        static_assert(std::ranges::sized_range<iterable_box<T>>);
        static_assert(std::ranges::view<iterable_box<T>>);
    };

}

template <typename T>
constexpr bool std::ranges::enable_borrowed_range<mope::iterable_box<T>> = true;
