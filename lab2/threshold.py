import subprocess
import matplotlib.pyplot as plt
import numpy as np


def measure_sort_time(sort_function : str, array_size : int, num_tests=3, num_probes=1000):

    times = []
    for _ in range(num_tests):
        try:
            result = subprocess.run([
                './build/lab23',
                sort_function,
                str(array_size),
                str(num_probes)
            ], capture_output=True, text=True, timeout=30)

            if result.returncode == 0:
                time_us = int(result.stdout.strip())
                times.append(time_us)
            else:
                print(f"Error for {sort_function} size {array_size}: {result.stderr}")
                return float('inf')

        except subprocess.TimeoutExpired:
            print(f"Timeout for {sort_function} size {array_size}")
            return float('inf')
        except Exception as e:
            print(f"Exception for {sort_function} size {array_size}: {e}")
            return float('inf')

    return np.mean(times) if times else float('inf')


def find_intersection_point():
    sizes = list(range(5, 51, 5)) + list(range(60, 201, 10)) + list(range(250, 1001, 50)) + list(range(1100, 5001, 200))

    quick_times = []
    insertion_times = []
    valid_sizes = []

    print("Testing performance comparison...")
    print("Size\tQuickSort\tInsertionSort")
    print("-" * 40)

    for size in sizes:
        quick_time = measure_sort_time("quicksort", size, 5)
        insertion_time = measure_sort_time("insertionsort", size, 5)

        if quick_time < float('inf') and insertion_time < float('inf'):
            quick_times.append(quick_time)
            insertion_times.append(insertion_time)
            valid_sizes.append(size)
            print(f"{size}\t{quick_time:.1f}\t\t{insertion_time:.1f}")

        if size > 200 and insertion_time > quick_time * 5:
            print(f"Stopping at size {size} - Insertion Sort became too slow")
            break

    intersection_size = None
    for i in range(1, len(valid_sizes)):
        if insertion_times[i-1] <= quick_times[i-1] and insertion_times[i] >= quick_times[i]:
            x1, y1_i, y1_q = valid_sizes[i-1], insertion_times[i-1], quick_times[i-1]
            x2, y2_i, y2_q = valid_sizes[i], insertion_times[i], quick_times[i]

            m_i = (y2_i - y1_i) / (x2 - x1) if (x2 - x1) != 0 else 0
            m_q = (y2_q - y1_q) / (x2 - x1) if (x2 - x1) != 0 else 0

            if m_i != m_q:
                intersection_size = (y1_q - y1_i) / (m_i - m_q) + x1
                break

    plt.figure(figsize=(12, 8))
    plt.plot(valid_sizes, quick_times, 'b-', linewidth=2, marker='o', label='QuickSort')
    plt.plot(valid_sizes, insertion_times, 'r-', linewidth=2, marker='s', label='Insertion Sort')

    if intersection_size:
        plt.axvline(x=intersection_size, color='green', linestyle='--',
                   label=f'Optimal threshold: {intersection_size:.0f} elements')
        print(f"\nOptimal threshold: {intersection_size:.0f} elements")

    plt.xlabel('Array Size')
    plt.ylabel('Time (microseconds)')
    plt.title('QuickSort vs Insertion Sort Performance')
    plt.legend()
    plt.grid(True, alpha=0.3)
    plt.tight_layout()
    plt.savefig('sorting_comparison.png', dpi=150, bbox_inches='tight')
    plt.show()

    return intersection_size


if __name__ == "__main__":
    optimal_threshold = find_intersection_point()
    if optimal_threshold:
        print(f"\nRecommended INSERTION_SORT_THRESHOLD: {int(optimal_threshold)}")
    else:
        print("\nCould not determine optimal threshold")
