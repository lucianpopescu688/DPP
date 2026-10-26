// =============================================================================
// DPP Laboratory 2 - In-Place Convolution with Barrier Synchronization
// Language: C++26 | Compiled with: g++ -std=c++2c -O3 -pthread
// =============================================================================

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <array>
#include <span>
#include <format>
#include <thread>
#include <barrier>
#include <chrono>
#include <algorithm>

// --- CONSTANTS ---
// Fixed 3x3 sharpening kernel (constexpr for compile-time evaluation)
constexpr std::array<std::array<int, 3>, 3> KERNEL = {{
    {{ 0, -1,  0}},
    {{-1,  5, -1}},
    {{ 0, -1,  0}}
}};

// --- MEMORY MANAGEMENT ---
// Allocate NxM matrix on the heap (dynamic allocation only, as required)
auto allocate_matrix(int N, int M) -> int** {
    auto** matrix = new int*[N];
    for (int i = 0; i < N; i++) {
        matrix[i] = new int[M]();
    }
    return matrix;
}

// Free 2D heap-allocated matrix
void free_matrix(int** matrix, int N) {
    if (!matrix) return;
    for (int i = 0; i < N; i++) {
        delete[] matrix[i];
    }
    delete[] matrix;
}

// --- FILE I/O & VERIFICATION ---
// Reads matrix dimensions and data from file (format: "N M\n <data>")
auto read_matrix(const std::string& filename, int& N, int& M) -> int** {
    std::ifstream fin(filename);
    if (!fin) {
        // Try parent directory fallback (for scripts that run from subdirectory)
        fin.open("../" + filename);
        if (!fin) return nullptr;
    }
    if (!(fin >> N >> M)) return nullptr;
    auto** matrix = allocate_matrix(N, M);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            fin >> matrix[i][j];
        }
    }
    return matrix;
}

// Writes matrix with dimensions header to output file
void write_matrix(const std::string& filename, int** matrix, int N, int M) {
    std::ofstream fout(filename);
    fout << N << ' ' << M << '\n';
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            fout << matrix[i][j] << (j + 1 < M ? " " : "");
        }
        fout << '\n';
    }
}

// Verifies two output files match value-by-value without allocating an extra matrix
// (Complies with O(M) space constraint - no second matrix in memory)
auto verify_files_equal(const std::string& file1, const std::string& file2) -> bool {
    std::ifstream f1(file1), f2(file2);
    if (!f1.is_open() || !f2.is_open()) return false;
    int n1, m1, n2, m2;
    if (!(f1 >> n1 >> m1) || !(f2 >> n2 >> m2)) return false;
    if (n1 != n2 || m1 != m2) return false;
    int val1, val2;
    while ((f1 >> val1) && (f2 >> val2)) {
        if (val1 != val2) return false;
    }
    return true;
}

// --- CONVOLUTION HELPER ---
// Applies the 3x3 kernel to a single interior pixel using span views
constexpr auto apply_kernel(std::span<const int> prev,
                            std::span<const int> curr,
                            std::span<const int> next, int j) -> int {
    return prev[j - 1] * KERNEL[0][0] + prev[j] * KERNEL[0][1] + prev[j + 1] * KERNEL[0][2] +
           curr[j - 1] * KERNEL[1][0] + curr[j] * KERNEL[1][1] + curr[j + 1] * KERNEL[1][2] +
           next[j - 1] * KERNEL[2][0] + next[j] * KERNEL[2][1] + next[j + 1] * KERNEL[2][2];
}

// --- SEQUENTIAL IN-PLACE CONVOLUTION (O(M) AUXILIARY SPACE) ---
// Processes rows top-to-bottom, keeping only prev_row and curr_row as 1D buffers.
// Each computed row replaces the original row in-place.
void sequential_convolution(int N, int M, int** matrix) {
    if (N < 3 || M < 3) return;

    auto* prev_row = new int[M];
    auto* curr_row = new int[M];

    std::copy_n(matrix[0], M, prev_row);
    std::copy_n(matrix[1], M, curr_row);

    for (int i = 1; i < N - 1; i++) {
        std::span<const int> prev(prev_row, M);
        std::span<const int> curr(curr_row, M);
        std::span<const int> next(matrix[i + 1], M);

        auto* computed_row = new int[M];
        for (int j = 1; j < M - 1; j++) {
            computed_row[j] = apply_kernel(prev, curr, next, j);
        }
        // Border pixels are preserved (not filtered)
        computed_row[0] = curr_row[0];
        computed_row[M - 1] = curr_row[M - 1];

        // Replace row in-place
        delete[] matrix[i];
        matrix[i] = computed_row;

        // Shift buffers forward
        if (i > 1) delete[] prev_row;
        prev_row = curr_row;
        curr_row = new int[M];
        std::copy_n(matrix[i + 1], M, curr_row);
    }
    delete[] prev_row;
    delete[] curr_row;
}

// --- PARALLEL WORKER (VARIANT 2B - BARRIER + DELAYED BOUNDARY WRITE) ---
// Each thread computes its assigned rows using O(M) auxiliary space.
// Boundary rows (shared with adjacent threads) are buffered and written only
// after all threads synchronize at the barrier, preventing data races.
void parallel_worker(int id, int P, int N, int M, int** matrix, std::barrier<>& sync_barrier) {
    // Compute row range for this thread (horizontal line-based decomposition)
    const int inner_rows = N - 2;
    const int start_row  = 1 + id * inner_rows / P;
    const int end_row    = (id == P - 1) ? N - 1 : 1 + (id + 1) * inner_rows / P;

    if (start_row >= end_row) {
        sync_barrier.arrive_and_wait();
        return;
    }

    // Allocate 1D temporary arrays (Strictly O(M) auxiliary space per thread)
    auto* prev_row   = new int[M]();
    auto* curr_row   = new int[M]();
    auto* new_top    = new int[M]();
    auto* new_bottom = new int[M]();

    if (start_row > 0) {
        std::copy_n(matrix[start_row - 1], M, prev_row);
    }
    std::copy_n(matrix[start_row], M, curr_row);

    // PHASE 1: COMPUTE all rows in this thread's range
    for (int i = start_row; i < end_row; i++) {
        auto* next_row = new int[M]();
        if (i + 1 < N) {
            std::copy_n(matrix[i + 1], M, next_row);
        }

        // Apply convolution kernel to interior pixels
        auto* computed_row = new int[M]();
        std::span<const int> prev(prev_row, M);
        std::span<const int> curr(curr_row, M);
        std::span<const int> next(next_row, M);

        for (int j = 1; j < M - 1; j++) {
            computed_row[j] = apply_kernel(prev, curr, next, j);
        }
        computed_row[0] = curr_row[0];
        computed_row[M - 1] = curr_row[M - 1];

        // Determine if this row is at the boundary between two threads
        const bool is_top_boundary    = (i == start_row && id > 0);
        const bool is_bottom_boundary = (i == end_row - 1 && id < P - 1);

        if (is_top_boundary && is_bottom_boundary) {
            // Single row belongs to both boundaries - save to top buffer
            std::copy_n(computed_row, M, new_top);
            delete[] computed_row;
        } else if (is_top_boundary) {
            std::copy_n(computed_row, M, new_top);
            delete[] computed_row;
        } else if (is_bottom_boundary) {
            std::copy_n(computed_row, M, new_bottom);
            delete[] computed_row;
        } else {
            // Interior row: write directly to matrix (safe, no adjacent thread reads this)
            std::copy_n(computed_row, M, matrix[i]);
            delete[] computed_row;
        }

        delete[] prev_row;
        prev_row = curr_row;
        curr_row = next_row;
    }
    delete[] prev_row;
    delete[] curr_row;

    // PHASE 2: SYNCHRONIZATION BARRIER - wait for all threads to finish computing
    sync_barrier.arrive_and_wait();

    // PHASE 3: WRITE BOUNDARY ROWS - safe now since all threads have read their data
    const bool is_top_boundary    = (id > 0);
    const bool is_bottom_boundary = (id < P - 1);

    if (is_top_boundary && is_bottom_boundary && (end_row - start_row == 1)) {
        std::copy_n(new_top, M, matrix[start_row]);
    } else {
        if (is_top_boundary) {
            std::copy_n(new_top, M, matrix[start_row]);
        }
        if (is_bottom_boundary) {
            std::copy_n(new_bottom, M, matrix[end_row - 1]);
        }
    }

    delete[] new_top;
    delete[] new_bottom;
}

// Parallel in-place convolution coordinator
void parallel_convolution(int N, int M, int** matrix, int P) {
    if (N < 3 || M < 3) return;
    const int activeP = std::min(P, N - 2);
    if (activeP <= 1) {
        sequential_convolution(N, M, matrix);
        return;
    }

    std::barrier sync_barrier(activeP);
    std::vector<std::jthread> threads;
    threads.reserve(activeP);

    for (int i = 0; i < activeP; i++) {
        threads.emplace_back(parallel_worker, i, activeP, N, M, matrix, std::ref(sync_barrier));
    }
    // std::jthread auto-joins on destruction - no manual join loop needed
}

// Measure execution time in milliseconds using high-resolution clock
template <typename Func>
auto measure_ms(Func&& fn) -> double {
    const auto t0 = std::chrono::high_resolution_clock::now();
    fn();
    const auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// --- MAIN ENTRY ---
int main(int argc, char** argv) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string mode = "all";
    int P = 4;
    std::string filename = "date.txt";

    // Flexible command-line parsing:
    // Syntax 1: ./main <threads> [date.txt]
    // Syntax 2: ./main <mode> <threads> [date.txt]
    if (argc >= 2) {
        std::string arg1 = argv[1];
        if (std::isdigit(arg1[0])) {
            P = std::stoi(arg1);
            mode = (P == 1) ? "sequential" : "parallel";
            if (argc >= 3) filename = argv[2];
        } else {
            mode = arg1;
            if (argc >= 3) P = std::stoi(argv[2]);
            if (argc >= 4) filename = argv[3];
        }
    }

    int N = 0, M = 0;
    auto** matrix = read_matrix(filename, N, M);
    if (!matrix) {
        std::cerr << std::format("Error: Could not read {}\n", filename);
        return 1;
    }

    // MODE: all / verify (For live demonstration to the lab assistant)
    if (mode == "all" || mode == "verify") {
        std::cout << std::format("Matrix: {} x {} | Kernel: 3 x 3 | Threads: {} (In-Place O(M) Aux Space)\n", N, M, P);
        std::cout << "--------------------------------------------------\n";

        // 1. Run Sequential
        const double msSeq = measure_ms([&]() { sequential_convolution(N, M, matrix); });
        std::cout << std::format("[Sequential]          Time: {:.4f} ms\n", msSeq);
        write_matrix("output_seq.txt", matrix, N, M);
        write_matrix("output.txt", matrix, N, M);
        free_matrix(matrix, N);

        // 2. Re-read original matrix and run Parallel (Variant 2B)
        matrix = read_matrix(filename, N, M);
        const double msPar = measure_ms([&]() { parallel_convolution(N, M, matrix, P); });
        write_matrix("output.txt", matrix, N, M);

        const bool match = verify_files_equal("output_seq.txt", "output.txt");
        const double speedup = (msPar > 0) ? (msSeq / msPar) : 1.0;
        std::cout << std::format("[Parallel (Barrier)]  Time: {:.4f} ms | Match: {} | Speedup: {:.2f}x\n",
                                 msPar, match ? "true" : "false", speedup);
        std::cout << "--------------------------------------------------\n";
        std::cout << "Output saved to output.txt\n";

        free_matrix(matrix, N);
        return 0;
    }

    // MODE: Single execution (Used by automated benchmark script run_benchmark.ps1)
    const double duration = measure_ms([&]() {
        if (mode == "sequential" || P == 1) {
            sequential_convolution(N, M, matrix);
        } else {
            parallel_convolution(N, M, matrix, P);
        }
    });

    write_matrix("output.txt", matrix, N, M);
    if (mode == "sequential" || P == 1) {
        write_matrix("output_seq.txt", matrix, N, M);
    }

    // If sequential baseline exists, verify correctness on-the-fly
    if (std::ifstream fseq("output_seq.txt"); fseq.good()) {
        fseq.close();
        if (!verify_files_equal("output_seq.txt", "output.txt")) {
            std::cerr << "CORRECTNESS ERROR: Result does not match output_seq.txt!\n";
            free_matrix(matrix, N);
            return 1;
        }
    }

    // Output pure execution time in ms (for automated script parsing)
    std::cout << std::format("{:.4f}\n", duration);

    free_matrix(matrix, N);
    return 0;
}
