import java.io.*;
import java.util.StringTokenizer;

public class Main {

    @FunctionalInterface
    interface TaskRunner {
        void run() throws InterruptedException;
    }

    // Resolves file path from either the current directory or the parent directory
    private static File resolveFile(String filename) {
        File f = new File(filename);
        if (f.exists()) return f;
        File parent = new File("..", filename);
        return parent.exists() ? parent : f;
    }

    // Reads a 2D matrix from a file (supports both "n m" and square "k" headers)
    public static int[][] readMatrix(String filename) throws IOException {
        try (BufferedReader br = new BufferedReader(new FileReader(resolveFile(filename)))) {
            String line = br.readLine();
            if (line == null) return new int[0][0];

            StringTokenizer st = new StringTokenizer(line);
            int rows = Integer.parseInt(st.nextToken());
            int cols = st.hasMoreTokens() ? Integer.parseInt(st.nextToken()) : rows;

            int[][] matrix = new int[rows][cols];
            for (int i = 0; i < rows; i++) {
                line = br.readLine();
                if (line == null) break;
                st = new StringTokenizer(line);
                for (int j = 0; j < cols; j++) {
                    matrix[i][j] = Integer.parseInt(st.nextToken());
                }
            }
            return matrix;
        }
    }

    // Writes the 2D matrix directly to an output file
    public static void writeMatrix(String filename, int[][] matrix) throws IOException {
        try (BufferedWriter bw = new BufferedWriter(new FileWriter(filename))) {
            bw.write(matrix.length + " " + matrix[0].length + "\n");
            for (int[] row : matrix) {
                for (int j = 0; j < row.length; j++) {
                    bw.write(row[j] + (j + 1 < row.length ? " " : ""));
                }
                bw.newLine();
            }
        }
    }

    // Verifies that two matrices have identical dimensions and values
    public static boolean areEqual(int[][] a, int[][] b) {
        if (a.length != b.length || a[0].length != b[0].length) return false;
        for (int i = 0; i < a.length; i++) {
            for (int j = 0; j < a[0].length; j++) {
                if (a[i][j] != b[i][j]) return false;
            }
        }
        return true;
    }

    // Computes convolution for a single cell (i, j) using clamped virtual borders
    public static int computeConvolution(int[][] F, int[][] C, int i, int j, int n, int m, int radius) {
        int sum = 0;
        for (int a = -radius; a <= radius; a++) {
            int r = Math.clamp(i + a, 0, n - 1);
            for (int b = -radius; b <= radius; b++) {
                int c = Math.clamp(j + b, 0, m - 1);
                sum += F[r][c] * C[a + radius][b + radius];
            }
        }
        return sum;
    }

    // Computes convolution over the rectangular submatrix [r0, r1) x [c0, c1)
    private static void computeRegion(int[][] F, int[][] C, int[][] V, int r0, int r1, int c0, int c1, int n, int m, int radius) {
        for (int i = r0; i < r1; i++) {
            for (int j = c0; j < c1; j++) {
                V[i][j] = computeConvolution(F, C, i, j, n, m, radius);
            }
        }
    }

    // Returns balanced [start, end) bounds for partition 'id' out of 'parts'
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

    // 1. Sequential convolution
    public static void sequential(int[][] F, int[][] C, int[][] V, int n, int m, int k) {
        computeRegion(F, C, V, 0, n, 0, m, n, m, k / 2);
    }

    // 2. Horizontal decomposition (rows distributed across p threads)
    public static void horizontal(int[][] F, int[][] C, int[][] V, int n, int m, int k, int p) throws InterruptedException {
        Thread[] threads = new Thread[p];
        for (int id = 0; id < p; id++) {
            int[] rows = getChunk(id, n, p);
            threads[id] = new Thread(() -> computeRegion(F, C, V, rows[0], rows[1], 0, m, n, m, k / 2));
        }
        startAndJoin(threads);
    }

    // 3. Vertical decomposition (columns distributed across p threads)
    public static void vertical(int[][] F, int[][] C, int[][] V, int n, int m, int k, int p) throws InterruptedException {
        Thread[] threads = new Thread[p];
        for (int id = 0; id < p; id++) {
            int[] cols = getChunk(id, m, p);
            threads[id] = new Thread(() -> computeRegion(F, C, V, 0, n, cols[0], cols[1], n, m, k / 2));
        }
        startAndJoin(threads);
    }

    // 4. Block decomposition (2D submatrix grid pr x pc distributed across p threads)
    public static void block(int[][] F, int[][] C, int[][] V, int n, int m, int k, int p) throws InterruptedException {
        int pr = (int) Math.sqrt(p);
        while (p % pr != 0) pr--;
        int pc = p / pr;

        Thread[] threads = new Thread[p];
        int idx = 0;
        for (int r = 0; r < pr; r++) {
            int[] rows = getChunk(r, n, pr);
            for (int c = 0; c < pc; c++) {
                int[] cols = getChunk(c, m, pc);
                threads[idx++] = new Thread(() -> computeRegion(F, C, V, rows[0], rows[1], cols[0], cols[1], n, m, k / 2));
            }
        }
        startAndJoin(threads);
    }

    // 5. Cyclic distribution: delta(i, j) = (i * m + j) % p
    public static void cyclic(int[][] F, int[][] C, int[][] V, int n, int m, int k, int p) throws InterruptedException {
        int radius = k / 2, total = n * m;
        Thread[] threads = new Thread[p];
        for (int id = 0; id < p; id++) {
            int startId = id;
            threads[id] = new Thread(() -> {
                for (int idx = startId; idx < total; idx += p) {
                    V[idx / m][idx % m] = computeConvolution(F, C, idx / m, idx % m, n, m, radius);
                }
            });
        }
        startAndJoin(threads);
    }

    // Measures the execution time of a task in milliseconds
    private static double measureMs(TaskRunner task) throws InterruptedException {
        long t0 = System.nanoTime();
        task.run();
        return (System.nanoTime() - t0) / 1e6;
    }

    public static void main(String[] args) throws Exception {
        String mode = args.length > 0 ? args[0].toLowerCase() : "all";
        int p = args.length > 1 ? Integer.parseInt(args[1]) : 4;
        String fFile = args.length > 2 ? args[2] : "MatrixF.txt";
        String cFile = args.length > 3 ? args[3] : "ConvolutionM.txt";

        int[][] F = readMatrix(fFile);
        int[][] C = readMatrix(cFile);
        int n = F.length, m = F[0].length, k = C.length;

        int[][] V_seq = new int[n][m];
        int[][] V_par = new int[n][m];

        if (mode.equals("all") || mode.equals("verify")) {
            System.out.printf("Matrix F: %d x %d | Kernel C: %d x %d | Threads p = %d%n", n, m, k, k, p);
            System.out.println("--------------------------------------------------");

            double msSeq = measureMs(() -> sequential(F, C, V_seq, n, m, k));
            System.out.printf("[Sequential] Execution Time: %.4f ms%n", msSeq);
            writeMatrix("output.txt", V_seq);
            writeMatrix("output_seq.txt", V_seq);

            String[] labels = {"[Horizontal]", "[Vertical]  ", "[Block]     ", "[Cyclic]    "};
            TaskRunner[] tasks = {
                () -> horizontal(F, C, V_par, n, m, k, p),
                () -> vertical(F, C, V_par, n, m, k, p),
                () -> block(F, C, V_par, n, m, k, p),
                () -> cyclic(F, C, V_par, n, m, k, p)
            };

            for (int i = 0; i < labels.length; i++) {
                double ms = measureMs(tasks[i]);
                boolean ok = areEqual(V_seq, V_par);
                System.out.printf("%s Execution Time: %.4f ms | Match: %b | Speedup: %.2fx%n",
                        labels[i], ms, ok, msSeq / ms);
            }
            System.out.println("--------------------------------------------------");
            System.out.println("Output matrix saved to output.txt");
            return;
        }

        double durationMs = measureMs(() -> {
            switch (mode) {
                case "sequential" -> sequential(F, C, V_par, n, m, k);
                case "horizontal" -> horizontal(F, C, V_par, n, m, k, p);
                case "vertical" -> vertical(F, C, V_par, n, m, k, p);
                case "block" -> block(F, C, V_par, n, m, k, p);
                case "cyclic" -> cyclic(F, C, V_par, n, m, k, p);
                default -> throw new IllegalArgumentException("Unknown mode: " + mode);
            }
        });

        writeMatrix("output.txt", V_par);
        if (mode.equals("sequential")) {
            writeMatrix("output_seq.txt", V_par);
        }

        // Verify correctness against the sequential baseline file
        File seqFile = resolveFile("output_seq.txt");
        if (seqFile.exists()) {
            int[][] expected = readMatrix(seqFile.getPath());
            if (!areEqual(expected, V_par)) {
                System.err.println("CORRECTNESS ERROR: Result does not match sequential baseline!");
                System.exit(1);
            }
        }

        System.out.printf("%.4f%n", durationMs);
    }
}