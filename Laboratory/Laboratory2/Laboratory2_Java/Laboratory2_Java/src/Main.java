import java.io.*;
import java.util.*;
import java.util.concurrent.CyclicBarrier;

public class Main {

    // --- CONSTANTS ---
    // Standard 3x3 sharpening kernel: center = 5, orthogonal neighbors = -1
    static final int[][] KERNEL = {
        { 0, -1,  0},
        {-1,  5, -1},
        { 0, -1,  0}
    };

    // --- FILE I/O ---
    static int[][] readMatrix(String filename) {
        File f = new File(filename);
        if (!f.exists()) f = new File("..", filename);
        if (!f.exists()) f = new File("../..", filename);
        if (!f.exists()) return null;

        try (BufferedReader br = new BufferedReader(new FileReader(f))) {
            String line = br.readLine();
            while (line != null && line.isBlank()) line = br.readLine();
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
        } catch (Exception ignored) {
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
        } catch (Exception ignored) {}
    }

    // Verify output files match byte-for-byte without extra matrix allocation
    static boolean verifyFilesEqual(String file1, String file2) {
        File f1 = new File(file1);
        if (!f1.exists()) f1 = new File("..", file1);
        File f2 = new File(file2);
        if (!f2.exists()) f2 = new File("..", file2);
        if (!f1.exists() || !f2.exists()) return false;

        try (BufferedReader br1 = new BufferedReader(new FileReader(f1));
             BufferedReader br2 = new BufferedReader(new FileReader(f2))) {
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
    static int applyKernel(int[] prev, int[] curr, int[] next, int j) {
        return prev[j - 1] * KERNEL[0][0] + prev[j] * KERNEL[0][1] + prev[j + 1] * KERNEL[0][2] +
               curr[j - 1] * KERNEL[1][0] + curr[j] * KERNEL[1][1] + curr[j + 1] * KERNEL[1][2] +
               next[j - 1] * KERNEL[2][0] + next[j] * KERNEL[2][1] + next[j + 1] * KERNEL[2][2];
    }

    // --- SEQUENTIAL IN-PLACE CONVOLUTION (O(M) AUXILIARY SPACE) ---
    static void sequentialConvolution(int N, int M, int[][] matrix) {
        if (N < 3 || M < 3) return;

        int[] prev = Arrays.copyOf(matrix[0], M);
        int[] curr = Arrays.copyOf(matrix[1], M);
        int[] comp = new int[M];

        for (int i = 1; i < N - 1; i++) {
            int[] next = matrix[i + 1];
            for (int j = 1; j < M - 1; j++) {
                comp[j] = applyKernel(prev, curr, next, j);
            }
            comp[0] = curr[0];
            comp[M - 1] = curr[M - 1];

            // Shift buffers forward before writing in-place
            System.arraycopy(curr, 0, prev, 0, M);
            System.arraycopy(next, 0, curr, 0, M);
            System.arraycopy(comp, 0, matrix[i], 0, M);
        }
    }

    // --- PARALLEL WORKER (VARIANT 2B - BARRIER + DELAYED BOUNDARY WRITE) ---
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
            int innerRows = N - 2;
            int startRow  = 1 + id * innerRows / P;
            int endRow    = (id == P - 1) ? N - 1 : 1 + (id + 1) * innerRows / P;

            if (startRow >= endRow) {
                try { barrier.await(); } catch (Exception ignored) {}
                return;
            }

            // 1D temporary arrays allocated ONCE per thread (Strictly O(M) auxiliary space)
            int[] prev      = new int[M];
            int[] curr      = new int[M];
            int[] comp      = new int[M];
            int[] newTop    = new int[M];
            int[] newBottom = new int[M];

            if (startRow > 0) System.arraycopy(matrix[startRow - 1], 0, prev, 0, M);
            System.arraycopy(matrix[startRow], 0, curr, 0, M);

            // PHASE 1: COMPUTE all assigned rows
            for (int i = startRow; i < endRow; i++) {
                int[] next = (i + 1 < N) ? matrix[i + 1] : null;

                for (int j = 1; j < M - 1; j++) {
                    comp[j] = applyKernel(prev, curr, next, j);
                }
                comp[0] = curr[0];
                comp[M - 1] = curr[M - 1];

                // Boundary rows are saved in buffers; interior rows written directly
                if (i == startRow && id > 0) {
                    System.arraycopy(comp, 0, newTop, 0, M);
                } else if (i == endRow - 1 && id < P - 1) {
                    System.arraycopy(comp, 0, newBottom, 0, M);
                } else {
                    System.arraycopy(comp, 0, matrix[i], 0, M);
                }

                if (i + 1 < endRow) {
                    System.arraycopy(curr, 0, prev, 0, M);
                    System.arraycopy(next, 0, curr, 0, M);
                }
            }

            // PHASE 2: SYNCHRONIZE
            try { barrier.await(); } catch (Exception ignored) {}

            // PHASE 3: WRITE BOUNDARY ROWS (safe after barrier)
            if (id > 0) {
                System.arraycopy(newTop, 0, matrix[startRow], 0, M);
            }
            if (id < P - 1 && (endRow - 1 != startRow || id == 0)) {
                System.arraycopy(newBottom, 0, matrix[endRow - 1], 0, M);
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
            try { threads[i].join(); } catch (InterruptedException ignored) {}
        }
    }

    // --- MAIN ENTRY ---
    public static void main(String[] args) {
        String mode = "all";
        int P = 4;
        String filename = "date.txt";

        // Clean, flexible argument parsing
        for (String arg : args) {
            if (Character.isDigit(arg.charAt(0))) P = Integer.parseInt(arg);
            else if (arg.equals("sequential") || arg.equals("parallel") || arg.equals("all") || arg.equals("verify")) mode = arg;
            else if (arg.endsWith(".txt")) filename = arg;
        }
        if (P == 1 && !mode.equals("all")) mode = "sequential";

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

            // 2. Parallel run
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

        // MODE: Single execution (Used by automated benchmark script)
        long t0 = System.nanoTime();
        if (mode.equals("sequential") || P == 1) {
            sequentialConvolution(N, M, matrix);
        } else {
            parallelConvolution(N, M, matrix, P);
        }
        double durationMs = (System.nanoTime() - t0) / 1e6;

        writeMatrix("output.txt", matrix);
        if (mode.equals("sequential") || P == 1) writeMatrix("output_seq.txt", matrix);

        // Verify against output_seq.txt if present
        File seqFile = new File("output_seq.txt");
        if (!seqFile.exists()) seqFile = new File("..", "output_seq.txt");
        if (seqFile.exists() && !verifyFilesEqual(seqFile.getPath(), "output.txt")) {
            System.err.println("CORRECTNESS ERROR: Result does not match output_seq.txt!");
            System.exit(1);
        }

        System.out.printf(Locale.US, "%.4f%n", durationMs);
    }
}
