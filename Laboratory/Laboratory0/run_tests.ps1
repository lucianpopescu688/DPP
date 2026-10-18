# Prompt the user for the benchmark parameters (vector size and number of execution runs)
$n_input = Read-Host "Enter the vector size (press Enter to use default: 10000000)"
if ([string]::IsNullOrWhiteSpace($n_input)) {
    $n = 10000000
} else {
    $n = [int]$n_input
}

$runs_input = Read-Host "Enter the number of runs (press Enter to use default: 10)"
if ([string]::IsNullOrWhiteSpace($runs_input)) {
    $runs = 10
} else {
    $runs = [int]$runs_input
}

# Define relative paths to the compiled executables and source directories
$c_debug_exe = "VectorsAddC++\cmake-build-debug\VectorsAddC__.exe"
$c_release_exe = "VectorsAddC++\cmake-build-release\VectorsAddC__.exe"
$java_src = "VectorsAddJava\VectorsAddition\src\Main.java"
$java_dir = "VectorsAddJava\VectorsAddition\src"

Write-Host "========================================="
Write-Host "Laboratory 0 - Vector Addition Benchmark"
Write-Host "Vector size (n) = $n"
Write-Host "Number of runs  = $runs"
Write-Host "========================================="

# Initialize the CSV results file with appropriate column headers if it does not already exist
$csv_file = "results.csv"
if (-Not (Test-Path $csv_file)) {
    Set-Content -Path $csv_file -Value "Language/Mode,Vector Size,Run Number,Execution Time"
}

# Append a visual delimiter to clearly separate different benchmark sessions within the CSV
Add-Content -Path $csv_file -Value ",,,"
Add-Content -Path $csv_file -Value "=== NEW TEST SESSION ===,,,,"

# Dynamically inject the CLion MinGW binary directory into the system PATH
# This ensures that the dynamically linked C++ executables can locate required DLLs at runtime
$mingw_path = "C:\Program Files\JetBrains\CLion 2026.2.3.1\bin\mingw\bin"
if (Test-Path $mingw_path) {
    $env:PATH = "$mingw_path;" + $env:PATH
}

# Function to execute a given executable multiple times, measure execution time, and calculate the average
function Run-Benchmark($exe_path, $name, $cmd_prefix) {
    Write-Host "`n[$name] Checking for executable..."
    if (-Not (Test-Path $exe_path)) {
        Write-Host "WARNING: $name executable not found." -ForegroundColor Yellow
        Write-Host "Make sure you have built the '$name' profile in CLion." -ForegroundColor Yellow
        return
    }

    Write-Host "[$name] Running tests..."
    $sum = 0.0

    for ($i = 0; $i -lt $runs; $i++) {
        # Execute the process and capture standard output
        $output = (cmd /c $cmd_prefix .\$exe_path $n 2>&1)
        
        # Verify that the process executed successfully and returned the execution time
        if ($output.count -eq 0 -or $output -eq $null) {
            Write-Host "   Run $($i+1): Program failed silently! Output is empty." -ForegroundColor Red
            continue
        }
        
        # Parse the execution time from the console output
        if ($output -is [array]) {
            $time = $output[$output.length - 1] -as [double]
        } else {
            $time = $output -as [double]
        }
        
        Write-Host "   Run $($i+1): $time ms"
        $sum += $time
        
        # Log the individual run metric to the CSV file, formatting to 6 decimal places to prevent scientific notation
        $time_formatted = "{0:F6}" -f $time
        Add-Content -Path $csv_file -Value "$name,$n inputs,Run $($i+1),$time_formatted ms"
    }

    # Calculate the average execution time across all successful runs
    $avg = $sum / $runs
    $avg_formatted = "{0:F6}" -f $avg
    Write-Host "[$name] Average time: $avg_formatted ms" -ForegroundColor Green
    
    # Append the calculated average to the CSV file
    Add-Content -Path $csv_file -Value "$name,$n inputs,Average,$avg_formatted ms"
}

# Execute benchmarks for both C++ build profiles (Debug/Unoptimized vs Release/Optimized)
Run-Benchmark -exe_path $c_debug_exe -name "C++ (Debug)" -cmd_prefix ""
Run-Benchmark -exe_path $c_release_exe -name "C++ (Release)" -cmd_prefix ""

# Compile and execute the Java implementation
Write-Host "`n[Java] Compiling..."
javac $java_src

if ($LASTEXITCODE -ne 0) {
    Write-Host "ERROR: Java compilation failed." -ForegroundColor Red
} else {
    Write-Host "[Java] Compilation successful. Running tests..."
    $sum_j = 0.0

    for ($i = 0; $i -lt $runs; $i++) {
        $output = (cmd /c java -cp $java_dir Main $n 2>&1)
        
        if ($output -is [array]) {
            $time_j = $output[$output.length - 1] -as [double]
        } else {
            $time_j = $output -as [double]
        }
        
        Write-Host "   Run $($i+1): $time_j ms"
        $sum_j += $time_j
        
        $time_j_formatted = "{0:F6}" -f $time_j
        Add-Content -Path $csv_file -Value "Java,$n inputs,Run $($i+1),$time_j_formatted ms"
    }

    # Calculate and log the final Java average
    $avg_j = $sum_j / $runs
    $avg_j_formatted = "{0:F6}" -f $avg_j
    Write-Host "[Java] Average time: $avg_j_formatted ms" -ForegroundColor Green
    
    Add-Content -Path $csv_file -Value "Java,$n inputs,Average,$avg_j_formatted ms"
}

Write-Host "`nTests completed."
