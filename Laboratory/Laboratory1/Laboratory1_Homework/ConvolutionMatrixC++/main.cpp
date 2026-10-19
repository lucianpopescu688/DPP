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

// Resolves file path from either the current directory or the parent directory
std::string resolveFile(const std::string& filename) {
    if (std::ifstream(filename).good()) return filename;
    std::string parent = "../" + filename;
    if (std::ifstream(parent).good()) return parent;
    return filename;
}

// Dynamically allocates a 2D matrix of size rows x cols on the heap
int** allocateMatrix(int rows, int cols) {
    int** matrix = new int*[rows];
    for (int i = 0; i < rows; i++) {
        matrix[i] = new int[cols];
    }
    return matrix;
}

// Frees a dynamically allocated 2D heap matrix
void freeMatrix(int** matrix, int rows) {
    if (!matrix) return;
    for (int i = 0; i < rows; i++) {
        delete[] matrix[i];
    }
    delete[] matrix;
}

// Reads a matrix from a text file (supports both "n m" and square "k" headers)
int** readMatrix(const std::string& filename, int& rows, int& cols) {
    std::ifstream file(resolveFile(filename));
    std::string line;
    if (!std::getline(file, line)) {
        rows = cols = 0;
        return nullptr;
    }
    std::stringstream ss(line);
    ss >> rows;
    if (!(ss >> cols)) cols = rows;

    int** matrix = allocateMatrix(rows, cols);
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            file >> matrix[i][j];
        }
    }
    return matrix;
}

// Writes a 2D matrix to an output text file
void writeMatrix(const std::string& filename, int** matrix, int rows, int cols) {
    std::ofstream file(filename);
    file << rows << " " << cols << "\n";
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            file << matrix[i][j] << (j + 1 < cols ? " " : "");
        }
        file << "\n";
    }
}

// Verifies that two matrices have identical dimensions and element values
bool areEqual(int** a, int** b, int rowsA, int colsA, int rowsB, int colsB) {
    if (!a || !b || rowsA != rowsB || colsA != colsB) return false;
    for (int i = 0; i < rowsA; i++) {
        for (int j = 0; j < colsA; j++) {
            if (a[i][j] != b[i][j]) return false;
        }
    }
    return true;
}

// Computes the convolution for a single cell (i, j) using clamped virtual borders
int computeConvolution(int** F, int** C, int i, int j, int n, int m, int radius) {
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
void computeRegion(int** F, int** C, int** V, int r0, int r1, int c0, int c1, int n, int m, int radius) {
    for (int i = r0; i < r1; i++) {
        for (int j = c0; j < c1; j++) {
            V[i][j] = computeConvolution(F, C, i, j, n, m, radius);
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
void sequential(int** F, int** C, int** V, int n, int m, int k) {
    computeRegion(F, C, V, 0, n, 0, m, n, m, k / 2);
}

// 2. Horizontal decomposition (rows distributed across p threads)
void horizontal(int** F, int** C, int** V, int n, int m, int k, int p) {
    std::vector<std::jthread> threads;
    threads.reserve(p);
    for (int id = 0; id < p; id++) {
        auto [r0, r1] = getChunk(id, n, p);
        threads.emplace_back(computeRegion, F, C, V, r0, r1, 0, m, n, m, k / 2);
    }
}

// 3. Vertical decomposition (columns distributed across p threads)
void vertical(int** F, int** C, int** V, int n, int m, int k, int p) {
    std::vector<std::jthread> threads;
    threads.reserve(p);
    for (int id = 0; id < p; id++) {
        auto [c0, c1] = getChunk(id, m, p);
        threads.emplace_back(computeRegion, F, C, V, 0, n, c0, c1, n, m, k / 2);
    }
}

// 4. Block decomposition (2D submatrix grid pr x pc distributed across p threads)
void block(int** F, int** C, int** V, int n, int m, int k, int p) {
    int pr = static_cast<int>(std::sqrt(p));
    while (p % pr != 0) pr--;
    int pc = p / pr;

    std::vector<std::jthread> threads;
    threads.reserve(p);
    for (int r = 0; r < pr; r++) {
        auto [r0, r1] = getChunk(r, n, pr);
        for (int c = 0; c < pc; c++) {
            auto [c0, c1] = getChunk(c, m, pc);
            threads.emplace_back(computeRegion, F, C, V, r0, r1, c0, c1, n, m, k / 2);
        }
    }
}

// 5. Cyclic distribution: delta(i, j) = (i * m + j) % p
void cyclic(int** F, int** C, int** V, int n, int m, int k, int p) {
    int radius = k / 2, total = n * m;
    std::vector<std::jthread> threads;
    threads.reserve(p);
    for (int id = 0; id < p; id++) {
        threads.emplace_back([=]() {
            for (int idx = id; idx < total; idx += p) {
                V[idx / m][idx % m] = computeConvolution(F, C, idx / m, idx % m, n, m, radius);
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

    int n = 0, m = 0, k = 0, kCols = 0;
    int** F = readMatrix(fFile, n, m);
    int** C = readMatrix(cFile, k, kCols);
    int** V_par = allocateMatrix(n, m);

    if (mode == "all" || mode == "verify") {
        int** V_seq = allocateMatrix(n, m);
        std::cout << "Matrix F: " << n << " x " << m << " | Kernel C: " << k << " x " << k << " | Threads p = " << p << "\n"
                  << "--------------------------------------------------\n";

        double msSeq = measureMs([&]() { sequential(F, C, V_seq, n, m, k); });
        std::cout << "[Sequential] Execution Time: " << msSeq << " ms\n";
        writeMatrix("output.txt", V_seq, n, m);
        writeMatrix("output_seq.txt", V_seq, n, m);

        struct Variant { std::string label; std::function<void()> run; };
        std::vector<Variant> variants = {
            {"[Horizontal]", [&]() { horizontal(F, C, V_par, n, m, k, p); }},
            {"[Vertical]  ", [&]() { vertical(F, C, V_par, n, m, k, p); }},
            {"[Block]     ", [&]() { block(F, C, V_par, n, m, k, p); }},
            {"[Cyclic]    ", [&]() { cyclic(F, C, V_par, n, m, k, p); }}
        };

        for (const auto& v : variants) {
            double ms = measureMs(v.run);
            bool ok = areEqual(V_seq, V_par, n, m, n, m);
            std::cout << v.label << " Execution Time: " << ms << " ms | Match: "
                      << (ok ? "true" : "false") << " | Speedup: " << (msSeq / ms) << "x\n";
        }
        std::cout << "--------------------------------------------------\nOutput matrix saved to output.txt\n";
        freeMatrix(V_seq, n);
    } else {
        double durationMs = measureMs([&]() {
            if (mode == "sequential") sequential(F, C, V_par, n, m, k);
            else if (mode == "horizontal") horizontal(F, C, V_par, n, m, k, p);
            else if (mode == "vertical") vertical(F, C, V_par, n, m, k, p);
            else if (mode == "block") block(F, C, V_par, n, m, k, p);
            else if (mode == "cyclic") cyclic(F, C, V_par, n, m, k, p);
        });

        writeMatrix("output.txt", V_par, n, m);
        if (mode == "sequential") {
            writeMatrix("output_seq.txt", V_par, n, m);
        }

        // Verify correctness against the sequential baseline file
        std::string seqPath = resolveFile("output_seq.txt");
        if (std::ifstream(seqPath).good()) {
            int seqN = 0, seqM = 0;
            int** expected = readMatrix(seqPath, seqN, seqM);
            bool ok = areEqual(expected, V_par, seqN, seqM, n, m);
            freeMatrix(expected, seqN);
            if (!ok) {
                std::cerr << "CORRECTNESS ERROR: Result does not match sequential baseline!\n";
                freeMatrix(F, n); freeMatrix(C, k); freeMatrix(V_par, n);
                return 1;
            }
        }
        std::cout << durationMs << "\n";
    }

    freeMatrix(F, n);
    freeMatrix(C, k);
    freeMatrix(V_par, n);
    return 0;
}