import java.io.*;
import java.util.StringTokenizer;

public class Main {

    private static File resolveFile(String filename) {
        File f = new File(filename);
        if (f.exists()) return f;
        File parent = new File("..", filename);
        if (parent.exists()) return parent;
        return f;
    }

    public static int[][] readMatrix(String filename) throws IOException {
        File file = resolveFile(filename);
        try (BufferedReader br = new BufferedReader(new FileReader(file))) {
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

    public static void writeMatrix(String filename, int[][] matrix) throws IOException {
        int rows = matrix.length;
        int cols = matrix[0].length;
        try (BufferedWriter bw = new BufferedWriter(new FileWriter(filename))) {
            bw.write(rows + " " + cols);
            bw.newLine();
            for (int i = 0; i < rows; i++) {
                StringBuilder sb = new StringBuilder();
                for (int j = 0; j < cols; j++) {
                    sb.append(matrix[i][j]).append(" ");
                }
                bw.write(sb.toString().trim());
                bw.newLine();
            }
        }
    }

    public static boolean areEqual(int[][] a, int[][] b) {
        if (a.length != b.length || a[0].length != b[0].length) return false;
        for (int i = 0; i < a.length; i++) {
            for (int j = 0; j < a[0].length; j++) {
                if (a[i][j] != b[i][j]) return false;
            }
        }
        return true;
    }

    public static int computeConvolution(int[][] F, int[][] C, int i, int j, int n, int m, int k, int radius) {
        int sum = 0;
        for (int a = -radius; a <= radius; a++) {
            for (int b = -radius; b <= radius; b++) {
                int r = Math.clamp(i + a, 0, n - 1);
                int c = Math.clamp(j + b, 0, m - 1);
                sum += F[r][c] * C[a + radius][b + radius];
            }
        }
        return sum;
    }

    private static void startAndJoin(Thread[] threads) throws InterruptedException {
        for (Thread t : threads) t.start();
        for (Thread t : threads) t.join();
    }

    public static void sequential(int[][] F, int[][] C, int[][] V, int n, int m, int k) {
        int radius = k / 2;
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < m; j++) {
                V[i][j] = computeConvolution(F, C, i, j, n, m, k, radius);
            }
        }
    }

    public static void horizontal(int[][] F, int[][] C, int[][] V, int n, int m, int k, int p) throws InterruptedException {
        int radius = k / 2;
        Thread[] threads = new Thread[p];
        int base = n / p, rem = n % p, startRow = 0;

        for (int id = 0; id < p; id++) {
            int r0 = startRow;
            int r1 = startRow + base + (id < rem ? 1 : 0);
            threads[id] = new Thread(() -> {
                for (int i = r0; i < r1; i++) {
                    for (int j = 0; j < m; j++) {
                        V[i][j] = computeConvolution(F, C, i, j, n, m, k, radius);
                    }
                }
            });
            startRow = r1;
        }
        startAndJoin(threads);
    }

    public static void vertical(int[][] F, int[][] C, int[][] V, int n, int m, int k, int p) throws InterruptedException {
        int radius = k / 2;
        Thread[] threads = new Thread[p];
        int base = m / p, rem = m % p, startCol = 0;

        for (int id = 0; id < p; id++) {
            int c0 = startCol;
            int c1 = startCol + base + (id < rem ? 1 : 0);
            threads[id] = new Thread(() -> {
                for (int i = 0; i < n; i++) {
                    for (int j = c0; j < c1; j++) {
                        V[i][j] = computeConvolution(F, C, i, j, n, m, k, radius);
                    }
                }
            });
            startCol = c1;
        }
        startAndJoin(threads);
    }

    public static void block(int[][] F, int[][] C, int[][] V, int n, int m, int k, int p) throws InterruptedException {
        int radius = k / 2;
        int pr = 1, pc = p;
        for (int r = (int) Math.sqrt(p); r >= 1; r--) {
            if (p % r == 0) {
                pr = r;
                pc = p / r;
                break;
            }
        }

        Thread[] threads = new Thread[p];
        int threadIdx = 0, startRow = 0;
        int rowBase = n / pr, rowRem = n % pr;
        int colBase = m / pc, colRem = m % pc;

        for (int r = 0; r < pr; r++) {
            int r0 = startRow;
            int r1 = startRow + rowBase + (r < rowRem ? 1 : 0);
            int startCol = 0;
            for (int c = 0; c < pc; c++) {
                int c0 = startCol;
                int c1 = startCol + colBase + (c < colRem ? 1 : 0);
                threads[threadIdx++] = new Thread(() -> {
                    for (int i = r0; i < r1; i++) {
                        for (int j = c0; j < c1; j++) {
                            V[i][j] = computeConvolution(F, C, i, j, n, m, k, radius);
                        }
                    }
                });
                startCol = c1;
            }
            startRow = r1;
        }
        startAndJoin(threads);
    }

    public static void cyclic(int[][] F, int[][] C, int[][] V, int n, int m, int k, int p) throws InterruptedException {
        int radius = k / 2;
        Thread[] threads = new Thread[p];
        int total = n * m;

        for (int id = 0; id < p; id++) {
            int startId = id;
            threads[id] = new Thread(() -> {
                for (int idx = startId; idx < total; idx += p) {
                    V[idx / m][idx % m] = computeConvolution(F, C, idx / m, idx % m, n, m, k, radius);
                }
            });
        }
        startAndJoin(threads);
    }

    public static void main(String[] args) throws Exception {
        String mode = args.length > 0 ? args[0].toLowerCase() : "all";
        int p = args.length > 1 ? Integer.parseInt(args[1]) : 4;
        String fFile = args.length > 2 ? args[2] : "MatrixF.txt";
        String cFile = args.length > 3 ? args[3] : "ConvolutionM.txt";

        int[][] F = readMatrix(fFile);
        int[][] C = readMatrix(cFile);
        int n = F.length;
        int m = F[0].length;
        int k = C.length;

        int[][] V_seq = new int[n][m];
        int[][] V_par = new int[n][m];

        if (mode.equals("all") || mode.equals("verify")) {
            System.out.printf("Matrix F: %d x %d | Kernel C: %d x %d | Threads p = %d%n", n, m, k, k, p);
            System.out.println("--------------------------------------------------");

            long t0 = System.nanoTime();
            sequential(F, C, V_seq, n, m, k);
            long tSeq = System.nanoTime() - t0;
            double msSeq = tSeq / 1e6;
            System.out.printf("[Sequential] Execution Time: %.4f ms%n", msSeq);
            writeMatrix("output.txt", V_seq);

            t0 = System.nanoTime();
            horizontal(F, C, V_par, n, m, k, p);
            long tHoriz = System.nanoTime() - t0;
            double msHoriz = tHoriz / 1e6;
            boolean okHoriz = areEqual(V_seq, V_par);
            System.out.printf("[Horizontal] Execution Time: %.4f ms | Match: %b | Speedup: %.2fx%n", msHoriz, okHoriz, msSeq / msHoriz);

            t0 = System.nanoTime();
            vertical(F, C, V_par, n, m, k, p);
            long tVert = System.nanoTime() - t0;
            double msVert = tVert / 1e6;
            boolean okVert = areEqual(V_seq, V_par);
            System.out.printf("[Vertical]   Execution Time: %.4f ms | Match: %b | Speedup: %.2fx%n", msVert, okVert, msSeq / msVert);

            t0 = System.nanoTime();
            block(F, C, V_par, n, m, k, p);
            long tBlock = System.nanoTime() - t0;
            double msBlock = tBlock / 1e6;
            boolean okBlock = areEqual(V_seq, V_par);
            System.out.printf("[Block]      Execution Time: %.4f ms | Match: %b | Speedup: %.2fx%n", msBlock, okBlock, msSeq / msBlock);

            t0 = System.nanoTime();
            cyclic(F, C, V_par, n, m, k, p);
            long tCyc = System.nanoTime() - t0;
            double msCyc = tCyc / 1e6;
            boolean okCyc = areEqual(V_seq, V_par);
            System.out.printf("[Cyclic]     Execution Time: %.4f ms | Match: %b | Speedup: %.2fx%n", msCyc, okCyc, msSeq / msCyc);

            System.out.println("--------------------------------------------------");
            System.out.println("Output matrix saved to output.txt");
            return;
        }

        long start = System.nanoTime();
        switch (mode) {
            case "seq" -> sequential(F, C, V_par, n, m, k);
            case "horizontal", "horiz" -> horizontal(F, C, V_par, n, m, k, p);
            case "vertical", "vert" -> vertical(F, C, V_par, n, m, k, p);
            case "block" -> block(F, C, V_par, n, m, k, p);
            case "cyclic", "cyc" -> cyclic(F, C, V_par, n, m, k, p);
            default -> throw new IllegalArgumentException("Unknown mode: " + mode);
        }
        long durationNs = System.nanoTime() - start;
        double durationMs = durationNs / 1e6;

        writeMatrix("output.txt", V_par);

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