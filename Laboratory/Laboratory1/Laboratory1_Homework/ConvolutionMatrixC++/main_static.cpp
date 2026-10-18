#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>
#include <functional>

const int MAX_N = 10005;
const int MAX_M = 10005;
const int MAX_K = 11;

// Statically allocated matrices in the .bss segment to prevent stack overflow
static int F[MAX_N][MAX_M];
static int C[MAX_K][MAX_K];
static int V_seq[MAX_N][MAX_M];
static int V_par[MAX_N][MAX_M];
static int V_check[MAX_N][MAX_M];

// Resolves file path from either the current directory or the parent directory
std::string resolveFile(const std::string& filename) {
    if (std::ifstream(filename).good()) return filename;
    std::string parent = "../" + filename;
    if (std::ifstream(parent).good()) return parent;
    return filename;
}

// Reads a 2D matrix into a statically allocated array
void readMatrix(const std::string& filename, int matrix[MAX_N][MAX_M], int& rows, int& cols) {
    std::ifstream file(resolveFile(filename));
    std::string line;
    if (!std::getline(file, line)) {
        rows = cols = 0;
        return;
    }
    std::stringstream ss(line);
    ss >> rows;
    if (!(ss >> cols)) cols = rows;

    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            file >> matrix[i][j];
        }
    }
}

// Reads the square convolution kernel C(k, k)
void readKernel(const std::string& filename, int kernel[MAX_K][MAX_K], int& k) {
    std::ifstream file(resolveFile(filename));
    std::string line;
    if (!std::getline(file, line)) {
        k = 0;
        return;
    }
    std::stringstream ss(line);
    ss >> k;
    for (int i = 0; i < k; i++) {
        for (int j = 0; j < k; j++) {
            file >> kernel[i][j];
        }
    }
}

// Writes a statically allocated matrix to an output text file
void writeMatrix(const std::string& filename, int matrix[MAX_N][MAX_M], int rows, int cols) {
    std::ofstream file(filename);
    file << rows << " " << cols << "\n";
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            file << matrix[i][j] << (j + 1 < cols ? " " : "");
        }
        file << "\n";
    }
}

// Verifies that two statically allocated matrices have identical dimensions and values
bool areEqual(int a[MAX_N][MAX_M], int b[MAX_N][MAX_M], int rowsA, int colsA, int rowsB, int colsB) {
    if (rowsA != rowsB || colsA != colsB) return false;
    for (int i = 0; i < rowsA; i++) {
        for (int j = 0; j < colsA; j++) {
            if (a[i][j] != b[i][j]) return false;
        }
    }
    return true;
}

// Computes the convolution for a single cell (i, j) using clamped virtual borders
int computeConvolution(int i, int j, int n, int m, int radius) {
    int sum = 0;
    for (int a = -radius; a <= radius; a++) {
        int r = std::clamp(i + a, 0, n - 1);
        for (int b = -radius; b <= radius; b++) {
            int c = std::clamp(j + b, 0, m - 1);
            sum += F[r][c] * C[a + radius][b + radius];
        }
    }
    return sum;
}

// Computes convolution over the rectangular submatrix [r0, r1) x [c0, c1)
void computeRegion(int V[MAX_N][MAX_M], int r0, int r1, int c0, int c1, int n, int m, int radius) {
    for (int i = r0; i < r1; i++) {
        for (int j = c0; j < c1; j++) {
            V[i][j] = computeConvolution(i, j, n, m, radius);
        }
    }
}

// Returns balanced [start, end) bounds for partition 'id' out of 'parts'
std::pair<int, int> getChunk(int id, int total, int parts) {
    int base = total / parts, rem = total % parts;
    int start = id * base + std::min(id, rem);
    int end = start + base + (id < rem ? 1 : 0);
    return {start, end};
}

// 1. Sequential convolution
void sequential(int V[MAX_N][MAX_M], int n, int m, int k) {
    computeRegion(V, 0, n, 0, m, n, m, k / 2);
}

// 2. Horizontal decomposition (rows distributed across p threads)
void horizontal(int V[MAX_N][MAX_M], int n, int m, int k, int p) {
    std::vector<std::jthread> threads;
    threads.reserve(p);
    for (int id = 0; id < p; id++) {
        auto [r0, r1] = getChunk(id, n, p);
        threads.emplace_back(computeRegion, V, r0, r1, 0, m, n, m, k / 2);
    }
}

// 3. Vertical decomposition (columns distributed across p threads)
void vertical(int V[MAX_N][MAX_M], int n, int m, int k, int p) {
    std::vector<std::jthread> threads;
    threads.reserve(p);
    for (int id = 0; id < p; id++) {
        auto [c0, c1] = getChunk(id, m, p);
        threads.emplace_back(computeRegion, V, 0, n, c0, c1, n, m, k / 2);
    }
}

// 4. Block decomposition (2D submatrix grid pr x pc distributed across p threads)
void block(int V[MAX_N][MAX_M], int n, int m, int k, int p) {
    int pr = static_cast<int>(std::sqrt(p));
    while (p % pr != 0) pr--;
    int pc = p / pr;

    std::vector<std::jthread> threads;
    threads.reserve(p);
    for (int r = 0; r < pr; r++) {
        auto [r0, r1] = getChunk(r, n, pr);
        for (int c = 0; c < pc; c++) {
            auto [c0, c1] = getChunk(c, m, pc);
            threads.emplace_back(computeRegion, V, r0, r1, c0, c1, n, m, k / 2);
        }
    }
}

// 5. Cyclic distribution: delta(i, j) = (i * m + j) % p
void cyclic(int V[MAX_N][MAX_M], int n, int m, int k, int p) {
    int radius = k / 2, total = n * m;
    std::vector<std::jthread> threads;
    threads.reserve(p);
    for (int id = 0; id < p; id++) {
        threads.emplace_back([=, &V]() {
            for (int idx = id; idx < total; idx += p) {
                V[idx / m][idx % m] = computeConvolution(idx / m, idx % m, n, m, radius);
            }
        });
    }
}

// Measures the execution time of a callable in milliseconds
template <typename Func>
double measureMs(Func&& fn) {
    auto t0 = std::chrono::high_resolution_clock::now();
    fn();
    auto t1 = std::chrono::high_resolution_clock::now();
    return std::chrono::duration<double, std::milli>(t1 - t0).count();
}

int main(int argc, char* argv[]) {
    std::ios_base::sync_with_stdio(false);
    std::string mode = argc > 1 ? argv[1] : "all";
    int p = argc > 2 ? std::stoi(argv[2]) : 4;
    std::string fFile = argc > 3 ? argv[3] : "MatrixF.txt";
    std::string cFile = argc > 4 ? argv[4] : "ConvolutionM.txt";

    int n = 0, m = 0, k = 0;
    readMatrix(fFile, F, n, m);
    readKernel(cFile, C, k);

    if (mode == "all" || mode == "verify") {
        std::cout << "Matrix F: " << n << " x " << m << " | Kernel C: " << k << " x " << k << " | Threads p = " << p << "\n"
                  << "--------------------------------------------------\n";

        double msSeq = measureMs([&]() { sequential(V_seq, n, m, k); });
        std::cout << "[Sequential] Execution Time: " << msSeq << " ms\n";
        writeMatrix("output.txt", V_seq, n, m);
        writeMatrix("output_seq.txt", V_seq, n, m);

        struct Variant { std::string label; std::function<void()> run; };
        std::vector<Variant> variants = {
            {"[Horizontal]", [&]() { horizontal(V_par, n, m, k, p); }},
            {"[Vertical]  ", [&]() { vertical(V_par, n, m, k, p); }},
            {"[Block]     ", [&]() { block(V_par, n, m, k, p); }},
            {"[Cyclic]    ", [&]() { cyclic(V_par, n, m, k, p); }}
        };

        for (const auto& v : variants) {
            double ms = measureMs(v.run);
            bool ok = areEqual(V_seq, V_par, n, m, n, m);
            std::cout << v.label << " Execution Time: " << ms << " ms | Match: "
                      << (ok ? "true" : "false") << " | Speedup: " << (msSeq / ms) << "x\n";
        }
        std::cout << "--------------------------------------------------\nOutput matrix saved to output.txt\n";
        return 0;
    }

    double durationMs = measureMs([&]() {
        if (mode == "sequential") sequential(V_par, n, m, k);
        else if (mode == "horizontal") horizontal(V_par, n, m, k, p);
        else if (mode == "vertical") vertical(V_par, n, m, k, p);
        else if (mode == "block") block(V_par, n, m, k, p);
        else if (mode == "cyclic") cyclic(V_par, n, m, k, p);
    });

    writeMatrix("output.txt", V_par, n, m);
    if (mode == "sequential") {
        writeMatrix("output_seq.txt", V_par, n, m);
    }

    // Verify correctness against the sequential baseline file
    std::string seqPath = resolveFile("output_seq.txt");
    if (std::ifstream(seqPath).good()) {
        int seqN = 0, seqM = 0;
        readMatrix(seqPath, V_check, seqN, seqM);
        if (!areEqual(V_check, V_par, seqN, seqM, n, m)) {
            std::cerr << "CORRECTNESS ERROR: Result does not match sequential baseline!\n";
            return 1;
        }
    }

    std::cout << durationMs << "\n";
    return 0;
}