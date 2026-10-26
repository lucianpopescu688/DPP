import java.io.*;
import java.util.*;
import java.util.concurrent.CyclicBarrier;

public class Main {

    // --- CONSTANTS ---
    static final int[][] KERNEL = {
        { 0, -1,  0},
        {-1,  5, -1},
        { 0, -1,  0}
    };

    // --- FILE I/O & UTILS ---
    static int[][] readMatrix(String filename) {
        File f = new File(filename);
        if (!f.exists()) {
            File p = new File("..", filename);
            if (p.exists()) f = p;
        }
        try (BufferedReader br = new BufferedReader(new FileReader(f))) {
            String line = br.readLine();
            while (line != null && line.trim().isEmpty()) line = br.readLine();
            if (line == null) return null;

            StringTokenizer st = new StringTokenizer(line);
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
            return matrix;
        } catch (Exception e) {
            return null;
        }
    }

    static void writeMatrix(String filename, int[][] matrix) {
        try (BufferedWriter bw = new BufferedWriter(new FileWriter(filename))) {
            int n = matrix.length, m = matrix[0].length;
            bw.write(n + " " + m + "\n");
            for (int i = 0; i < n; i++) {
                StringBuilder sb = new StringBuilder(m * 6);
                for (int j = 0; j < m; j++) {
                    sb.append(matrix[i][j]).append(j + 1 < m ? " " : "");
                }
                bw.write(sb.toString());
                bw.newLine();
            }
        } catch (Exception e) {
            System.err.println("Error writing " + filename);
        }
    }

    static boolean verifyFilesEqual(String file1, String file2) {
        File f1 = new File(file1);
        File f2 = new File(file2);
        if (!f1.exists()) f1 = new File("..", file1);
        if (!f2.exists()) f2 = new File("..", file2);
        if (!f1.exists() || !f2.exists()) return false;

        try (BufferedReader br1 = new BufferedReader(new FileReader(f1));
             BufferedReader br2 = new BufferedReader(new FileReader(f2))) {
            String l1, l2;
            while ((l1 = br1.readLine()) != null) {
                l2 = br2.readLine();
                if (l2 == null || !l1.trim().equals(l2.trim())) return false;
            }
            return br2.readLine() == null;
        } catch (Exception e) {
            return false;
        }
    }

    // --- SEQUENTIAL CONVOLUTION (O(M) AUXILIARY SPACE) ---
    static void sequentialConvolution(int N, int M, int[][] matrix) {
        if (N < 3 || M < 3) return;

        int[] prevRow = Arrays.copyOf(matrix[0], M);
        int[] currRow = Arrays.copyOf(matrix[1], M);

        for (int i = 1; i < N - 1; i++) {
            int[] nextRow = matrix[i + 1];
            int[] computedRow = new int[M];

            for (int j = 1; j < M - 1; j++) {
                computedRow[j] = 
                    prevRow[j - 1] * KERNEL[0][0] + prevRow[j] * KERNEL[0][1] + prevRow[j + 1] * KERNEL[0][2] +
                    currRow[j - 1] * KERNEL[1][0] + currRow[j] * KERNEL[1][1] + currRow[j + 1] * KERNEL[1][2] +
                    nextRow[j - 1] * KERNEL[2][0] + nextRow[j] * KERNEL[2][1] + nextRow[j + 1] * KERNEL[2][2];
            }
            computedRow[0] = currRow[0];
            computedRow[M - 1] = currRow[M - 1];

            // In-place row assignment
            matrix[i] = computedRow;

            // Shift forward
            prevRow = currRow;
            currRow = Arrays.copyOf(nextRow, M);
        }
    }

    // --- PARALLEL WORKER (VARIANT 2B - BARRIER + DELAYED BOUNDARY WRITE) ---
    static class ParallelWorker implements Runnable {
        int id, P, N, M;
        int[][] matrix;
        CyclicBarrier barrier;

        public ParallelWorker(int id, int P, int N, int M, int[][] matrix, CyclicBarrier barrier) {
            this.id = id; this.P = P; this.N = N; this.M = M;
            this.matrix = matrix; this.barrier = barrier;
        }

        @Override
        public void run() {
            int innerRows = N - 2;
            int startRow = 1 + id * innerRows / P;
            int endRow   = 1 + (id + 1) * innerRows / P;
            if (id == P - 1) endRow = N - 1;

            if (startRow >= endRow) {
                try { barrier.await(); } catch (Exception e) {}
                return;
            }

            // Strictly O(M) auxiliary space per thread
            int[] prevRow   = new int[M];
            int[] currRow   = new int[M];
            int[] newTop    = new int[M];
            int[] newBottom = new int[M];

            if (startRow > 0) System.arraycopy(matrix[startRow - 1], 0, prevRow, 0, M);
            System.arraycopy(matrix[startRow], 0, currRow, 0, M);

            // PHASE 1: COMPUTE
            for (int i = startRow; i < endRow; i++) {
                int[] nextRow = new int[M];
                if (i + 1 < N) System.arraycopy(matrix[i + 1], 0, nextRow, 0, M);

                int[] computedRow = new int[M];
                for (int j = 1; j < M - 1; j++) {
                    computedRow[j] = 
                        prevRow[j - 1] * KERNEL[0][0] + prevRow[j] * KERNEL[0][1] + prevRow[j + 1] * KERNEL[0][2] +
                        currRow[j - 1] * KERNEL[1][0] + currRow[j] * KERNEL[1][1] + currRow[j + 1] * KERNEL[1][2] +
                        nextRow[j - 1] * KERNEL[2][0] + nextRow[j] * KERNEL[2][1] + nextRow[j + 1] * KERNEL[2][2];
                }
                computedRow[0] = currRow[0];
                computedRow[M - 1] = currRow[M - 1];

                boolean isTopBoundary    = (i == startRow && id > 0);
                boolean isBottomBoundary = (i == endRow - 1 && id < P - 1);

                if (isTopBoundary && isBottomBoundary) {
                    System.arraycopy(computedRow, 0, newTop, 0, M);
                } else if (isTopBoundary) {
                    System.arraycopy(computedRow, 0, newTop, 0, M);
                } else if (isBottomBoundary) {
                    System.arraycopy(computedRow, 0, newBottom, 0, M);
                } else {
                    matrix[i] = computedRow; // Internal rows written directly
                }

                prevRow = currRow;
                currRow = nextRow;
            }

            // PHASE 2: SYNCHRONIZATION BARRIER
            try { barrier.await(); } catch (Exception e) { e.printStackTrace(); }

            // PHASE 3: WRITE BOUNDARIES INTO MATRIX
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

        CyclicBarrier barrier = new CyclicBarrier(activeP);
        Thread[] threads = new Thread[activeP];
        for (int i = 0; i < activeP; i++) {
            threads[i] = new Thread(new ParallelWorker(i, activeP, N, M, matrix, barrier));
            threads[i].start();
        }
        for (int i = 0; i < activeP; i++) {
            try { threads[i].join(); } catch (InterruptedException e) {}
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

        int[][] matrix = readMatrix(filename);
        if (matrix == null) {
            System.err.println("Error: Could not read " + filename);
            return;
        }
        int N = matrix.length, M = matrix[0].length;

        // MODE: all / verify (For live demonstration to the lab assistant)
        if (mode.equals("all") || mode.equals("verify")) {
            System.out.printf("Matrix: %d x %d | Kernel: 3 x 3 | Threads: %d (In-Place O(M) Aux Space)%n", N, M, P);
            System.out.println("--------------------------------------------------");

            // 1. Sequential run
            long t0 = System.nanoTime();
            sequentialConvolution(N, M, matrix);
            double msSeq = (System.nanoTime() - t0) / 1e6;
            System.out.printf("[Sequential]          Time: %.4f ms%n", msSeq);
            writeMatrix("output_seq.txt", matrix);
            writeMatrix("output.txt", matrix);

            // 2. Re-read original matrix and run Parallel
            matrix = readMatrix(filename);
            t0 = System.nanoTime();
            parallelConvolution(N, M, matrix, P);
            double msPar = (System.nanoTime() - t0) / 1e6;
            writeMatrix("output.txt", matrix);

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

        writeMatrix("output.txt", matrix);
        if (mode.equals("sequential") || P == 1) {
            writeMatrix("output_seq.txt", matrix);
        }

        File seqFile = new File("output_seq.txt");
        if (seqFile.exists()) {
            if (!verifyFilesEqual("output_seq.txt", "output.txt")) {
                System.err.println("CORRECTNESS ERROR: Result does not match output_seq.txt!");
                System.exit(1);
            }
        }

        System.out.printf(Locale.US, "%.4f%n", durationMs);
    }
}
