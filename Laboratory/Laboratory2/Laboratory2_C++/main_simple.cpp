#include <iostream>
#include <thread>
#include <barrier>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <vector>

using namespace std;

// --- CONSTANTS ---
const int KERNEL[3][3] = {
    { 0, -1,  0},
    {-1,  5, -1},
    { 0, -1,  0}
};

// --- MEMORY MANAGEMENT ---
int** allocate_matrix(int N, int M) {
    int** matrix = new int*[N];
    for (int i = 0; i < N; i++) {
        matrix[i] = new int[M]();
    }
    return matrix;
}

void free_matrix(int** matrix, int N) {
    for (int i = 0; i < N; i++) {
        delete[] matrix[i];
    }
    delete[] matrix;
}

void copy_matrix(int** src, int** dest, int N, int M) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            dest[i][j] = src[i][j];
        }
    }
}

// --- FILE I/O & UTILS ---
int** read_matrix(const string& filename, int& N, int& M) {
    ifstream fin(filename);
    if (!fin) return nullptr;
    fin >> N >> M;
    int** matrix = allocate_matrix(N, M);
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            fin >> matrix[i][j];
        }
    }
    return matrix;
}

void write_matrix(const string& filename, int** matrix, int N, int M) {
    ofstream fout(filename);
    fout << N << " " << M << "\n";
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            fout << matrix[i][j] << " ";
        }
        fout << "\n";
    }
}

bool verify_matrices(int** m1, int** m2, int N, int M) {
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < M; j++) {
            if (m1[i][j] != m2[i][j]) return false;
        }
    }
    return true;
}

// --- SEQUENTIAL CONVOLUTION ---
void sequential_convolution(int N, int M, int** matrix) {
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
        delete[] matrix[i]; // Free old row
        matrix[i] = computed_row;
        
        // Shift pointers
        if (i > 1) delete[] prev_row; // Free detached buffer from previous step
        prev_row = curr_row;
        curr_row = new int[M]; // Detach next row to become current
        for (int j = 0; j < M; j++) curr_row[j] = next_row[j];
    }
    delete[] prev_row;
    delete[] curr_row;
}

// --- PARALLEL WORKER (VARIANT 2B) ---
void parallel_worker(int id, int P, int N, int M, int** matrix, std::barrier<>& sync_barrier) {
    int start_row = 1 + id * (N - 2) / P;
    int end_row = 1 + (id + 1) * (N - 2) / P;
    if (id == P - 1) end_row = N - 1;

    if (start_row >= end_row) {
        sync_barrier.arrive_and_wait();
        return;
    }

    // Allocate 1D temporary arrays (O(M) space)
    int* prev_row = new int[M]();
    int* curr_row = new int[M]();
    int* new_top = new int[M]();
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

        bool is_top_boundary = (i == start_row && id > 0);
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
            // Write directly into matrix (Variant 2b)
            for (int j = 0; j < M; j++) matrix[i][j] = computed_row[j];
            delete[] computed_row;
        }

        delete[] prev_row;
        prev_row = curr_row;
        curr_row = next_row;
    }
    delete[] prev_row;
    delete[] curr_row;

    // PHASE 2: SYNCHRONIZE
    sync_barrier.arrive_and_wait();

    // PHASE 3: UPDATE BOUNDARIES
    bool is_top_boundary = (id > 0);
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

    // Cleanup thread local buffers
    delete[] new_top;
    delete[] new_bottom;
}

// --- BENCHMARK RUNNER ---
int** run_benchmarks(int N, int M, int P, int** original_matrix) {
    long long total_seq_time = 0, total_par_time = 0;
    int** final_result = nullptr;

    cout << "Running 10 benchmark iterations [N=" << N << ", M=" << M << ", P=" << P << "]...\n";

    for (int run = 0; run < 10; run++) {
        // Sequential
        int** seq_matrix = allocate_matrix(N, M);
        copy_matrix(original_matrix, seq_matrix, N, M);
        
        auto t1 = chrono::high_resolution_clock::now();
        sequential_convolution(N, M, seq_matrix);
        auto t2 = chrono::high_resolution_clock::now();
        total_seq_time += chrono::duration_cast<chrono::milliseconds>(t2 - t1).count();
        
        if (run == 0) {
            final_result = allocate_matrix(N, M);
            copy_matrix(seq_matrix, final_result, N, M);
        }

        // Parallel
        int** par_matrix = allocate_matrix(N, M);
        copy_matrix(original_matrix, par_matrix, N, M);
        
        std::barrier sync_barrier(P);
        vector<thread> threads;
        
        auto t3 = chrono::high_resolution_clock::now();
        for (int i = 0; i < P; i++) {
            threads.emplace_back(parallel_worker, i, P, N, M, par_matrix, ref(sync_barrier));
        }
        for (auto& t : threads) t.join();
        auto t4 = chrono::high_resolution_clock::now();
        total_par_time += chrono::duration_cast<chrono::milliseconds>(t4 - t3).count();

        // Verification
        if (!verify_matrices(seq_matrix, par_matrix, N, M)) {
            cout << "  -> Run " << run + 1 << " FAILED: Matrix mismatch detected!\n";
        }

        free_matrix(seq_matrix, N);
        free_matrix(par_matrix, N);
    }

    cout << "  => Avg Sequential Time: " << fixed << setprecision(2) << (total_seq_time / 10.0) << " ms\n";
    cout << "  => Avg Parallel Time:   " << fixed << setprecision(2) << (total_par_time / 10.0) << " ms\n\n";
    
    return final_result;
}

// --- MAIN ENTRY ---
int main(int argc, char** argv) {
    if (argc < 2) {
        cout << "Usage: ./main <num_threads> [filename]\n";
        return 1;
    }
    
    int P = stoi(argv[1]);
    string filename = (argc > 2) ? argv[2] : "date.txt";
    int N, M;
    
    int** original_matrix = read_matrix(filename, N, M);
    if (!original_matrix) {
        cerr << "Error: Could not read " << filename << "\n";
        return 1;
    }

    int** final_result = run_benchmarks(N, M, P, original_matrix);
    
    write_matrix("output.txt", final_result, N, M);
    cout << "Benchmark complete. Results saved to output.txt\n";

    // Free memory
    free_matrix(original_matrix, N);
    free_matrix(final_result, N);

    return 0;
}
