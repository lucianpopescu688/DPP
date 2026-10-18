public class Main {
    public static void main(String[] args) {
        if (args.length < 1) {
            System.err.println("Usage: java Main <vector_size>");
            System.exit(1);
        }

        int n = 0;
        try {
            n = Integer.parseInt(args[0]);
        } catch (NumberFormatException e) {
            System.err.println("Vector size must be an integer.");
            System.exit(1);
        }

        if (n <= 0) {
            System.err.println("Vector size must be positive.");
            System.exit(1);
        }

        double[] A = new double[n];
        double[] B = new double[n];
        double[] C = new double[n];

        // Initialize vectors A and B with random real numbers
        // We do this before the timer starts to not affect the measured execution time
        java.util.Random random = new java.util.Random();
        for (int i = 0; i < n; i++) {
            A[i] = random.nextDouble() * 100.0;
            B[i] = random.nextDouble() * 100.0;
        }

        // Start the timer just before the actual computation begins
        long startTime = System.nanoTime();

        for (int i = 0; i < n; i++) {
            C[i] = A[i] + B[i];
        }

        // Stop the timer immediately after the computation finishes
        long endTime = System.nanoTime();

        // Calculate the elapsed time in milliseconds
        double durationMs = (endTime - startTime) / 1_000_000.0;

        // Print the execution time to the standard output.
        System.out.println(durationMs);
    }
}
