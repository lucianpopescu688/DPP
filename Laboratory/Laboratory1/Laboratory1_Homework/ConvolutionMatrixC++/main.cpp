#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>

std::string resolveFile(const std::string& filename) {
    std::ifstream f(filename);
    if (f.good()) return filename;
    std::string parent = "../" + filename;
    std::ifstream fp(parent);
    if (fp.good()) return parent;
    return filename;
}

int** allocateMatrix(int rows, int cols) {
    int** matrix = new int*[rows];
    for (int i = 0; i < rows; i++) {
        matrix[i] = new int[cols];
    }
    return matrix;
}

void freeMatrix(int** matrix, int rows) {
    for (int i = 0; i < rows; i++) {
        delete[] matrix[i];
    }
    delete[] matrix;
}

int** readMatrix(const std::string& filename, int& rows, int& cols) {
    std::ifstream file(resolveFile(filename));
    std::string line;
    if (!std::getline(file, line)) {
        rows = 0; cols = 0;
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

void writeMatrix(const std::string& filename, int** matrix, int rows, int cols) {
    std::ofstream file(filename);
    file << rows << " " << cols << "\n";
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            file << matrix[i][j] << (j == cols - 1 ? "" : " ");
        }
        file << "\n";
    }
}

bool areEqual(int** a, int** b, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (a[i][j] != b[i][j]) return false;
        }
    }
    return true;
}

int computeConvolution(int** F, int** C, int i, int j, int n, int m, int k, int radius) {
    int sum = 0;
    for (int a = -radius; a <= radius; a++) {
        for (int b = -radius; b <= radius; b++) {
            int r = std::clamp(i + a, 0, n - 1);
            int c = std::clamp(j + b, 0, m - 1);
            sum += F[r][c] * C[a + radius][b + radius];
        }
    }
    return sum;
}

void sequential(int** F, int** C, int** V, int n, int m, int k) {
    int radius = k / 2;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            V[i][j] = computeConvolution(F, C, i, j, n, m, k, radius);
        }
    }
}

void horizontal(int** F, int** C, int** V, int n, int m, int k, int p) {
    int radius = k / 2;
    std::vector<std::jthread> threads;
    threads.reserve(p);

    int base = n / p, rem = n % p, startRow = 0;
    for (int id = 0; id < p; id++) {
        int r0 = startRow;
        int r1 = startRow + base + (id < rem ? 1 : 0);
        threads.emplace_back([=]() {
            for (int i = r0; i < r1; i++) {
                for (int j = 0; j < m; j++) {
                    V[i][j] = computeConvolution(F, C, i, j, n, m, k, radius);
                }
            }
        });
        startRow = r1;
    }
}

void vertical(int** F, int** C, int** V, int n, int m, int k, int p) {
    int radius = k / 2;
    std::vector<std::jthread> threads;
    threads.reserve(p);

    int base = m / p, rem = m % p, startCol = 0;
    for (int id = 0; id < p; id++) {
        int c0 = startCol;
        int c1 = startCol + base + (id < rem ? 1 : 0);
        threads.emplace_back([=]() {
            for (int i = 0; i < n; i++) {
                for (int j = c0; j < c1; j++) {
                    V[i][j] = computeConvolution(F, C, i, j, n, m, k, radius);
                }
            }
        });
        startCol = c1;
    }
}

void block(int** F, int** C, int** V, int n, int m, int k, int p) {
    int radius = k / 2;
    int pr = 1, pc = p;
    for (int r = (int)std::sqrt(p); r >= 1; r--) {
        if (p % r == 0) {
            pr = r;
            pc = p / r;
            break;
        }
    }

    std::vector<std::jthread> threads;
    threads.reserve(p);

    int rowBase = n / pr, rowRem = n % pr;
    int colBase = m / pc, colRem = m % pc;
    int startRow = 0;

    for (int r = 0; r < pr; r++) {
        int r0 = startRow;
        int r1 = startRow + rowBase + (r < rowRem ? 1 : 0);
        int startCol = 0;
        for (int c = 0; c < pc; c++) {
            int c0 = startCol;
            int c1 = startCol + colBase + (c < colRem ? 1 : 0);
            threads.emplace_back([=]() {
                for (int i = r0; i < r1; i++) {
                    for (int j = c0; j < c1; j++) {
                        V[i][j] = computeConvolution(F, C, i, j, n, m, k, radius);
                    }
                }
            });
            startCol = c1;
        }
        startRow = r1;
    }
}

void cyclic(int** F, int** C, int** V, int n, int m, int k, int p) {
    int radius = k / 2;
    std::vector<std::jthread> threads;
    threads.reserve(p);

    int total = n * m;
    for (int id = 0; id < p; id++) {
        threads.emplace_back([=]() {
            for (int idx = id; idx < total; idx += p) {
                V[idx / m][idx % m] = computeConvolution(F, C, idx / m, idx % m, n, m, k, radius);
            }
        });
    }
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

    int** V_seq = allocateMatrix(n, m);
    int** V_par = allocateMatrix(n, m);

    if (mode == "all" || mode == "verify") {
        std::cout << "Matrix F: " << n << " x " << m << " | Kernel C: " << k << " x " << k << " | Threads p = " << p << "\n";
        std::cout << "--------------------------------------------------\n";

        auto t0 = std::chrono::high_resolution_clock::now();
        sequential(F, C, V_seq, n, m, k);
        auto t1 = std::chrono::high_resolution_clock::now();
        double msSeq = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "[Sequential] Execution Time: " << msSeq << " ms\n";
        writeMatrix("output.txt", V_seq, n, m);

        t0 = std::chrono::high_resolution_clock::now();
        horizontal(F, C, V_par, n, m, k, p);
        t1 = std::chrono::high_resolution_clock::now();
        double msHoriz = std::chrono::duration<double, std::milli>(t1 - t0).count();
        bool okHoriz = areEqual(V_seq, V_par, n, m);
        std::cout << "[Horizontal] Execution Time: " << msHoriz << " ms | Match: " << (okHoriz ? "true" : "false") << " | Speedup: " << (msSeq / msHoriz) << "x\n";

        t0 = std::chrono::high_resolution_clock::now();
        vertical(F, C, V_par, n, m, k, p);
        t1 = std::chrono::high_resolution_clock::now();
        double msVert = std::chrono::duration<double, std::milli>(t1 - t0).count();
        bool okVert = areEqual(V_seq, V_par, n, m);
        std::cout << "[Vertical]   Execution Time: " << msVert << " ms | Match: " << (okVert ? "true" : "false") << " | Speedup: " << (msSeq / msVert) << "x\n";

        t0 = std::chrono::high_resolution_clock::now();
        block(F, C, V_par, n, m, k, p);
        t1 = std::chrono::high_resolution_clock::now();
        double msBlock = std::chrono::duration<double, std::milli>(t1 - t0).count();
        bool okBlock = areEqual(V_seq, V_par, n, m);
        std::cout << "[Block]      Execution Time: " << msBlock << " ms | Match: " << (okBlock ? "true" : "false") << " | Speedup: " << (msSeq / msBlock) << "x\n";

        t0 = std::chrono::high_resolution_clock::now();
        cyclic(F, C, V_par, n, m, k, p);
        t1 = std::chrono::high_resolution_clock::now();
        double msCyc = std::chrono::duration<double, std::milli>(t1 - t0).count();
        bool okCyc = areEqual(V_seq, V_par, n, m);
        std::cout << "[Cyclic]     Execution Time: " << msCyc << " ms | Match: " << (okCyc ? "true" : "false") << " | Speedup: " << (msSeq / msCyc) << "x\n";

        std::cout << "--------------------------------------------------\n";
        std::cout << "Output matrix saved to output.txt\n";

        freeMatrix(F, n);
        freeMatrix(C, k);
        freeMatrix(V_seq, n);
        freeMatrix(V_par, n);
        return 0;
    }

    auto start = std::chrono::high_resolution_clock::now();
    if (mode == "seq") {
        sequential(F, C, V_par, n, m, k);
    } else if (mode == "horizontal" || mode == "horiz") {
        horizontal(F, C, V_par, n, m, k, p);
    } else if (mode == "vertical" || mode == "vert") {
        vertical(F, C, V_par, n, m, k, p);
    } else if (mode == "block") {
        block(F, C, V_par, n, m, k, p);
    } else if (mode == "cyclic" || mode == "cyc") {
        cyclic(F, C, V_par, n, m, k, p);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();

    writeMatrix("output.txt", V_par, n, m);

    std::string seqPath = resolveFile("output_seq.txt");
    std::ifstream seqFile(seqPath);
    if (seqFile.good()) {
        int seqN = 0, seqM = 0;
        int** expected = readMatrix(seqPath, seqN, seqM);
        if (!areEqual(expected, V_par, n, m)) {
            std::cerr << "CORRECTNESS ERROR: Result does not match sequential baseline!\n";
            freeMatrix(expected, seqN);
            freeMatrix(F, n);
            freeMatrix(C, k);
            freeMatrix(V_seq, n);
            freeMatrix(V_par, n);
            return 1;
        }
        freeMatrix(expected, seqN);
    }

    std::cout << durationMs << "\n";

    freeMatrix(F, n);
    freeMatrix(C, k);
    freeMatrix(V_seq, n);
    freeMatrix(V_par, n);
    return 0;
}