#include <iostream>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <random>

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <vector_size>" << std::endl;
        return 1;
    }

    long long n = std::atoll(argv[1]);
    if (n <= 0) {
        std::cerr << "Vector size must be a positive integer." << std::endl;
        return 1;
    }

    std::vector<double> A(n);
    std::vector<double> B(n);
    std::vector<double> C(n);

    // Initialize vectors A and B with random real numbers
    // We do this before the timer starts to not affect the measured execution time
    // Using system_clock to seed to prevent MinGW crash with random_device
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::mt19937_64 rng(seed);
    std::uniform_real_distribution<double> dist(0.0, 100.0);
    
    for (long long i = 0; i < n; ++i) {
        A[i] = dist(rng);
        B[i] = dist(rng);
    }

    // Start the timer just before the actual computation begins
    auto start_time = std::chrono::high_resolution_clock::now();

    // Perform the vector addition: C[i] = A[i] + B[i]
    for (long long i = 0; i < n; ++i) {
        C[i] = A[i] + B[i];
    }

    // Stop the timer immediately after the computation finishes
    auto end_time = std::chrono::high_resolution_clock::now();
    
    // Calculate the elapsed time duration in milliseconds
    std::chrono::duration<double, std::milli> duration = end_time - start_time;

    // Print ONLY the execution time to the standard output so the script can parse it.
    std::cout << duration.count() << std::endl;

    return 0;
}