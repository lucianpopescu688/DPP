// =============================================================================
// DPP Laboratory 2 - In-Place Convolution with Barrier Synchronization
// Language: Java 25 | Compiled with: javac --release 25
// =============================================================================

import java.io.*;
import java.util.*;
import java.util.concurrent.CyclicBarrier;

public class Main {

    // --- CONSTANTS ---
    // Fixed 3x3 sharpening kernel
    static final int[][] KERNEL = {
        { 0, -1,  0},
        {-1,  5, -1},
        { 0, -1,  0}
    };

    // --- RECORD: Encapsulates matrix data and dimensions ---
    record MatrixData(int[][] data, int rows, int cols) {
        static MatrixData fromFile(String filename) {
            var f = new File(filename);
            if (!f.exists()) f = new File("..", filename);
            if (!f.exists()) f = new File("../..", filename);
            if (!f.exists()) f = new File("../../..", filename);
            if (!f.exists()) return null;
            try (var br = new BufferedReader(new FileReader(f))) {
                String line = br.readLine();
                while (line != null && line.isBlank()) line = br.readLine();
                if (line == null) return null;

                var st = new StringTokenizer(line);
                int n = Integer.parseInt(st.nextToken());
                int m = Integer.parseInt(st.nextToken());

                int[][] matrix = new int[n][m];
                for (int i = 0; i < n; i++) {
                    while (!st.hasMoreTokens()) {
                        line = br.readLine();
                        if (line == null) break;
                        st = new StringTokenizer(line);
                    }
                    for (int j = 0; j < m; j++) {
                        matrix[i][j] = Integer.parseInt(st.nextToken());
                    }
                }
                return new MatrixData(matrix, n, m);
            } catch (Exception ignored) {
                return null;
            }
        }

        void writeToFile(String filename) {
            try (var bw = new BufferedWriter(new FileWriter(filename))) {
                bw.write(rows + " " + cols);
                bw.newLine();
                for (int i = 0; i < rows; i++) {
                    var sb = new StringBuilder(cols * 6);
                    for (int j = 0; j < cols; j++) {
                        sb.append(data[i][j]);
                        if (j + 1 < cols) sb.append(' ');
                    }
                    bw.write(sb.toString());
                    bw.newLine();
                }
            } catch (Exception e) {
                System.err.println("Error writing " + filename);
            }
        }
    }

    // --- FILE VERIFICATION ---
    // Compares two output files value-by-value without allocating a second matrix
    static boolean verifyFilesEqual(String file1, String file2) {
        var f1 = new File(file1);
        if (!f1.exists()) f1 = new File("..", file1);
        if (!f1.exists()) f1 = new File("../..", file1);
        if (!f1.exists()) f1 = new File("../../..", file1);

        var f2 = new File(file2);
        if (!f2.exists()) f2 = new File("..", file2);
        if (!f2.exists()) f2 = new File("../..", file2);
        if (!f2.exists()) f2 = new File("../../..", file2);

        if (!f1.exists() || !f2.exists()) return false;

        try (var br1 = new BufferedReader(new FileReader(f1));
             var br2 = new BufferedReader(new FileReader(f2))) {
            String l1, l2;
            while ((l1 = br1.readLine()) != null) {
                l2 = br2.readLine();
                if (l2 == null || !l1.strip().equals(l2.strip())) return false;
            }
            return br2.readLine() == null;
        } catch (Exception ignored) {
            return false;
        }
    }

    // --- CONVOLUTION HELPER ---
    // Applies the 3x3 kernel to a single interior pixel
    static int applyKernel(int[] prev, int[] curr, int[] next, int j) {
        return prev[j - 1] * KERNEL[0][0] + prev[j] * KERNEL[0][1] + prev[j + 1] * KERNEL[0][2] +
               curr[j - 1] * KERNEL[1][0] + curr[j] * KERNEL[1][1] + curr[j + 1] * KERNEL[1][2] +
               next[j - 1] * KERNEL[2][0] + next[j] * KERNEL[2][1] + next[j + 1] * KERNEL[2][2];
    }

    // --- SEQUENTIAL CONVOLUTION (O(M) AUXILIARY SPACE) ---
    // Processes rows top-to-bottom with only prev_row and curr_row as 1D buffers
    static void sequentialConvolution(int N, int M, int[][] matrix) {
        if (N < 3 || M < 3) return;

        int[] prevRow = Arrays.copyOf(matrix[0], M);
        int[] currRow = Arrays.copyOf(matrix[1], M);

        for (int i = 1; i < N - 1; i++) {
            int[] nextRow = matrix[i + 1];
            int[] computedRow = new int[M];

            for (int j = 1; j < M - 1; j++) {
                computedRow[j] = applyKernel(prevRow, currRow, nextRow, j);
            }
            // Border pixels preserved
            computedRow[0] = currRow[0];
            computedRow[M - 1] = currRow[M - 1];

            // In-place row replacement
            matrix[i] = computedRow;

            // Shift buffers forward
            prevRow = currRow;
            currRow = Arrays.copyOf(nextRow, M);
        }
    }

    // --- PARALLEL WORKER
    // Each thread computes its assigned rows using O(M) auxiliary space.
    // Boundary rows are buffered and written only after barrier synchronization.
    static class ParallelWorker implements Runnable {
        private final int id, P, N, M;
        private final int[][] matrix;
        private final CyclicBarrier barrier;

        ParallelWorker(int id, int P, int N, int M, int[][] matrix, CyclicBarrier barrier) {
            this.id = id; this.P = P; this.N = N; this.M = M;
            this.matrix = matrix; this.barrier = barrier;
        }

        @Override
        public void run() {
            // Compute row range (horizontal line-based decomposition)
            int innerRows = N - 2;
            int startRow = 1 + id * innerRows / P;
            int endRow   = (id == P - 1) ? N - 1 : 1 + (id + 1) * innerRows / P;

            if (startRow >= endRow) {
                try { barrier.await(); } catch (Exception ignored) {}
                return;
            }

            // Strictly O(M) auxiliary space per thread - allocated ONCE
            int[] prevRow     = new int[M];
            int[] currRow     = new int[M];
            int[] computedRow = new int[M];
            int[] newTop      = new int[M];
            int[] newBottom   = new int[M];

            if (startRow > 0) System.arraycopy(matrix[startRow - 1], 0, prevRow, 0, M);
            System.arraycopy(matrix[startRow], 0, currRow, 0, M);

            // PHASE 1: COMPUTE all rows in this thread's range
            for (int i = startRow; i < endRow; i++) {
                int[] nextRow = (i + 1 < N) ? matrix[i + 1] : null;

                for (int j = 1; j < M - 1; j++) {
                    computedRow[j] = applyKernel(prevRow, currRow, nextRow, j);
                }
                computedRow[0]     = currRow[0];
                computedRow[M - 1] = currRow[M - 1];

                // Determine boundary status
                boolean isTopBoundary    = (i == startRow && id > 0);
                boolean isBottomBoundary = (i == endRow - 1 && id < P - 1);

                if (isTopBoundary && isBottomBoundary) {
                    System.arraycopy(computedRow, 0, newTop, 0, M);
                } else if (isTopBoundary) {
                    System.arraycopy(computedRow, 0, newTop, 0, M);
                } else if (isBottomBoundary) {
                    System.arraycopy(computedRow, 0, newBottom, 0, M);
                } else {
                    // Interior row: write directly (safe - no adjacent thread reads this)
                    System.arraycopy(computedRow, 0, matrix[i], 0, M);
                }

                if (i + 1 < endRow) {
                    System.arraycopy(currRow, 0, prevRow, 0, M);
                    System.arraycopy(nextRow, 0, currRow, 0, M);
                }
            }

            try { barrier.await(); } catch (Exception e) { e.printStackTrace(); }

            boolean isTopBoundary    = (id > 0);
            boolean isBottomBoundary = (id < P - 1);

            if (isTopBoundary && isBottomBoundary && (endRow - startRow == 1)) {
                matrix[startRow] = newTop;
            } else {
                if (isTopBoundary) matrix[startRow] = newTop;
                if (isBottomBoundary) matrix[endRow - 1] = newBottom;
            }
        }
    }

    // Parallel in-place convolution coordinator
    static void parallelConvolution(int N, int M, int[][] matrix, int P) {
        if (N < 3 || M < 3) return;
        int activeP = Math.min(P, N - 2);
        if (activeP <= 1) {
            sequentialConvolution(N, M, matrix);
            return;
        }

        var barrier = new CyclicBarrier(activeP);
        var threads = new Thread[activeP];
        for (int i = 0; i < activeP; i++) {
            threads[i] = new Thread(new ParallelWorker(i, activeP, N, M, matrix, barrier));
            threads[i].start();
        }
        for (int i = 0; i < activeP; i++) {
            try { threads[i].join(); } catch (InterruptedException ignored) {}
        }
    }

    // --- MAIN ENTRY ---
    public static void main(String[] args) {
        String mode = "all";
        int P = 4;
        String filename = "date.txt";

        // Syntax 1: java Main <threads> [date.txt]
        // Syntax 2: java Main <mode> <threads> [date.txt]
        if (args.length >= 1) {
            String arg0 = args[0];
            if (Character.isDigit(arg0.charAt(0))) {
                P = Integer.parseInt(arg0);
                mode = (P == 1) ? "sequential" : "parallel";
                if (args.length >= 2) filename = args[1];
            } else {
                mode = arg0.toLowerCase();
                if (args.length >= 2) P = Integer.parseInt(args[1]);
                if (args.length >= 3) filename = args[2];
            }
        }

        var matrixData = MatrixData.fromFile(filename);
        if (matrixData == null) {
            System.err.println("Error: Could not read " + filename);
            return;
        }
        int[][] matrix = matrixData.data();
        int N = matrixData.rows(), M = matrixData.cols();

        // MODE: all / verify (For live demonstration to the lab assistant)
        if (mode.equals("all") || mode.equals("verify")) {
            System.out.printf("Matrix: %d x %d | Kernel: 3 x 3 | Threads: %d (In-Place O(M) Aux Space)%n", N, M, P);
            System.out.println("--------------------------------------------------");

            // 1. Sequential run
            long t0 = System.nanoTime();
            sequentialConvolution(N, M, matrix);
            double msSeq = (System.nanoTime() - t0) / 1e6;
            System.out.printf("[Sequential]          Time: %.4f ms%n", msSeq);
            new MatrixData(matrix, N, M).writeToFile("output_seq.txt");
            new MatrixData(matrix, N, M).writeToFile("output.txt");

            // 2. Re-read original matrix and run Parallel
            matrixData = MatrixData.fromFile(filename);
            matrix = matrixData.data();
            t0 = System.nanoTime();
            parallelConvolution(N, M, matrix, P);
            double msPar = (System.nanoTime() - t0) / 1e6;
            new MatrixData(matrix, N, M).writeToFile("output.txt");

            boolean match = verifyFilesEqual("output_seq.txt", "output.txt");
            double speedup = (msPar > 0) ? (msSeq / msPar) : 1.0;
            System.out.printf("[Parallel (Barrier)]  Time: %.4f ms | Match: %b | Speedup: %.2fx%n", msPar, match, speedup);
            System.out.println("--------------------------------------------------");
            System.out.println("Output saved to output.txt");
            return;
        }

        // MODE: Single execution (Used by automated benchmark script run_benchmark.ps1)
        long t0 = System.nanoTime();
        if (mode.equals("sequential") || P == 1) {
            sequentialConvolution(N, M, matrix);
        } else {
            parallelConvolution(N, M, matrix, P);
        }
        double durationMs = (System.nanoTime() - t0) / 1e6;

        var result = new MatrixData(matrix, N, M);
        result.writeToFile("output.txt");
        if (mode.equals("sequential") || P == 1) {
            result.writeToFile("output_seq.txt");
        }

        var seqFile = new File("output_seq.txt");
        if (seqFile.exists()) {
            if (!verifyFilesEqual("output_seq.txt", "output.txt")) {
                System.err.println("CORRECTNESS ERROR: Result does not match output_seq.txt!");
                System.exit(1);
            }
        }

        System.out.printf(Locale.US, "%.4f%n", durationMs);
    }
}
