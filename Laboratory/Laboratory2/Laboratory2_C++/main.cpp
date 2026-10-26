#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <thread>
#include <barrier>
#include <chrono>
#include <algorithm>
#include <format>

// --- CONSTANTS ---
// Standard 3x3 sharpening kernel: center = 5, orthogonal neighbors = -1
const int KERNEL[3][3] = {
    { 0, -1,  0},
    {-1,  5, -1},
    { 0, -1,  0}
};

// --- MEMORY MANAGEMENT ---
int** allocate_matrix(int N, int M) {
    auto** matrix = new int*[N];
    for (int i = 0; i < N; i++) matrix[i] = new int[M]();
    return matrix;
}

void free_matrix(int** matrix, int N) {
    if (!matrix) return;
    for (int i = 0; i < N; i++) delete[] matrix[i];
    delete[] matrix;
}

// --- FILE I/O ---
// Searches current directory, parent directory, and grandparent directory
int** read_matrix(const std::string& filename, int& N, int& M) {
    std::ifstream fin(filename);
    if (!fin) fin.open("../" + filename);
    if (!fin) fin.open("../../" + filename);
    if (!fin) return nullptr;

    if (!(fin >> N >> M)) return nullptr;
    auto** matrix = allocate_matrix(N, M);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) fin >> matrix[i][j];
    }
    return matrix;
}

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

// Verify output files match byte-for-byte without extra matrix allocation
bool verify_files_equal(const std::string& file1, const std::string& file2) {
    std::ifstream f1(file1), f2(file2);
    if (!f1) f1.open("../" + file1);
    if (!f2) f2.open("../" + file2);
    if (!f1 || !f2) return false;

    int n1, m1, n2, m2;
    if (!(f1 >> n1 >> m1) || !(f2 >> n2 >> m2)) return false;
    if (n1 != n2 || m1 != m2) return false;

    int v1, v2;
    while ((f1 >> v1) && (f2 >> v2)) {
        if (v1 != v2) return false;
    }
    return true;
}

// --- CONVOLUTION HELPER ---
inline int apply_kernel(const int* prev, const int* curr, const int* next, int j) {
    return prev[j - 1] * KERNEL[0][0] + prev[j] * KERNEL[0][1] + prev[j + 1] * KERNEL[0][2] +
           curr[j - 1] * KERNEL[1][0] + curr[j] * KERNEL[1][1] + curr[j + 1] * KERNEL[1][2] +
           next[j - 1] * KERNEL[2][0] + next[j] * KERNEL[2][1] + next[j + 1] * KERNEL[2][2];
}

// --- SEQUENTIAL IN-PLACE CONVOLUTION (O(M) AUXILIARY SPACE) ---
void sequential_convolution(int N, int M, int** matrix) {
    if (N < 3 || M < 3) return;

    auto* prev = new int[M];
    auto* curr = new int[M];
    auto* comp = new int[M];

    std::copy_n(matrix[0], M, prev);
    std::copy_n(matrix[1], M, curr);

    for (int i = 1; i < N - 1; i++) {
        const int* next = matrix[i + 1];
        for (int j = 1; j < M - 1; j++) {
            comp[j] = apply_kernel(prev, curr, next, j);
        }
        comp[0] = curr[0];
        comp[M - 1] = curr[M - 1];

        // Shift buffers forward before writing in-place
        std::copy_n(curr, M, prev);
        std::copy_n(next, M, curr);
        std::copy_n(comp, M, matrix[i]);
    }

    delete[] prev;
    delete[] curr;
    delete[] comp;
}

// --- PARALLEL WORKER (VARIANT 2B - BARRIER + DELAYED BOUNDARY WRITE) ---
void parallel_worker(int id, int P, int N, int M, int** matrix, std::barrier<>& sync_barrier) {
    int inner_rows = N - 2;
    int start_row  = 1 + id * inner_rows / P;
    int end_row    = (id == P - 1) ? N - 1 : 1 + (id + 1) * inner_rows / P;

    if (start_row >= end_row) {
        sync_barrier.arrive_and_wait();
        return;
    }

    // 1D temporary arrays allocated ONCE per thread (Strictly O(M) auxiliary space)
    auto* prev       = new int[M]();
    auto* curr       = new int[M]();
    auto* comp       = new int[M]();
    auto* new_top    = new int[M]();
    auto* new_bottom = new int[M]();

    if (start_row > 0) std::copy_n(matrix[start_row - 1], M, prev);
    std::copy_n(matrix[start_row], M, curr);

    // PHASE 1: COMPUTE all assigned rows
    for (int i = start_row; i < end_row; i++) {
        const int* next = (i + 1 < N) ? matrix[i + 1] : nullptr;

        for (int j = 1; j < M - 1; j++) {
            comp[j] = apply_kernel(prev, curr, next, j);
        }
        comp[0] = curr[0];
        comp[M - 1] = curr[M - 1];

        // Boundary rows are saved in buffers; interior rows written directly
        if (i == start_row && id > 0) {
            std::copy_n(comp, M, new_top);
        } else if (i == end_row - 1 && id < P - 1) {
            std::copy_n(comp, M, new_bottom);
        } else {
            std::copy_n(comp, M, matrix[i]);
        }

        if (i + 1 < end_row) {
            std::copy_n(curr, M, prev);
            std::copy_n(next, M, curr);
        }
    }

    // PHASE 2: SYNCHRONIZE
    sync_barrier.arrive_and_wait();

    // PHASE 3: WRITE BOUNDARY ROWS (safe after barrier)
    if (id > 0) {
        std::copy_n(new_top, M, matrix[start_row]);
    }
    if (id < P - 1 && (end_row - 1 != start_row || id == 0)) {
        std::copy_n(new_bottom, M, matrix[end_row - 1]);
    }

    delete[] prev;
    delete[] curr;
    delete[] comp;
    delete[] new_top;
    delete[] new_bottom;
}

// Parallel in-place convolution coordinator
void parallel_convolution(int N, int M, int** matrix, int P) {
    if (N < 3 || M < 3) return;
    int activeP = std::min(P, N - 2);
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
    // std::jthread auto-joins upon destruction
}

// Measure execution time in milliseconds
template <typename Func>
double measure_ms(Func&& fn) {
    auto t0 = std::chrono::high_resolution_clock::now();
    fn();
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

// --- MAIN ENTRY ---
int main(int argc, char** argv) {
    std::ios_base::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string mode = "all";
    int P = 4;
    std::string filename = "date.txt";

    // Clean, flexible argument parsing
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (std::isdigit(arg[0])) P = std::stoi(arg);
        else if (arg == "sequential" || arg == "parallel" || arg == "all" || arg == "verify") mode = arg;
        else if (arg.ends_with(".txt")) filename = arg;
    }
    if (P == 1 && mode != "all") mode = "sequential";

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

        // 1. Sequential run
        double msSeq = measure_ms([&]() { sequential_convolution(N, M, matrix); });
        std::cout << std::format("[Sequential]          Time: {:.4f} ms\n", msSeq);
        write_matrix("output_seq.txt", matrix, N, M);
        write_matrix("output.txt", matrix, N, M);
        free_matrix(matrix, N);

        // 2. Parallel run
        matrix = read_matrix(filename, N, M);
        double msPar = measure_ms([&]() { parallel_convolution(N, M, matrix, P); });
        write_matrix("output.txt", matrix, N, M);

        bool match = verify_files_equal("output_seq.txt", "output.txt");
        double speedup = (msPar > 0) ? (msSeq / msPar) : 1.0;
        std::cout << std::format("[Parallel (Barrier)]  Time: {:.4f} ms | Match: {} | Speedup: {:.2f}x\n",
                                 msPar, match ? "true" : "false", speedup);
        std::cout << "--------------------------------------------------\n";
        std::cout << "Output saved to output.txt\n";

        free_matrix(matrix, N);
        return 0;
    }

    // MODE: Single execution (Used by automated benchmark script)
    double duration = measure_ms([&]() {
        if (mode == "sequential" || P == 1) {
            sequential_convolution(N, M, matrix);
        } else {
            parallel_convolution(N, M, matrix, P);
        }
    });

    write_matrix("output.txt", matrix, N, M);
    if (mode == "sequential" || P == 1) write_matrix("output_seq.txt", matrix, N, M);

    // Verify against output_seq.txt if present
    std::string seq_file = "output_seq.txt";
    if (!std::ifstream(seq_file).good() && std::ifstream("../" + seq_file).good()) seq_file = "../" + seq_file;
    if (std::ifstream(seq_file).good() && !verify_files_equal(seq_file, "output.txt")) {
        std::cerr << "CORRECTNESS ERROR: Result does not match output_seq.txt!\n";
        free_matrix(matrix, N);
        return 1;
    }

    std::cout << std::format("{:.4f}\n", duration);
    free_matrix(matrix, N);
    return 0;
}
