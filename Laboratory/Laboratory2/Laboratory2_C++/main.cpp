#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <barrier>
#include <atomic>
#include <chrono>
#include <algorithm>
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

// Reads input matrix F (n x m) and kernel C (k x k) from "date.txt" (or fallback kernel file)
bool readInputData(const std::string& dataFile, const std::string& kernelFallback,
                   int**& F, int& n, int& m, int**& C, int& k) {
    std::ifstream file(resolveFile(dataFile));
    if (!file.is_open()) return false;

    if (!(file >> n >> m)) return false;
    F = allocateMatrix(n, m);
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            file >> F[i][j];
        }
    }

    // Attempt to read kernel C directly from date.txt if present
    std::string remLine;
    std::getline(file >> std::ws, remLine);
    if (!remLine.empty()) {
        std::stringstream ss(remLine);
        int kCols = 0;
        ss >> k;
        if (!(ss >> kCols)) kCols = k;
        C = allocateMatrix(k, k);
        for (int i = 0; i < k; i++) {
            for (int j = 0; j < k; j++) {
                file >> C[i][j];
            }
        }
        return true;
    }

    // Fallback: read kernel from ConvolutionM.txt if not embedded in date.txt
    std::ifstream kFile(resolveFile(kernelFallback));
    if (kFile.is_open()) {
        std::string line;
        std::getline(kFile, line);
        std::stringstream ss(line);
        int kCols = 0;
        ss >> k;
        if (!(ss >> kCols)) kCols = k;
        C = allocateMatrix(k, k);
        for (int i = 0; i < k; i++) {
            for (int j = 0; j < k; j++) {
                kFile >> C[i][j];
            }
        }
        return true;
    }

    // Default 3x3 kernel (standard sharpening filter from Lab 2)
    k = 3;
    C = allocateMatrix(3, 3);
    const int defK[3][3] = {
        { 0, -1,  0},
        {-1,  5, -1},
        { 0, -1,  0}
    };
    for (int i = 0; i < 3; i++)
        for (int j = 0; j < 3; j++)
            C[i][j] = defK[i][j];
    return true;
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

// Verifies that two output files have identical content without allocating a second matrix
bool verifyFilesEqual(const std::string& fileA, const std::string& fileB) {
    std::ifstream fa(resolveFile(fileA));
    std::ifstream fb(resolveFile(fileB));
    if (!fa.is_open() || !fb.is_open()) return false;

    int rA = 0, cA = 0, rB = 0, cB = 0;
    if (!(fa >> rA >> cA) || !(fb >> rB >> cB)) return false;
    if (rA != rB || cA != cB) return false;

    long long total = 1LL * rA * cA;
    for (long long idx = 0; idx < total; idx++) {
        int valA = 0, valB = 0;
        if (!(fa >> valA) || !(fb >> valB) || valA != valB) return false;
    }
    return true;
}

// Computes the 3x3 convolution for a single row using 3 source row buffers
inline void computeRow3x3(const int* rowAbove, const int* rowCurr, const int* rowBelow,
                          int* destRow, int** C, int m) {
    for (int j = 0; j < m; j++) {
        int left  = (j > 0) ? (j - 1) : 0;
        int right = (j + 1 < m) ? (j + 1) : (m - 1);
        destRow[j] = rowAbove[left] * C[0][0] + rowAbove[j] * C[0][1] + rowAbove[right] * C[0][2]
                   + rowCurr[left]  * C[1][0] + rowCurr[j]  * C[1][1] + rowCurr[right]  * C[1][2]
                   + rowBelow[left] * C[2][0] + rowBelow[j] * C[2][1] + rowBelow[right] * C[2][2];
    }
}

// Returns balanced [start, end) row bounds for partition 'id' out of 'parts'
std::pair<int, int> getChunk(int id, int total, int parts) {
    int base = total / parts, rem = total % parts;
    int start = id * base + std::min(id, rem);
    int end = start + base + (id < rem ? 1 : 0);
    return {start, end};
}

// 1. Sequential In-Place Convolution — O(M) auxiliary space (2 row vectors of length M)
void sequentialInPlace(int** F, int** C, int n, int m) {
    int* prevRow = new int[m];
    int* currRow = new int[m];

    std::copy(F[0], F[0] + m, prevRow);

    for (int i = 0; i < n; i++) {
        std::copy(F[i], F[i] + m, currRow);
        const int* below = (i + 1 < n) ? F[i + 1] : currRow;
        computeRow3x3(prevRow, currRow, below, F[i], C, m);
        std::swap(prevRow, currRow);
    }

    delete[] prevRow;
    delete[] currRow;
}

// 2. Variant 1b: Copy Neighbor Boundaries + Synchronization Barrier (3M ints/thread)
void parallelVariant1b(int** F, int** C, int n, int m, int p) {
    int activeP = std::min(p, n);
    std::barrier syncPoint(activeP);
    std::vector<std::jthread> threads;
    threads.reserve(activeP);

    for (int id = 0; id < activeP; id++) {
        auto [r0, r1] = getChunk(id, n, activeP);
        threads.emplace_back([&, r0, r1]() {
            int* prevRow        = new int[m];
            int* currRow        = new int[m];
            int* bottomBoundary = new int[m];

            // Step 1: Copy required neighbor boundaries into auxiliary vectors
            int topIdx = (r0 > 0) ? (r0 - 1) : 0;
            int botIdx = (r1 < n) ? r1 : (n - 1);
            std::copy(F[topIdx], F[topIdx] + m, prevRow);
            std::copy(F[botIdx], F[botIdx] + m, bottomBoundary);

            // Step 2: Synchronization barrier
            syncPoint.arrive_and_wait();

            // Step 3: Update assigned rows in-place using auxiliary vectors
            for (int i = r0; i < r1; i++) {
                std::copy(F[i], F[i] + m, currRow);
                const int* below = (i + 1 < r1) ? F[i + 1] : bottomBoundary;
                computeRow3x3(prevRow, currRow, below, F[i], C, m);
                std::swap(prevRow, currRow);
            }

            delete[] prevRow;
            delete[] currRow;
            delete[] bottomBoundary;
        });
    }
}

// 3. Variant 1f: Copy Neighbor Boundaries + Fine-Grained Atomic Flags (3M ints/thread)
void parallelVariant1f(int** F, int** C, int n, int m, int p) {
    int activeP = std::min(p, n);
    // Fine-grained flags per neighbor direction (topReady / bottomReady)
    std::vector<std::atomic<bool>> topReady(activeP);
    std::vector<std::atomic<bool>> botReady(activeP);
    for (int i = 0; i < activeP; i++) {
        topReady[i].store(false, std::memory_order_relaxed);
        botReady[i].store(false, std::memory_order_relaxed);
    }

    std::vector<std::jthread> threads;
    threads.reserve(activeP);

    for (int id = 0; id < activeP; id++) {
        auto [r0, r1] = getChunk(id, n, activeP);
        threads.emplace_back([&, id, r0, r1]() {
            int* prevRow        = new int[m];
            int* currRow        = new int[m];
            int* bottomBoundary = new int[m];

            // Step 1 & 2: Copy neighbor boundaries and signal neighbor flags
            int topIdx = (r0 > 0) ? (r0 - 1) : 0;
            std::copy(F[topIdx], F[topIdx] + m, prevRow);
            topReady[id].store(true, std::memory_order_release);

            int botIdx = (r1 < n) ? r1 : (n - 1);
            std::copy(F[botIdx], F[botIdx] + m, bottomBoundary);
            botReady[id].store(true, std::memory_order_release);

            // Step 3: Check that neighbors have copied our boundary rows
            if (id > 0) {
                while (!botReady[id - 1].load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
            }
            if (id + 1 < activeP) {
                while (!topReady[id + 1].load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
            }

            // Step 4: Update assigned rows in-place
            for (int i = r0; i < r1; i++) {
                std::copy(F[i], F[i] + m, currRow);
                const int* below = (i + 1 < r1) ? F[i + 1] : bottomBoundary;
                computeRow3x3(prevRow, currRow, below, F[i], C, m);
                std::swap(prevRow, currRow);
            }

            delete[] prevRow;
            delete[] currRow;
            delete[] bottomBoundary;
        });
    }
}

// 4. Variant 2b: Compute Boundary New Values to Vectors, Interior In-Place + Barrier (3M ints/thread)
void parallelVariant2b(int** F, int** C, int n, int m, int p) {
    int activeP = std::min(p, n);
    std::barrier syncPoint(activeP);
    std::vector<std::jthread> threads;
    threads.reserve(activeP);

    for (int id = 0; id < activeP; id++) {
        auto [r0, r1] = getChunk(id, n, activeP);
        threads.emplace_back([&, r0, r1]() {
            int* newFirstRow = new int[m];
            int* prevRow     = new int[m];
            int* currRow     = new int[m]; // Reused as newLastRow after interior loop

            // Step 1a: Compute new values for top boundary row r0 into newFirstRow
            const int* topAbove = (r0 > 0) ? F[r0 - 1] : F[0];
            const int* topBelow = (r0 + 1 < n) ? F[r0 + 1] : F[r0];
            computeRow3x3(topAbove, F[r0], topBelow, newFirstRow, C, m);

            if (r1 - 1 > r0) {
                // Step 1b: Compute interior rows [r0 + 1, r1 - 2] directly into matrix F
                std::copy(F[r0], F[r0] + m, prevRow);
                for (int i = r0 + 1; i <= r1 - 2; i++) {
                    std::copy(F[i], F[i] + m, currRow);
                    computeRow3x3(prevRow, currRow, F[i + 1], F[i], C, m);
                    std::swap(prevRow, currRow);
                }

                // Step 1c: Compute bottom boundary row (r1 - 1) into currRow (acting as newLastRow)
                // Note: prevRow now holds the untouched original values of row (r1 - 2)!
                int last = r1 - 1;
                const int* botBelow = (r1 < n) ? F[r1] : F[last];
                computeRow3x3(prevRow, F[last], botBelow, currRow, C, m);
            }

            // Step 2: Synchronization barrier
            syncPoint.arrive_and_wait();

            // Step 3: Copy computed boundary vectors into matrix F
            std::copy(newFirstRow, newFirstRow + m, F[r0]);
            if (r1 - 1 > r0) {
                std::copy(currRow, currRow + m, F[r1 - 1]);
            }

            delete[] newFirstRow;
            delete[] prevRow;
            delete[] currRow;
        });
    }
}

// 5. Variant 2f: Compute Boundary New Values First + Atomic Flags + Interior In-Place (4M ints/thread)
void parallelVariant2f(int** F, int** C, int n, int m, int p) {
    int activeP = std::min(p, n);
    std::vector<std::atomic<bool>> topDone(activeP);
    std::vector<std::atomic<bool>> botDone(activeP);
    for (int i = 0; i < activeP; i++) {
        topDone[i].store(false, std::memory_order_relaxed);
        botDone[i].store(false, std::memory_order_relaxed);
    }

    std::vector<std::jthread> threads;
    threads.reserve(activeP);

    for (int id = 0; id < activeP; id++) {
        auto [r0, r1] = getChunk(id, n, activeP);
        threads.emplace_back([&, id, r0, r1]() {
            int* newFirstRow = new int[m];
            int* newLastRow  = (r1 - 1 > r0) ? new int[m] : nullptr;
            int* prevRow     = new int[m];
            int* currRow     = new int[m];

            // Step 1 & 2: Compute boundary rows into separate vectors and signal flags immediately
            const int* topAbove = (r0 > 0) ? F[r0 - 1] : F[0];
            const int* topBelow = (r0 + 1 < n) ? F[r0 + 1] : F[r0];
            computeRow3x3(topAbove, F[r0], topBelow, newFirstRow, C, m);
            topDone[id].store(true, std::memory_order_release);

            if (r1 - 1 > r0) {
                int last = r1 - 1;
                const int* botAbove = F[last - 1];
                const int* botBelow = (r1 < n) ? F[r1] : F[last];
                computeRow3x3(botAbove, F[last], botBelow, newLastRow, C, m);
            }
            botDone[id].store(true, std::memory_order_release);

            // Step 3: Compute interior rows [r0 + 1, r1 - 2] directly into matrix F
            if (r1 - r0 > 2) {
                std::copy(F[r0], F[r0] + m, prevRow);
                for (int i = r0 + 1; i <= r1 - 2; i++) {
                    std::copy(F[i], F[i] + m, currRow);
                    computeRow3x3(prevRow, currRow, F[i + 1], F[i], C, m);
                    std::swap(prevRow, currRow);
                }
            }

            // Step 4 & 5: Wait for neighbor flags and update boundary elements in F
            if (id > 0) {
                while (!botDone[id - 1].load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
            }
            std::copy(newFirstRow, newFirstRow + m, F[r0]);

            if (r1 - 1 > r0) {
                if (id + 1 < activeP) {
                    while (!topDone[id + 1].load(std::memory_order_acquire)) {
                        std::this_thread::yield();
                    }
                }
                std::copy(newLastRow, newLastRow + m, F[r1 - 1]);
            }

            delete[] newFirstRow;
            delete[] newLastRow;
            delete[] prevRow;
            delete[] currRow;
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
    std::string dataFile = argc > 3 ? argv[3] : "date.txt";
    std::string kernelFile = argc > 4 ? argv[4] : "ConvolutionM.txt";

    int n = 0, m = 0, k = 0;
    int** F = nullptr;
    int** C = nullptr;

    if (!readInputData(dataFile, kernelFile, F, n, m, C, k)) {
        std::cerr << "Error: Could not read input file " << dataFile << "\n";
        return 1;
    }

    if (mode == "all" || mode == "verify") {
        std::cout << "Matrix F: " << n << " x " << m << " | Kernel C: " << k << " x " << k
                  << " | Threads p = " << p << " (In-Place O(m) Aux Space)\n"
                  << "------------------------------------------------------------------\n";

        double msSeq = measureMs([&]() { sequentialInPlace(F, C, n, m); });
        std::cout << "[Sequential]           Execution Time: " << msSeq << " ms\n";
        writeMatrix("output.txt", F, n, m);
        writeMatrix("output_seq.txt", F, n, m);

        struct Variant { std::string label; std::function<void()> run; };
        std::vector<Variant> variants = {
            {"[Variant 1b (Barrier)]", [&]() { parallelVariant1b(F, C, n, m, p); }},
            {"[Variant 1f (Flags)  ]", [&]() { parallelVariant1f(F, C, n, m, p); }},
            {"[Variant 2b (Barrier)]", [&]() { parallelVariant2b(F, C, n, m, p); }},
            {"[Variant 2f (Flags)  ]", [&]() { parallelVariant2f(F, C, n, m, p); }}
        };

        for (const auto& v : variants) {
            freeMatrix(F, n);
            freeMatrix(C, k);
            readInputData(dataFile, kernelFile, F, n, m, C, k);

            double ms = measureMs(v.run);
            writeMatrix("output.txt", F, n, m);
            bool ok = verifyFilesEqual("output_seq.txt", "output.txt");
            std::cout << v.label << " Execution Time: " << ms << " ms | Match: "
                      << (ok ? "true" : "false") << " | Speedup: " << (msSeq / ms) << "x\n";
        }
        std::cout << "------------------------------------------------------------------\n"
                  << "Output matrix saved to output.txt\n";
    } else {
        double durationMs = measureMs([&]() {
            if (mode == "sequential") sequentialInPlace(F, C, n, m);
            else if (mode == "1b" || mode == "variant1b" || mode == "horizontal") parallelVariant1b(F, C, n, m, p);
            else if (mode == "1f" || mode == "variant1f") parallelVariant1f(F, C, n, m, p);
            else if (mode == "2b" || mode == "variant2b") parallelVariant2b(F, C, n, m, p);
            else if (mode == "2f" || mode == "variant2f") parallelVariant2f(F, C, n, m, p);
            else {
                std::cerr << "Unknown mode: " << mode << "\n";
                std::exit(1);
            }
        });

        writeMatrix("output.txt", F, n, m);
        if (mode == "sequential") {
            writeMatrix("output_seq.txt", F, n, m);
        }

        // Verify correctness against the sequential baseline file without allocating an extra matrix
        std::string seqPath = resolveFile("output_seq.txt");
        if (std::ifstream(seqPath).good()) {
            if (!verifyFilesEqual(seqPath, "output.txt")) {
                std::cerr << "CORRECTNESS ERROR: Result does not match sequential baseline!\n";
                freeMatrix(F, n);
                freeMatrix(C, k);
                return 1;
            }
        }
        std::cout << durationMs << "\n";
    }

    freeMatrix(F, n);
    freeMatrix(C, k);
    return 0;
}
