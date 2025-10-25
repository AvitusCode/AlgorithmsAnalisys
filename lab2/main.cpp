#include <algorithm>
#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "quick_sort.hpp"

std::vector<int> generateArray(size_t size)
{
    std::vector<int> arr(size);
    std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<> dis(1, 100000);

    std::generate(arr.begin(), arr.end(), [&dis, &gen] { return dis(gen); });

    return arr;
}

template <bool IsQuick>
void benchmarkSort(size_t size, size_t num_runs = 1000)
{
    double total_time{};

    for (size_t run = 0; run <= num_runs; ++run) {
        auto arr   = generateArray(size);
        auto start = std::chrono::high_resolution_clock::now();
        if constexpr (IsQuick) {
            jd::sort(arr.begin(), arr.end());
        } else {
            jd::insertionSort(arr.begin(), arr.end());
        }
        auto end = std::chrono::high_resolution_clock::now();

        if (!std::is_sorted(arr.begin(), arr.end())) {
            std::cerr << "Sort failed!" << std::endl;
            std::exit(1);
        }

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        if (run > 0) {
            total_time += duration.count();
        }
    }

    double average_time = total_time / num_runs;
    std::cout << static_cast<long>(average_time) << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc != 4) {
        std::cerr << "Usage: " << argv[0] << " <quicksort|insertionsort> <size> <probes>" << std::endl;
        return 1;
    }

    const std::string algorithm = argv[1];
    const size_t size           = std::stoul(argv[2]);
    const size_t probes         = std::stoul(argv[3]);

    if (algorithm == "quicksort") {
        benchmarkSort<true>(size, probes);
    } else if (algorithm == "insertionsort") {
        benchmarkSort<false>(size, probes);
    } else {
        std::cerr << "Unknown algorithm: " << algorithm << std::endl;
        std::cerr << "Use 'quicksort' or 'insertionsort'" << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
