#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <algorithm>
#include <cmath>

const int MAX_N = 10005;
const int MAX_M = 10005;
const int MAX_K = 11;

static int F[MAX_N][MAX_M];
static int C[MAX_K][MAX_K];
static int V_seq[MAX_N][MAX_M];
static int V_par[MAX_N][MAX_M];
static int V_check[MAX_N][MAX_M];

std::string resolveFile(const std::string& filename) {
    std::ifstream f(filename);
    if (f.good()) return filename;
    std::string parent = "../" + filename;
    std::ifstream fp(parent);
    if (fp.good()) return parent;
    return filename;
}

void readMatrix(const std::string& filename, int matrix[MAX_N][MAX_M], int& rows, int& cols) {
    std::ifstream file(resolveFile(filename));
    std::string line;
    if (!std::getline(file, line)) {
        rows = 0; cols = 0;
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

void writeMatrix(const std::string& filename, int matrix[MAX_N][MAX_M], int rows, int cols) {
    std::ofstream file(filename);
    file << rows << " " << cols << "\n";
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            file << matrix[i][j] << (j == cols - 1 ? "" : " ");
        }
        file << "\n";
    }
}

bool areEqual(int a[MAX_N][MAX_M], int b[MAX_N][MAX_M], int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            if (a[i][j] != b[i][j]) return false;
        }
    }
    return true;
}

int computeConvolution(int i, int j, int n, int m, int k, int radius) {
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

void sequential(int V[MAX_N][MAX_M], int n, int m, int k) {
    int radius = k / 2;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < m; j++) {
            V[i][j] = computeConvolution(i, j, n, m, k, radius);
        }
    }
}

void horizontal(int V[MAX_N][MAX_M], int n, int m, int k, int p) {
    int radius = k / 2;
    std::vector<std::jthread> threads;
    threads.reserve(p);

    int base = n / p, rem = n % p, startRow = 0;
    for (int id = 0; id < p; id++) {
        int r0 = startRow;
        int r1 = startRow + base + (id < rem ? 1 : 0);
        threads.emplace_back([=, &V]() {
            for (int i = r0; i < r1; i++) {
                for (int j = 0; j < m; j++) {
                    V[i][j] = computeConvolution(i, j, n, m, k, radius);
                }
            }
        });
        startRow = r1;
    }
}

void vertical(int V[MAX_N][MAX_M], int n, int m, int k, int p) {
    int radius = k / 2;
    std::vector<std::jthread> threads;
    threads.reserve(p);

    int base = m / p, rem = m % p, startCol = 0;
    for (int id = 0; id < p; id++) {
        int c0 = startCol;
        int c1 = startCol + base + (id < rem ? 1 : 0);
        threads.emplace_back([=, &V]() {
            for (int i = 0; i < n; i++) {
                for (int j = c0; j < c1; j++) {
                    V[i][j] = computeConvolution(i, j, n, m, k, radius);
                }
            }
        });
        startCol = c1;
    }
}

void block(int V[MAX_N][MAX_M], int n, int m, int k, int p) {
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
            threads.emplace_back([=, &V]() {
                for (int i = r0; i < r1; i++) {
                    for (int j = c0; j < c1; j++) {
                        V[i][j] = computeConvolution(i, j, n, m, k, radius);
                    }
                }
            });
            startCol = c1;
        }
        startRow = r1;
    }
}

void cyclic(int V[MAX_N][MAX_M], int n, int m, int k, int p) {
    int radius = k / 2;
    std::vector<std::jthread> threads;
    threads.reserve(p);

    int total = n * m;
    for (int id = 0; id < p; id++) {
        threads.emplace_back([=, &V]() {
            for (int idx = id; idx < total; idx += p) {
                V[idx / m][idx % m] = computeConvolution(idx / m, idx % m, n, m, k, radius);
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

    int n = 0, m = 0, k = 0;
    readMatrix(fFile, F, n, m);
    readKernel(cFile, C, k);

    if (mode == "all" || mode == "verify") {
        std::cout << "Matrix F: " << n << " x " << m << " | Kernel C: " << k << " x " << k << " | Threads p = " << p << "\n";
        std::cout << "--------------------------------------------------\n";

        auto t0 = std::chrono::high_resolution_clock::now();
        sequential(V_seq, n, m, k);
        auto t1 = std::chrono::high_resolution_clock::now();
        double msSeq = std::chrono::duration<double, std::milli>(t1 - t0).count();
        std::cout << "[Sequential] Execution Time: " << msSeq << " ms\n";
        writeMatrix("output.txt", V_seq, n, m);

        t0 = std::chrono::high_resolution_clock::now();
        horizontal(V_par, n, m, k, p);
        t1 = std::chrono::high_resolution_clock::now();
        double msHoriz = std::chrono::duration<double, std::milli>(t1 - t0).count();
        bool okHoriz = areEqual(V_seq, V_par, n, m);
        std::cout << "[Horizontal] Execution Time: " << msHoriz << " ms | Match: " << (okHoriz ? "true" : "false") << " | Speedup: " << (msSeq / msHoriz) << "x\n";

        t0 = std::chrono::high_resolution_clock::now();
        vertical(V_par, n, m, k, p);
        t1 = std::chrono::high_resolution_clock::now();
        double msVert = std::chrono::duration<double, std::milli>(t1 - t0).count();
        bool okVert = areEqual(V_seq, V_par, n, m);
        std::cout << "[Vertical]   Execution Time: " << msVert << " ms | Match: " << (okVert ? "true" : "false") << " | Speedup: " << (msSeq / msVert) << "x\n";

        t0 = std::chrono::high_resolution_clock::now();
        block(V_par, n, m, k, p);
        t1 = std::chrono::high_resolution_clock::now();
        double msBlock = std::chrono::duration<double, std::milli>(t1 - t0).count();
        bool okBlock = areEqual(V_seq, V_par, n, m);
        std::cout << "[Block]      Execution Time: " << msBlock << " ms | Match: " << (okBlock ? "true" : "false") << " | Speedup: " << (msSeq / msBlock) << "x\n";

        t0 = std::chrono::high_resolution_clock::now();
        cyclic(V_par, n, m, k, p);
        t1 = std::chrono::high_resolution_clock::now();
        double msCyc = std::chrono::duration<double, std::milli>(t1 - t0).count();
        bool okCyc = areEqual(V_seq, V_par, n, m);
        std::cout << "[Cyclic]     Execution Time: " << msCyc << " ms | Match: " << (okCyc ? "true" : "false") << " | Speedup: " << (msSeq / msCyc) << "x\n";

        std::cout << "--------------------------------------------------\n";
        std::cout << "Output matrix saved to output.txt\n";
        return 0;
    }

    auto start = std::chrono::high_resolution_clock::now();
    if (mode == "seq") {
        sequential(V_par, n, m, k);
    } else if (mode == "horizontal" || mode == "horiz") {
        horizontal(V_par, n, m, k, p);
    } else if (mode == "vertical" || mode == "vert") {
        vertical(V_par, n, m, k, p);
    } else if (mode == "block") {
        block(V_par, n, m, k, p);
    } else if (mode == "cyclic" || mode == "cyc") {
        cyclic(V_par, n, m, k, p);
    }
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();

    writeMatrix("output.txt", V_par, n, m);

    std::string seqPath = resolveFile("output_seq.txt");
    std::ifstream seqFile(seqPath);
    if (seqFile.good()) {
        int seqN = 0, seqM = 0;
        readMatrix(seqPath, V_check, seqN, seqM);
        if (!areEqual(V_check, V_par, n, m)) {
            std::cerr << "CORRECTNESS ERROR: Result does not match sequential baseline!\n";
            return 1;
        }
    }

    std::cout << durationMs << "\n";
    return 0;
}
