import java.util.ArrayList;
import java.util.Random;

public class Main {

    // ---------- 1. Generate a vector with n random real values in [1, 100) ----------
    static ArrayList<Double> generateVector(int n) {
        Random random = new Random();
        ArrayList<Double> v = new ArrayList<>(n);

        for (int i = 0; i < n; i++) {
            v.add(1 + 99 * random.nextDouble()); // result[i]=random.nextInt(1000);
        }
        return v;
    }

    // Create a vector of size n pre-filled with 0.0 so that set(i, ...) is valid.
    static ArrayList<Double> newVector(int n) {
        ArrayList<Double> v = new ArrayList<>(n);
        for (int i = 0; i < n; i++) {
            v.add(0.0);
        }
        return v;
    }

    // ---------- 2. The two operations on one pair of elements ----------
    static double add(double a, double b) {
        return a + b;
    }

    static double sqrtOfSquares(double a, double b) {
        return Math.sqrt(a * a * a * a + b * b * b * b);
    }

    static double complexOp(double a, double b) {
        return Math.pow(a, b);
    }

    static double complexOp2(double a, double b) {
        double result = 0;
        result = (a % b + a) * b - 1;
        return result;
    }

    // The operation used by ALL versions (sequential, linear, cyclic).
    // To switch the experiment, change only this line:
    //   return add(a, b);   or   return sqrtOfSquares(a, b);
    static double operation(double a, double b) {
        return complexOp2(a, b);
    }

    // ---------- 3. Print a vector ----------
    static void printVector(String name, ArrayList<Double> v) {
        System.out.println(name + " = " + v);
    }

    // ---------- 4. Sequential version ----------
    static void sequential(ArrayList<Double> a, ArrayList<Double> b, ArrayList<Double> c) {
        for (int i = 0; i < c.size(); i++) {
            c.set(i, operation(a.get(i), b.get(i)));
        }
    }

    // ---------- 5. Linear (block) distribution ----------
    // Runnable = the "note" with the work to do. Each thread gets one segment: start .. end-1
    static class LinearTask implements Runnable {
        private final ArrayList<Double> a, b, c;
        private final int start, end;

        LinearTask(ArrayList<Double> a, ArrayList<Double> b, ArrayList<Double> c, int start, int end) {
            this.a = a;
            this.b = b;
            this.c = c;
            this.start = start;
            this.end = end;
        }

        @Override
        public void run() {
            for (int i = start; i < end; i++) {
                c.set(i, operation(a.get(i), b.get(i)));
            }
        }
    }

    static void linear(ArrayList<Double> a, ArrayList<Double> b, ArrayList<Double> c, int p) throws InterruptedException {
        int n = c.size();
        int size = n / p;    // every thread gets at least 'size' elements
        int extra = n % p;   // the first 'extra' threads get one more element

        Thread[] threads = new Thread[p];
        int start = 0;
        for (int id = 0; id < p; id++) {
            int end = start + size;
            if (id < extra) {
                end++;
            }
            threads[id] = new Thread(new LinearTask(a, b, c, start, end));
            start = end;
        }
        startAndJoin(threads);
    }

    // ---------- 6. Cyclic distribution ----------
    // Thread 'id' processes the indices id, id + p, id + 2p, ...
    static class CyclicTask implements Runnable {
        private final ArrayList<Double> a, b, c;
        private final int id, p;

        CyclicTask(ArrayList<Double> a, ArrayList<Double> b, ArrayList<Double> c, int id, int p) {
            this.a = a;
            this.b = b;
            this.c = c;
            this.id = id;
            this.p = p;
        }

        @Override
        public void run() {
            for (int i = id; i < a.size(); i = i + p) {
                c.set(i, operation(a.get(i), b.get(i)));
            }
        }
    }

    static void cyclic(ArrayList<Double> a, ArrayList<Double> b, ArrayList<Double> c, int p) throws InterruptedException {
        Thread[] threads = new Thread[p];
        for (int i = 0; i < p; i++) {
            threads[i] = new Thread(new CyclicTask(a, b, c, i, p));
        }
        startAndJoin(threads);
    }

    // ---------- Start ALL threads first, then wait for ALL of them ----------
    static void startAndJoin(Thread[] threads) throws InterruptedException {
        for (Thread t : threads) {
            // start() creates a NEW thread, and that new thread executes run().
            // If we called t.run() instead, no new thread would exist:
            // run() would be executed by the main thread, one task after another.
            t.start();
        }
        for (Thread t : threads) {
            // join() = "main waits here until thread t has finished".
            // Without it, main could use vector C while the threads are still writing in it.
            // Starting everything first and joining afterwards keeps the threads running together.
            t.join();
        }
    }

    // ---------- 7. Measure the three versions and print one line ----------
    static void measure(ArrayList<Double> a, ArrayList<Double> b, ArrayList<Double> c, int p) throws InterruptedException {
        double seqMs = 0, linMs = 0, cycMs = 0;

        for (int round = 0; round < 2; round++) {   // round 0 = warm-up, round 1 = kept
            long t0 = System.nanoTime();
            long t1 = System.nanoTime();
            sequential(a, b, c);
            seqMs = (System.nanoTime() - t0) / 1e6;

            t0 = System.nanoTime();
            linear(a, b, c, p);
            linMs = (System.nanoTime() - t0) / 1e6;

            t1 = System.nanoTime();
            cyclic(a, b, c, p);
            cycMs = (System.nanoTime() - t1) / 1e6;
        }

        System.out.printf("n=%-9d p=%-3d sequential=%9.3f ms  linear=%9.3f ms  cyclic=%9.3f ms%n",
                a.size(), p, seqMs, linMs, cycMs);
    }

    // ---------- Check correctness on a small example: n = 10, p = 3 ----------
    static void verify() throws InterruptedException {
        int n = 10, p = 3;
        ArrayList<Double> a = generateVector(n);
        ArrayList<Double> b = generateVector(n);
        ArrayList<Double> expected = newVector(n);
        ArrayList<Double> c = newVector(n);

        sequential(a, b, expected);
        linear(a, b, c, p);
        boolean linearOk = expected.equals(c);

        for (int i = 0; i < n; i++) {
            c.set(i, 0.0);
        }
        cyclic(a, b, c, p);
        boolean cyclickOk = expected.equals(c);

        printVector("A         ", a);
        printVector("B         ", b);
        printVector("sequential", expected);
        printVector("cyclic    ", c);
        System.out.println("linear correct: " + linearOk);
        System.out.println("cyclic correct: " + cyclickOk);
        System.out.println();
    }

    public static void main(String[] args) throws InterruptedException {
        System.out.println("Processors: " + Runtime.getRuntime().availableProcessors());
        verify();

        int[] sizes = {1000, 10000, 100000, 1000000, 10000000};
        int[] threadCounts = {4, 6, 8, 16};

        for (int n : sizes) {
            ArrayList<Double> a = generateVector(n);
            ArrayList<Double> b = generateVector(n);
            ArrayList<Double> c = newVector(n);
            for (int p : threadCounts) {
                measure(a, b, c, p);
            }
            System.out.println();
        }
    }
}
