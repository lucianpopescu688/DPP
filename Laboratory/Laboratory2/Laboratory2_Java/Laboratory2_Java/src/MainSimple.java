import java.io.*;
import java.util.*;
import java.util.concurrent.CyclicBarrier;

public class MainSimple {
    
    // --- CONSTANTS ---
    static final int[][] KERNEL = {
        { 0, -1,  0},
        {-1,  5, -1},
        { 0, -1,  0}
    };

    // --- MAIN ENTRY ---
    public static void main(String[] args) {
        if (args.length < 1) {
            System.out.println("Usage: java MainSimple <num_threads> [filename]");
            return;
        }
        
        int P = Integer.parseInt(args[0]);
        String filename = (args.length > 1) ? args[1] : "date.txt";
        int[] dims = new int[2]; // Used to extract N and M from file reader
        
        int[][] originalMatrix = readMatrix(filename, dims);
        if (originalMatrix == null) {
            System.err.println("Error: Could not read " + filename);
            return;
        }

        int N = dims[0], M = dims[1];
        int[][] finalResult = runBenchmarks(N, M, P, originalMatrix);
        
        writeMatrix("output.txt", finalResult);
        System.out.println("Benchmark complete. Results saved to output.txt");
    }

    // --- BENCHMARK RUNNER ---
    static int[][] runBenchmarks(int N, int M, int P, int[][] originalMatrix) {
        long totalSeqTime = 0, totalParTime = 0;
        int[][] finalResult = null;

        System.out.printf("Running 10 benchmark iterations [N=%d, M=%d, P=%d]...\n", N, M, P);

        for (int run = 0; run < 10; run++) {
            // 1. Sequential Execution
            int[][] seqMatrix = copyMatrix(originalMatrix, N, M);
            long startSeq = System.currentTimeMillis();
            sequentialConvolution(N, M, seqMatrix);
            totalSeqTime += (System.currentTimeMillis() - startSeq);
            
            if (run == 0) {
                finalResult = copyMatrix(seqMatrix, N, M);
            }

            // 2. Parallel Execution
            int[][] parMatrix = copyMatrix(originalMatrix, N, M);
            CyclicBarrier barrier = new CyclicBarrier(P);
            Thread[] threads = new Thread[P];
            
            long startPar = System.currentTimeMillis();
            for (int i = 0; i < P; i++) {
                threads[i] = new Thread(new ParallelWorker(i, P, N, M, parMatrix, barrier));
                threads[i].start();
            }
            for (int i = 0; i < P; i++) {
                try { threads[i].join(); } catch (InterruptedException e) {}
            }
            totalParTime += (System.currentTimeMillis() - startPar);

            // 3. Verification
            if (!verifyMatrices(seqMatrix, parMatrix, N, M)) {
                System.out.printf("  -> Run %d FAILED: Matrix mismatch detected!\n", run + 1);
            }
        }

        System.out.printf("  => Avg Sequential Time: %.2f ms\n", (totalSeqTime / 10.0));
        System.out.printf("  => Avg Parallel Time:   %.2f ms\n\n", (totalParTime / 10.0));
        return finalResult;
    }

    // --- SEQUENTIAL CONVOLUTION ---
    static void sequentialConvolution(int N, int M, int[][] matrix) {
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

            // In-place replacement
            matrix[i] = computedRow;
            
            // Shift references forward
            prevRow = currRow;
            currRow = Arrays.copyOf(nextRow, M);
        }
    }


    // --- PARALLEL WORKER (VARIANT 2B) ---
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
            int startRow = 1 + id * (N - 2) / P;
            int endRow = 1 + (id + 1) * (N - 2) / P;
            if (id == P - 1) endRow = N - 1;

            if (startRow >= endRow) {
                try { barrier.await(); } catch (Exception e) {}
                return;
            }

            // O(M) Temporary space allocation (matches C++ dynamic 1D arrays)
            int[] prevRow = new int[M];
            int[] currRow = new int[M];
            int[] newTop = new int[M];
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

                boolean isTopBoundary = (i == startRow && id > 0);
                boolean isBottomBoundary = (i == endRow - 1 && id < P - 1);

                if (isTopBoundary && isBottomBoundary) {
                    System.arraycopy(computedRow, 0, newTop, 0, M);
                } else if (isTopBoundary) {
                    System.arraycopy(computedRow, 0, newTop, 0, M);
                } else if (isBottomBoundary) {
                    System.arraycopy(computedRow, 0, newBottom, 0, M);
                } else {
                    matrix[i] = computedRow; // Internal cells written directly
                }

                prevRow = currRow;
                currRow = nextRow;
            }

            // PHASE 2: SYNCHRONIZE
            try { barrier.await(); } catch (Exception e) { e.printStackTrace(); }

            // PHASE 3: UPDATE BOUNDARIES
            boolean isTopBoundary = (id > 0);
            boolean isBottomBoundary = (id < P - 1);
            
            if (isTopBoundary && isBottomBoundary && (endRow - startRow == 1)) {
                matrix[startRow] = newTop;
            } else {
                if (isTopBoundary) matrix[startRow] = newTop;
                if (isBottomBoundary) matrix[endRow - 1] = newBottom;
            }
        }
    }

    // --- FILE I/O & UTILS ---
    static int[][] readMatrix(String filename, int[] dims) {
        File f = new File(filename);
        if (!f.exists()) {
            File p = new File("..", filename);
            if (p.exists()) f = p;
        }
        try (Scanner scanner = new Scanner(f)) {
            dims[0] = scanner.nextInt();
            dims[1] = scanner.nextInt();
            int[][] mat = new int[dims[0]][dims[1]];
            for (int i = 0; i < dims[0]; i++) {
                for (int j = 0; j < dims[1]; j++) {
                    mat[i][j] = scanner.nextInt();
                }
            }
            return mat;
        } catch (Exception e) {
            return null;
        }
    }

    static void writeMatrix(String filename, int[][] matrix) {
        try (PrintWriter writer = new PrintWriter(filename)) {
            writer.println(matrix.length + " " + matrix[0].length);
            for (int[] row : matrix) {
                for (int val : row) {
                    writer.print(val + " ");
                }
                writer.println();
            }
        } catch (Exception e) {
            System.err.println("Failed to write to " + filename);
        }
    }

    static boolean verifyMatrices(int[][] m1, int[][] m2, int N, int M) {
        for (int i = 0; i < N; i++) {
            for (int j = 0; j < M; j++) {
                if (m1[i][j] != m2[i][j]) return false;
            }
        }
        return true;
    }

    static int[][] copyMatrix(int[][] original, int N, int M) {
        int[][] copy = new int[N][M];
        for (int i = 0; i < N; i++) {
            System.arraycopy(original[i], 0, copy[i], 0, M);
        }
        return copy;
    }
}
