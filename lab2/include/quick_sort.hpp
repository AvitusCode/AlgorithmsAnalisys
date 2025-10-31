#include <functional>
#include <iterator>
#include <utility>

#define INSERTION_THRESHOLD 16
#define JD_TEST

namespace jd
{
template <typename RandomIt>
RandomIt selectPivot(RandomIt first, RandomIt last)
{
    RandomIt mid       = first + (last - first) / 2;
    RandomIt last_iter = last - 1;

    if (*first < *mid) {
        if (*mid < *last_iter) {
            return mid;
        } else if (*first < *last_iter) {
            return last_iter;
        }

        return first;
    } else {
        if (*last_iter < *mid) {
            return mid;
        } else if (*last_iter < *first) {
            return last_iter;
        }

        return first;
    }
}

template <typename RandomIt, typename Compare>
RandomIt partitionHoare(RandomIt first, RandomIt last, Compare cmp)
{
    RandomIt pivot_it = selectPivot(first, last);
    auto pivot        = *pivot_it;

    RandomIt left  = first - 1;
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

template <typename RandomIt, typename Compare = std::less<typename std::iterator_traits<RandomIt>::value_type>>
void insertionSort(RandomIt first, RandomIt last, Compare cmp = Compare{})
{
    using DistT = typename std::iterator_traits<RandomIt>::difference_type;

    const DistT dist = last - first;
    if (dist <= 1)
        return;

    // first "min" element optimization
    RandomIt min_it = first;
    for (RandomIt it = first + 1; it != last; ++it) {
        if (cmp(*it, *min_it)) {
            min_it = it;
        }
    }
    std::iter_swap(first, min_it);

    for (DistT i = 2; i < dist; ++i) {
        auto key = std::move(first[i]);
        DistT j  = i;

        while (cmp(key, first[j - 1])) {
            std::iter_swap(first + j, first + j - 1);
            --j;
        }

        first[j] = std::move(key);
    }
}

template <typename RandomIt, typename Compare = std::less<typename std::iterator_traits<RandomIt>::value_type>>
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
        RandomIt pivot = partitionHoare(first, last, cmp);

        if (pivot - first < last - (pivot + 1)) {
            jd::sort(first, pivot + 1, cmp);
            first = pivot + 1;
        } else {
            jd::sort(pivot + 1, last, cmp);
            last = pivot + 1;
        }
    }
}
} // namespace jd
