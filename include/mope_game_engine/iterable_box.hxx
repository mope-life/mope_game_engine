#pragma once

#include <ranges>
#include <iterator>

template <typename T>
class iterable_box;

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

        auto operator++() -> iterator&
        {
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
};

template <typename T>
constexpr bool std::ranges::enable_borrowed_range<iterable_box<T>> = true;

static_assert(std::forward_iterator<iterable_box<int>::iterator>);
static_assert(std::ranges::forward_range<iterable_box<int>>);
static_assert(std::ranges::borrowed_range<iterable_box<int>>);
static_assert(std::ranges::sized_range<iterable_box<int>>);
static_assert(std::ranges::view<iterable_box<int>>);
