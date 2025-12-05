#include <concepts>
#include <functional>
#include <iterator>
#include <utility>

#define INSERTION_THRESHOLD 35
#define JD_TEST

namespace jd
{
template <typename It, typename Compare>
concept Sortable = std::random_access_iterator<It> && std::invocable<Compare, std::iter_value_t<It>, std::iter_value_t<It>> &&
                   std::predicate<Compare, std::iter_value_t<It>, std::iter_value_t<It>>;

template <typename RandomIt, typename Compare>
RandomIt selectPivot(RandomIt first, RandomIt last, const Compare& cmp)
{
    RandomIt mid       = first + (last - first) / 2;
    RandomIt last_iter = std::prev(last);

    if (*first < *mid) {
        if (cmp(*mid, *last_iter)) {
            return mid;
        } else if (cmp(*first, *last_iter)) {
            return last_iter;
        }

        return first;
    } else {
        if (cmp(*last_iter, *mid)) {
            return mid;
        } else if (cmp(*last_iter, *first)) {
            return last_iter;
        }

        return first;
    }
}

template <typename RandomIt, typename Compare>
RandomIt partitionHoare(RandomIt first, RandomIt last, const Compare& cmp)
{
    RandomIt pivot_it = selectPivot(first, last, cmp);
    auto pivot        = *pivot_it;

    RandomIt left  = std::prev(first);
    RandomIt right = last;

    while (true) {
        do {
            ++left;
        } while (cmp(*left, pivot));

        do {
            --right;
        } while (cmp(pivot, *right));

        if (left >= right) {
            return right;
        }

        std::iter_swap(left, right);
    }
}

template <typename RandomIt, typename Compare = std::less<std::iter_value_t<RandomIt>>>
requires Sortable<RandomIt, Compare>
void insertionSort(RandomIt first, RandomIt last, Compare cmp = Compare{})
{
    if (first == last) [[unlikely]]
        return;

    for (RandomIt it = std::next(first); it != last; ++it) {
        auto temp          = std::move(*it);
        RandomIt shift_pos = it;

        while (shift_pos != first) {
            RandomIt prev_pos = shift_pos - 1;
            if (!cmp(temp, *prev_pos)) {
                break;
            }

            *shift_pos = std::move(*prev_pos);
            shift_pos  = prev_pos;
        }

        *shift_pos = std::move(temp);
    }
}

template <typename RandomIt, typename Compare = std::less<std::iter_value_t<RandomIt>>>
requires Sortable<RandomIt, Compare>
void sort(RandomIt first, RandomIt last, Compare cmp = {})
{
    while (last - first > 1) {
#ifdef JD_TEST
        if (last - first == 2) {
            if (cmp(*(first + 1), *first)) {
                std::iter_swap(first, first + 1);
            }
            return;
        }
#else
        if (last - first <= INSERTION_THRESHOLD) {
            insertionSort(first, last, cmp);
            return;
        }
#endif
        RandomIt pivot_it = partitionHoare(first, last, cmp);

        if (pivot_it - first < last - (pivot_it + 1)) {
            jd::sort(first, pivot_it + 1, cmp);
            first = pivot_it + 1;
        } else {
            jd::sort(pivot_it + 1, last, cmp);
            last = pivot_it + 1;
        }
    }
}
} // namespace jd
