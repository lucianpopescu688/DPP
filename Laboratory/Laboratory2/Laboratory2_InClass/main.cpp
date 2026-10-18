#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <mutex>

using namespace std;

double *a, *b, *c;
int n, p;
double global_sum = 0;
double global_sum_mutex = 0;
mutex sum_mutex;
mutex print_mutex;

void hello(int id)
{
    lock_guard<mutex> lock(print_mutex);
    cout << "Thread ID " << id << " is running." << endl;
}

void sequential(double *a, double *b, int n)
{
    double sum = 0;
    auto t0 = chrono::high_resolution_clock::now();

    for (int i = 0; i < n; i++)
    {
        sum += a[i] * b[i];
    }

    auto t1 = chrono::high_resolution_clock::now();
    cout << "Time taken for sequential execution: "
         << chrono::duration<double, std::micro>(t1 - t0).count()
         << " microseconds" << endl;
    cout << "Sequential sum: " << sum << endl;
}

void linear(void (*worker)(int id, int start, int end))
{
    int rest = n % p;
    int base = n / p;
    int start = 0;
    int end = 0;

    vector<thread> threads;
    for (int i = 0; i < p; i++)
    {
        end = start + base + (i < rest ? 1 : 0);
        threads.emplace_back(worker, i, start, end);
        start = end;
    }

    for (auto& t : threads)
    {
        t.join();
    }
}

void worker_liniar(int id, int start, int end)
{
    double local_sum = 0;
    for (int i = start; i < end; i++)
    {
        local_sum += a[i] * b[i];
    }
    c[id] = local_sum;
}

void worker_liniar_mutex(int id, int start, int end)
{
    double local_sum = 0;
    for (int i = start; i < end; i++)
    {
        local_sum += a[i] * b[i];
    }

    lock_guard<mutex> lock(sum_mutex);
    global_sum_mutex += local_sum;
}

int main(int argc, char *argv[])
{
    n = atoi(argv[1]);
    p = atoi(argv[2]);

    a = new double[n];
    b = new double[n];
    c = new double[p];

    for (int i = 0; i < n; i++)
    {
        a[i] = rand() % 100;
        b[i] = rand() % 100;
    }

    vector<thread> hello_threads;
    for (int i = 0; i < 20; i++)
    {
        hello_threads.emplace_back(hello, i);
    }
    for (auto& t : hello_threads)
    {
        t.join();
    }
    cout << endl;

    sequential(a, b, n);

    auto t0 = chrono::high_resolution_clock::now();
    linear(worker_liniar);

    for (int i = 0; i < p; i++)
    {
        global_sum += c[i];
    }
    auto t1 = chrono::high_resolution_clock::now();

    cout << "Time taken for linear execution: "
         << chrono::duration<double, std::micro>(t1 - t0).count()
         << " microseconds" << endl;
    cout << "Linear sum: " << global_sum << endl;

    t0 = chrono::high_resolution_clock::now();
    linear(worker_liniar_mutex);
    t1 = chrono::high_resolution_clock::now();

    cout << "Time taken for linear mutex execution: "
         << chrono::duration<double, std::micro>(t1 - t0).count()
         << " microseconds" << endl;
    cout << "Linear mutex sum: " << global_sum_mutex << endl;

    delete[] a;
    delete[] b;
    delete[] c;

    return 0;
}