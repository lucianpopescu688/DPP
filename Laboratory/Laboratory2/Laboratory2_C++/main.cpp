#include <iostream>
#include <vector>
#include <thread>
#include <barrier>
#include <fstream>
#include <chrono>
#include <string>
#include <iomanip>

using namespace std;

// --- CONSTANTS ---
// Standard 3x3 sharpening kernel
const int KERNEL[3][3] = {
    { 0, -1,  0},
    {-1,  5, -1},
    { 0, -1,  0}
};

// --- MEMORY MANAGEMENT ---
// Allocate 2D matrix on the heap
int** allocate_matrix(int N, int M) {
    int** matrix = new int*[N];
    for (int i = 0; i < N; i++) {
        matrix[i] = new int[M]();
    }
    return matrix;
}

// Free 2D heap matrix
void free_matrix(int** matrix, int N) {
    if (!matrix) return;
    for (int i = 0; i < N; i++) {
        delete[] matrix[i];
    }
    delete[] matrix;
}

// --- FILE I/O & VERIFICATION ---
// Reads matrix from date.txt
int** read_matrix(const string& filename, int& N, int& M) {
    ifstream fin(filename);
    if (!fin) {
        // Try parent directory fallback
        fin.open("../" + filename);
        if (!fin) return nullptr;
    }
    if (!(fin >> N >> M)) return nullptr;
    int** matrix = allocate_matrix(N, M);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            fin >> matrix[i][j];
        }
    }
    return matrix;
}

// Writes matrix to output text file
void write_matrix(const string& filename, int** matrix, int N, int M) {
    ofstream fout(filename);
    fout << N << " " << M << "\n";
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            fout << matrix[i][j] << (j + 1 < M ? " " : "");
        }
        fout << "\n";
    }
}

// Verifies two output files match byte-for-byte without allocating an extra matrix in memory
bool verify_files_equal(const string& file1, const string& file2) {
    ifstream f1(file1), f2(file2);
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

// --- SEQUENTIAL IN-PLACE CONVOLUTION (O(M) AUXILIARY SPACE) ---
void sequential_convolution(int N, int M, int** matrix) {
    if (N < 3 || M < 3) return;

    int* prev_row = new int[M];
    int* curr_row = new int[M];
    
    for (int j = 0; j < M; j++) prev_row[j] = matrix[0][j];
    for (int j = 0; j < M; j++) curr_row[j] = matrix[1][j];

    for (int i = 1; i < N - 1; i++) {
        int* next_row = matrix[i + 1];
        int* computed_row = new int[M];

        for (int j = 1; j < M - 1; j++) {
            computed_row[j] = 
                prev_row[j - 1] * KERNEL[0][0] + prev_row[j] * KERNEL[0][1] + prev_row[j + 1] * KERNEL[0][2] +
                curr_row[j - 1] * KERNEL[1][0] + curr_row[j] * KERNEL[1][1] + curr_row[j + 1] * KERNEL[1][2] +
                next_row[j - 1] * KERNEL[2][0] + next_row[j] * KERNEL[2][1] + next_row[j + 1] * KERNEL[2][2];
        }
        computed_row[0] = curr_row[0];
        computed_row[M - 1] = curr_row[M - 1];

        // Replace row in matrix
        delete[] matrix[i];
        matrix[i] = computed_row;
        
        // Shift pointers
        if (i > 1) delete[] prev_row;
        prev_row = curr_row;
        curr_row = new int[M];
        for (int j = 0; j < M; j++) curr_row[j] = next_row[j];
    }
    delete[] prev_row;
    delete[] curr_row;
}

// --- PARALLEL WORKER (VARIANT 2B - BARRIER + DELAYED BOUNDARY WRITE) ---
void parallel_worker(int id, int P, int N, int M, int** matrix, std::barrier<>& sync_barrier) {
    int inner_rows = N - 2;
    int start_row = 1 + id * inner_rows / P;
    int end_row   = 1 + (id + 1) * inner_rows / P;
    if (id == P - 1) end_row = N - 1;

    if (start_row >= end_row) {
        sync_barrier.arrive_and_wait();
        return;
    }

    // Allocate 1D temporary arrays (Strictly O(M) auxiliary space per thread)
    int* prev_row   = new int[M]();
    int* curr_row   = new int[M]();
    int* new_top    = new int[M]();
    int* new_bottom = new int[M]();

    if (start_row > 0) {
        for (int j = 0; j < M; j++) prev_row[j] = matrix[start_row - 1][j];
    }
    for (int j = 0; j < M; j++) curr_row[j] = matrix[start_row][j];

    // PHASE 1: COMPUTE
    for (int i = start_row; i < end_row; i++) {
        int* next_row = new int[M]();
        if (i + 1 < N) {
            for (int j = 0; j < M; j++) next_row[j] = matrix[i + 1][j];
        }

        int* computed_row = new int[M]();
        for (int j = 1; j < M - 1; j++) {
            computed_row[j] = 
                prev_row[j - 1] * KERNEL[0][0] + prev_row[j] * KERNEL[0][1] + prev_row[j + 1] * KERNEL[0][2] +
                curr_row[j - 1] * KERNEL[1][0] + curr_row[j] * KERNEL[1][1] + curr_row[j + 1] * KERNEL[1][2] +
                next_row[j - 1] * KERNEL[2][0] + next_row[j] * KERNEL[2][1] + next_row[j + 1] * KERNEL[2][2];
        }
        computed_row[0] = curr_row[0];
        computed_row[M - 1] = curr_row[M - 1];

        bool is_top_boundary    = (i == start_row && id > 0);
        bool is_bottom_boundary = (i == end_row - 1 && id < P - 1);

        if (is_top_boundary && is_bottom_boundary) {
            for (int j = 0; j < M; j++) new_top[j] = computed_row[j];
            delete[] computed_row;
        } else if (is_top_boundary) {
            for (int j = 0; j < M; j++) new_top[j] = computed_row[j];
            delete[] computed_row;
        } else if (is_bottom_boundary) {
            for (int j = 0; j < M; j++) new_bottom[j] = computed_row[j];
            delete[] computed_row;
        } else {
            // Internal rows written directly into matrix
            for (int j = 0; j < M; j++) matrix[i][j] = computed_row[j];
            delete[] computed_row;
        }

        delete[] prev_row;
        prev_row = curr_row;
        curr_row = next_row;
    }
    delete[] prev_row;
    delete[] curr_row;

    // PHASE 2: SYNCHRONIZATION BARRIER
    sync_barrier.arrive_and_wait();

    // PHASE 3: WRITE BOUNDARIES INTO MATRIX
    bool is_top_boundary    = (id > 0);
    bool is_bottom_boundary = (id < P - 1);
    
    if (is_top_boundary && is_bottom_boundary && (end_row - start_row == 1)) {
        for (int j = 0; j < M; j++) matrix[start_row][j] = new_top[j];
    } else {
        if (is_top_boundary) {
            for (int j = 0; j < M; j++) matrix[start_row][j] = new_top[j];
        }
        if (is_bottom_boundary) {
            for (int j = 0; j < M; j++) matrix[end_row - 1][j] = new_bottom[j];
        }
    }

    delete[] new_top;
    delete[] new_bottom;
}

// Parallel in-place convolution coordinator
void parallel_convolution(int N, int M, int** matrix, int P) {
    if (N < 3 || M < 3) return;
    int activeP = min(P, N - 2);
    if (activeP <= 1) {
        sequential_convolution(N, M, matrix);
        return;
    }
    std::barrier sync_barrier(activeP);
    vector<thread> threads;
    threads.reserve(activeP);
    for (int i = 0; i < activeP; i++) {
        threads.emplace_back(parallel_worker, i, activeP, N, M, matrix, ref(sync_barrier));
    }
    for (auto& t : threads) {
        t.join();
    }
}

// Measure execution time in milliseconds
template <typename Func>
double measure_ms(Func&& fn) {
    auto t0 = chrono::high_resolution_clock::now();
    fn();
    auto t1 = chrono::high_resolution_clock::now();
    return chrono::duration<double, milli>(t1 - t0).count();
}

// --- MAIN ENTRY ---
int main(int argc, char** argv) {
    ios_base::sync_with_stdio(false);
    cin.tie(NULL);

    string mode = "all";
    int P = 4;
    string filename = "date.txt";

    // Flexible command-line parsing:
    // Syntax 1: ./main <threads> [date.txt]
    // Syntax 2: ./main <mode> <threads> [date.txt]
    if (argc >= 2) {
        string arg1 = argv[1];
        if (isdigit(arg1[0])) {
            P = stoi(arg1);
            mode = (P == 1) ? "sequential" : "parallel";
            if (argc >= 3) filename = argv[2];
        } else {
            mode = arg1;
            if (argc >= 3) P = stoi(argv[2]);
            if (argc >= 4) filename = argv[3];
        }
    }

    int N = 0, M = 0;
    int** matrix = read_matrix(filename, N, M);
    if (!matrix) {
        cerr << "Error: Could not read " << filename << "\n";
        return 1;
    }

    // MODE: all / verify (For live demonstration to the lab assistant)
    if (mode == "all" || mode == "verify") {
        cout << "Matrix: " << N << " x " << M << " | Kernel: 3 x 3 | Threads: " << P
             << " (In-Place O(M) Aux Space)\n";
        cout << "--------------------------------------------------\n";

        // 1. Run Sequential
        double msSeq = measure_ms([&]() { sequential_convolution(N, M, matrix); });
        cout << "[Sequential]          Time: " << fixed << setprecision(4) << msSeq << " ms\n";
        write_matrix("output_seq.txt", matrix, N, M);
        write_matrix("output.txt", matrix, N, M);
        free_matrix(matrix, N);

        // 2. Re-read original matrix and run Parallel (Variant 2B)
        matrix = read_matrix(filename, N, M);
        double msPar = measure_ms([&]() { parallel_convolution(N, M, matrix, P); });
        write_matrix("output.txt", matrix, N, M);

        bool match = verify_files_equal("output_seq.txt", "output.txt");
        double speedup = (msPar > 0) ? (msSeq / msPar) : 1.0;
        cout << "[Parallel (Barrier)]  Time: " << fixed << setprecision(4) << msPar
             << " ms | Match: " << (match ? "true" : "false")
             << " | Speedup: " << setprecision(2) << speedup << "x\n";
        cout << "--------------------------------------------------\n";
        cout << "Output saved to output.txt\n";

        free_matrix(matrix, N);
        return 0;
    }

    // MODE: Single execution (Used by automated benchmark script run_benchmark.ps1)
    double duration = measure_ms([&]() {
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
    ifstream fseq("output_seq.txt");
    if (fseq.good()) {
        fseq.close();
        if (!verify_files_equal("output_seq.txt", "output.txt")) {
            cerr << "CORRECTNESS ERROR: Result does not match output_seq.txt!\n";
            free_matrix(matrix, N);
            return 1;
        }
    }

    // Output pure execution time in ms (for automated script parsing)
    cout << fixed << setprecision(4) << duration << "\n";

    free_matrix(matrix, N);
    return 0;
}
