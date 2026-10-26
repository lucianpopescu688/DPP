import java.io.*;
import java.util.StringTokenizer;
import java.util.concurrent.CyclicBarrier;
import java.util.concurrent.atomic.AtomicBoolean;

public class Main {

    @FunctionalInterface
    interface TaskRunner {
        void run() throws Exception;
    }

    // Holds both the image matrix F and the 3x3 convolution kernel C read from "date.txt"
    static class InputData {
        int[][] F;
        int[][] C;
        int n, m, k;
    }

    // Resolves file path from either the current directory or the parent directory
    private static File resolveFile(String filename) {
        File f = new File(filename);
        if (f.exists()) return f;
        File parent = new File("..", filename);
        return parent.exists() ? parent : f;
    }

    // Helper to read the next token across lines from a BufferedReader
    private static String nextToken(BufferedReader br, StringTokenizer[] stHolder) throws IOException {
        while (stHolder[0] == null || !stHolder[0].hasMoreTokens()) {
            String line = br.readLine();
            if (line == null) return null;
            stHolder[0] = new StringTokenizer(line);
        }
        return stHolder[0].nextToken();
    }

    // Reads matrix F (n x m) and kernel C (k x k) from "date.txt" (or fallback ConvolutionM.txt)
    public static InputData readInputData(String dataFile, String kernelFallback) throws IOException {
        InputData data = new InputData();
        try (BufferedReader br = new BufferedReader(new FileReader(resolveFile(dataFile)))) {
            StringTokenizer[] st = new StringTokenizer[1];
            String tokN = nextToken(br, st);
            String tokM = nextToken(br, st);
            if (tokN == null || tokM == null) throw new IOException("Invalid header in " + dataFile);

            data.n = Integer.parseInt(tokN);
            data.m = Integer.parseInt(tokM);
            data.F = new int[data.n][data.m];

            for (int i = 0; i < data.n; i++) {
                for (int j = 0; j < data.m; j++) {
                    data.F[i][j] = Integer.parseInt(nextToken(br, st));
                }
            }

            // Check if kernel C is also stored in date.txt
            String tokK = nextToken(br, st);
            if (tokK != null) {
                data.k = Integer.parseInt(tokK);
                // If the header line had "k k" instead of just "k", consume the second k on the same line
                if (st[0] != null && st[0].hasMoreTokens()) {
                    st[0].nextToken();
                }
                data.C = new int[data.k][data.k];
                for (int i = 0; i < data.k; i++) {
                    for (int j = 0; j < data.k; j++) {
                        data.C[i][j] = Integer.parseInt(nextToken(br, st));
                    }
                }
                return data;
            }
        }

        // Fallback: read kernel from ConvolutionM.txt if not embedded in date.txt
        File kFile = resolveFile(kernelFallback);
        if (kFile.exists()) {
            try (BufferedReader br = new BufferedReader(new FileReader(kFile))) {
                StringTokenizer[] st = new StringTokenizer[1];
                data.k = Integer.parseInt(nextToken(br, st));
                if (st[0] != null && st[0].hasMoreTokens()) {
                    st[0].nextToken();
                }
                data.C = new int[data.k][data.k];
                for (int i = 0; i < data.k; i++) {
                    for (int j = 0; j < data.k; j++) {
                        data.C[i][j] = Integer.parseInt(nextToken(br, st));
                    }
                }
                return data;
            }
        }

        data.k = 3;
        data.C = new int[][]{{0, -1, 0}, {-1, 5, -1}, {0, -1, 0}};
        return data;
    }

    // Writes the 2D matrix directly to an output file
    public static void writeMatrix(String filename, int[][] matrix) throws IOException {
        try (BufferedWriter bw = new BufferedWriter(new FileWriter(filename))) {
            bw.write(matrix.length + " " + matrix[0].length + "\n");
            for (int[] row : matrix) {
                StringBuilder sb = new StringBuilder(row.length * 5);
                for (int j = 0; j < row.length; j++) {
                    sb.append(row[j]);
                    if (j + 1 < row.length) sb.append(' ');
                }
                sb.append('\n');
                bw.write(sb.toString());
            }
        }
    }

    // Verifies that two output files have identical content without allocating an extra matrix
    public static boolean verifyFilesEqual(String fileA, String fileB) throws IOException {
        try (BufferedReader brA = new BufferedReader(new FileReader(resolveFile(fileA)));
             BufferedReader brB = new BufferedReader(new FileReader(resolveFile(fileB)))) {
            String lineA, lineB;
            while (true) {
                lineA = brA.readLine();
                lineB = brB.readLine();
                if (lineA == null && lineB == null) return true;
                if (lineA == null || lineB == null) return false;
                if (!lineA.trim().equals(lineB.trim())) return false;
            }
        }
    }

    // Computes the 3x3 convolution for a single row using 3 source row buffers
    private static void computeRow3x3(int[] rowAbove, int[] rowCurr, int[] rowBelow,
                                      int[] destRow, int[][] C, int m) {
        int c00 = C[0][0], c01 = C[0][1], c02 = C[0][2];
        int c10 = C[1][0], c11 = C[1][1], c12 = C[1][2];
        int c20 = C[2][0], c21 = C[2][1], c22 = C[2][2];

        for (int j = 0; j < m; j++) {
            int left  = (j > 0) ? (j - 1) : 0;
            int right = (j + 1 < m) ? (j + 1) : (m - 1);
            destRow[j] = rowAbove[left] * c00 + rowAbove[j] * c01 + rowAbove[right] * c02
                       + rowCurr[left]  * c10 + rowCurr[j]  * c11 + rowCurr[right]  * c12
                       + rowBelow[left] * c20 + rowBelow[j] * c21 + rowBelow[right] * c22;
        }
    }

    // Returns balanced [start, end) row bounds for partition 'id' out of 'parts'
    private static int[] getChunk(int id, int total, int parts) {
        int base = total / parts, rem = total % parts;
        int start = id * base + Math.min(id, rem);
        int end = start + base + (id < rem ? 1 : 0);
        return new int[]{start, end};
    }

    // Starts all threads explicitly and waits for all of them to finish
    private static void startAndJoin(Thread[] threads) throws InterruptedException {
        for (Thread t : threads) t.start();
        for (Thread t : threads) t.join();
    }

    // 1. Sequential In-Place Convolution — O(M) auxiliary space (2 row vectors)
    public static void sequentialInPlace(int[][] F, int[][] C, int n, int m) {
        int[] prevRow = new int[m];
        int[] currRow = new int[m];

        System.arraycopy(F[0], 0, prevRow, 0, m);

        for (int i = 0; i < n; i++) {
            System.arraycopy(F[i], 0, currRow, 0, m);
            int[] below = (i + 1 < n) ? F[i + 1] : currRow;
            computeRow3x3(prevRow, currRow, below, F[i], C, m);

            int[] tmp = prevRow;
            prevRow = currRow;
            currRow = tmp;
        }
    }

    // 2. Variant 1b: Copy Neighbor Boundaries + CyclicBarrier (3M ints/thread)
    public static void parallelVariant1b(int[][] F, int[][] C, int n, int m, int p) throws InterruptedException {
        int activeP = Math.min(p, n);
        CyclicBarrier barrier = new CyclicBarrier(activeP);
        Thread[] threads = new Thread[activeP];

        for (int id = 0; id < activeP; id++) {
            int[] chunk = getChunk(id, n, activeP);
            int r0 = chunk[0], r1 = chunk[1];
            threads[id] = new Thread(() -> {
                try {
                    int[] prevRow        = new int[m];
                    int[] currRow        = new int[m];
                    int[] bottomBoundary = new int[m];

                    // Step 1: Copy required neighbor boundaries into auxiliary vectors
                    int topIdx = (r0 > 0) ? (r0 - 1) : 0;
                    int botIdx = (r1 < n) ? r1 : (n - 1);
                    System.arraycopy(F[topIdx], 0, prevRow, 0, m);
                    System.arraycopy(F[botIdx], 0, bottomBoundary, 0, m);

                    // Step 2: Synchronization barrier
                    barrier.await();

                    // Step 3: Update assigned rows in-place using auxiliary vectors
                    for (int i = r0; i < r1; i++) {
                        System.arraycopy(F[i], 0, currRow, 0, m);
                        int[] below = (i + 1 < r1) ? F[i + 1] : bottomBoundary;
                        computeRow3x3(prevRow, currRow, below, F[i], C, m);

                        int[] tmp = prevRow;
                        prevRow = currRow;
                        currRow = tmp;
                    }
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            });
        }
        startAndJoin(threads);
    }

    // 3. Variant 1f: Copy Neighbor Boundaries + Fine-Grained Atomic Flags (3M ints/thread)
    public static void parallelVariant1f(int[][] F, int[][] C, int n, int m, int p) throws InterruptedException {
        int activeP = Math.min(p, n);
        AtomicBoolean[] topReady = new AtomicBoolean[activeP];
        AtomicBoolean[] botReady = new AtomicBoolean[activeP];
        for (int i = 0; i < activeP; i++) {
            topReady[i] = new AtomicBoolean(false);
            botReady[i] = new AtomicBoolean(false);
        }

        Thread[] threads = new Thread[activeP];
        for (int id = 0; id < activeP; id++) {
            int threadId = id;
            int[] chunk = getChunk(id, n, activeP);
            int r0 = chunk[0], r1 = chunk[1];
            threads[id] = new Thread(() -> {
                int[] prevRow        = new int[m];
                int[] currRow        = new int[m];
                int[] bottomBoundary = new int[m];

                // Step 1 & 2: Copy neighbor boundaries and signal atomic flags
                int topIdx = (r0 > 0) ? (r0 - 1) : 0;
                System.arraycopy(F[topIdx], 0, prevRow, 0, m);
                topReady[threadId].set(true);

                int botIdx = (r1 < n) ? r1 : (n - 1);
                System.arraycopy(F[botIdx], 0, bottomBoundary, 0, m);
                botReady[threadId].set(true);

                // Step 3: Wait until immediate neighbor flags are true
                if (threadId > 0) {
                    while (!botReady[threadId - 1].get()) {
                        Thread.onSpinWait();
                    }
                }
                if (threadId + 1 < activeP) {
                    while (!topReady[threadId + 1].get()) {
                        Thread.onSpinWait();
                    }
                }

                // Step 4: Update assigned rows in-place
                for (int i = r0; i < r1; i++) {
                    System.arraycopy(F[i], 0, currRow, 0, m);
                    int[] below = (i + 1 < r1) ? F[i + 1] : bottomBoundary;
                    computeRow3x3(prevRow, currRow, below, F[i], C, m);

                    int[] tmp = prevRow;
                    prevRow = currRow;
                    currRow = tmp;
                }
            });
        }
        startAndJoin(threads);
    }

    // 4. Variant 2b: Compute Boundary New Values to Vectors, Interior In-Place + CyclicBarrier (3M ints/thread)
    public static void parallelVariant2b(int[][] F, int[][] C, int n, int m, int p) throws InterruptedException {
        int activeP = Math.min(p, n);
        CyclicBarrier barrier = new CyclicBarrier(activeP);
        Thread[] threads = new Thread[activeP];

        for (int id = 0; id < activeP; id++) {
            int[] chunk = getChunk(id, n, activeP);
            int r0 = chunk[0], r1 = chunk[1];
            threads[id] = new Thread(() -> {
                try {
                    int[] newFirstRow = new int[m];
                    int[] prevRow     = new int[m];
                    int[] currRow     = new int[m]; // Reused as newLastRow after interior loop

                    // Step 1a: Compute new values for top boundary row r0 into newFirstRow
                    int[] topAbove = (r0 > 0) ? F[r0 - 1] : F[0];
                    int[] topBelow = (r0 + 1 < n) ? F[r0 + 1] : F[r0];
                    computeRow3x3(topAbove, F[r0], topBelow, newFirstRow, C, m);

                    if (r1 - 1 > r0) {
                        // Step 1b: Compute interior rows [r0 + 1, r1 - 2] directly into matrix F
                        System.arraycopy(F[r0], 0, prevRow, 0, m);
                        for (int i = r0 + 1; i <= r1 - 2; i++) {
                            System.arraycopy(F[i], 0, currRow, 0, m);
                            computeRow3x3(prevRow, currRow, F[i + 1], F[i], C, m);

                            int[] tmp = prevRow;
                            prevRow = currRow;
                            currRow = tmp;
                        }

                        // Step 1c: Compute bottom boundary row (r1 - 1) into currRow (newLastRow)
                        int last = r1 - 1;
                        int[] botBelow = (r1 < n) ? F[r1] : F[last];
                        computeRow3x3(prevRow, F[last], botBelow, currRow, C, m);
                    }

                    // Step 2: Synchronization barrier
                    barrier.await();

                    // Step 3: Copy computed boundary vectors into matrix F
                    System.arraycopy(newFirstRow, 0, F[r0], 0, m);
                    if (r1 - 1 > r0) {
                        System.arraycopy(currRow, 0, F[r1 - 1], 0, m);
                    }
                } catch (Exception e) {
                    throw new RuntimeException(e);
                }
            });
        }
        startAndJoin(threads);
    }

    // 5. Variant 2f: Compute Boundary New Values First + Atomic Flags + Interior In-Place (4M ints/thread)
    public static void parallelVariant2f(int[][] F, int[][] C, int n, int m, int p) throws InterruptedException {
        int activeP = Math.min(p, n);
        AtomicBoolean[] topDone = new AtomicBoolean[activeP];
        AtomicBoolean[] botDone = new AtomicBoolean[activeP];
        for (int i = 0; i < activeP; i++) {
            topDone[i] = new AtomicBoolean(false);
            botDone[i] = new AtomicBoolean(false);
        }

        Thread[] threads = new Thread[activeP];
        for (int id = 0; id < activeP; id++) {
            int threadId = id;
            int[] chunk = getChunk(id, n, activeP);
            int r0 = chunk[0], r1 = chunk[1];
            threads[id] = new Thread(() -> {
                int[] newFirstRow = new int[m];
                int[] newLastRow  = (r1 - 1 > r0) ? new int[m] : null;
                int[] prevRow     = new int[m];
                int[] currRow     = new int[m];

                // Step 1 & 2: Compute boundary rows into separate vectors and set flags
                int[] topAbove = (r0 > 0) ? F[r0 - 1] : F[0];
                int[] topBelow = (r0 + 1 < n) ? F[r0 + 1] : F[r0];
                computeRow3x3(topAbove, F[r0], topBelow, newFirstRow, C, m);
                topDone[threadId].set(true);

                if (r1 - 1 > r0) {
                    int last = r1 - 1;
                    int[] botAbove = F[last - 1];
                    int[] botBelow = (r1 < n) ? F[r1] : F[last];
                    computeRow3x3(botAbove, F[last], botBelow, newLastRow, C, m);
                }
                botDone[threadId].set(true);

                // Step 3: Compute interior rows [r0 + 1, r1 - 2] directly into matrix F
                if (r1 - r0 > 2) {
                    System.arraycopy(F[r0], 0, prevRow, 0, m);
                    for (int i = r0 + 1; i <= r1 - 2; i++) {
                        System.arraycopy(F[i], 0, currRow, 0, m);
                        computeRow3x3(prevRow, currRow, F[i + 1], F[i], C, m);

                        int[] tmp = prevRow;
                        prevRow = currRow;
                        currRow = tmp;
                    }
                }

                // Step 4 & 5: Check neighbor flags and copy boundary vectors into matrix F
                if (threadId > 0) {
                    while (!botDone[threadId - 1].get()) {
                        Thread.onSpinWait();
                    }
                }
                System.arraycopy(newFirstRow, 0, F[r0], 0, m);

                if (r1 - 1 > r0) {
                    if (threadId + 1 < activeP) {
                        while (!topDone[threadId + 1].get()) {
                            Thread.onSpinWait();
                        }
                    }
                    System.arraycopy(newLastRow, 0, F[r1 - 1], 0, m);
                }
            });
        }
        startAndJoin(threads);
    }

    // Measures the execution time of a task in milliseconds
    private static double measureMs(TaskRunner task) throws Exception {
        long t0 = System.nanoTime();
        task.run();
        return (System.nanoTime() - t0) / 1e6;
    }

    public static void main(String[] args) throws Exception {
        String mode = args.length > 0 ? args[0].toLowerCase() : "all";
        int p = args.length > 1 ? Integer.parseInt(args[1]) : 4;
        String dataFile = args.length > 2 ? args[2] : "date.txt";
        String kernelFile = args.length > 3 ? args[3] : "ConvolutionM.txt";

        InputData data = readInputData(dataFile, kernelFile);

        if (mode.equals("all") || mode.equals("verify")) {
            System.out.printf("Matrix F: %d x %d | Kernel C: %d x %d | Threads p = %d (In-Place O(m) Aux Space)%n",
                    data.n, data.m, data.k, data.k, p);
            System.out.println("------------------------------------------------------------------");

            double msSeq = measureMs(() -> sequentialInPlace(data.F, data.C, data.n, data.m));
            System.out.printf("[Sequential]           Execution Time: %.4f ms%n", msSeq);
            writeMatrix("output.txt", data.F);
            writeMatrix("output_seq.txt", data.F);

            String[] labels = {
                "[Variant 1b (Barrier)]",
                "[Variant 1f (Flags)  ]",
                "[Variant 2b (Barrier)]",
                "[Variant 2f (Flags)  ]"
            };
            String[] modes = {"1b", "1f", "2b", "2f"};

            for (int i = 0; i < labels.length; i++) {
                InputData fresh = readInputData(dataFile, kernelFile);
                String mMode = modes[i];
                double ms = measureMs(() -> {
                    switch (mMode) {
                        case "1b" -> parallelVariant1b(fresh.F, fresh.C, fresh.n, fresh.m, p);
                        case "1f" -> parallelVariant1f(fresh.F, fresh.C, fresh.n, fresh.m, p);
                        case "2b" -> parallelVariant2b(fresh.F, fresh.C, fresh.n, fresh.m, p);
                        case "2f" -> parallelVariant2f(fresh.F, fresh.C, fresh.n, fresh.m, p);
                    }
                });
                writeMatrix("output.txt", fresh.F);
                boolean ok = verifyFilesEqual("output_seq.txt", "output.txt");
                System.out.printf("%s Execution Time: %.4f ms | Match: %b | Speedup: %.2fx%n",
                        labels[i], ms, ok, msSeq / ms);
            }
            System.out.println("------------------------------------------------------------------");
            System.out.println("Output matrix saved to output.txt");
            return;
        }

        double durationMs = measureMs(() -> {
            switch (mode) {
                case "sequential" -> sequentialInPlace(data.F, data.C, data.n, data.m);
                case "1b", "variant1b", "horizontal" -> parallelVariant1b(data.F, data.C, data.n, data.m, p);
                case "1f", "variant1f" -> parallelVariant1f(data.F, data.C, data.n, data.m, p);
                case "2b", "variant2b" -> parallelVariant2b(data.F, data.C, data.n, data.m, p);
                case "2f", "variant2f" -> parallelVariant2f(data.F, data.C, data.n, data.m, p);
                default -> throw new IllegalArgumentException("Unknown mode: " + mode);
            }
        });

        writeMatrix("output.txt", data.F);
        if (mode.equals("sequential")) {
            writeMatrix("output_seq.txt", data.F);
        }

        // Verify correctness against the sequential baseline file
        File seqFile = resolveFile("output_seq.txt");
        if (seqFile.exists()) {
            if (!verifyFilesEqual(seqFile.getPath(), "output.txt")) {
                System.err.println("CORRECTNESS ERROR: Result does not match sequential baseline!");
                System.exit(1);
            }
        }

        System.out.printf("%.4f%n", durationMs);
    }
}
