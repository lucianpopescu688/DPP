param (
    [int]$runs = 10,
    [string]$testCase = "all",
    [int]$n = 0,
    [int]$m = 0,
    [int[]]$threads = @()
)

$jdk = "C:\Program Files\JetBrains\IntelliJ IDEA 2026.2.3\jbr\bin"
$mingw = "C:\Program Files\JetBrains\CLion 2026.2.3.1\bin\mingw\bin"
if (Test-Path $jdk)   { $env:PATH = "$jdk;$env:PATH" }
if (Test-Path $mingw) { $env:PATH = "$mingw;$env:PATH" }

$javaDir = "$PWD\Laboratory2_Java\Laboratory2_Java"
if (-not (Test-Path "$javaDir\src\Main.java")) { $javaDir = "$PWD\Laboratory2_Java" }
$cppDir  = "$PWD\Laboratory2_C++"
$csvJava = "$PWD\results_java_lab2.csv"
$csvCpp  = "$PWD\results_cpp_lab2.csv"

# 1. Compile Java (Java 25) & C++ (C++26)
New-Item -ItemType Directory -Path "$javaDir\bin" -Force | Out-Null
Write-Host "Compiling Java (Java 25) and C++ (C++26)..." -ForegroundColor Yellow
cmd /c "javac -d `"$javaDir\bin`" `"$javaDir\src\Main.java`""
$gpp = if (Test-Path "$mingw\g++.exe") { "$mingw\g++.exe" } else { "g++" }
& $gpp -O3 -std=c++2c -pthread "$cppDir\main.cpp" -o "$cppDir\Lab2InPlace.exe"
if ($LASTEXITCODE -ne 0) { Write-Error "Compilation failed!"; exit 1 }

if ($testCase -eq "all" -or -not (Test-Path $csvJava)) {
    Set-Content $csvJava "Matrix Dimensions,Kernel,Algorithm,Threads,Average Time (ms),Speedup"
    Set-Content $csvCpp  "Matrix Dimensions,Kernel,Algorithm,Threads,Average Time (ms),Speedup"
}

# 2. Helper: Executes a command $runs times, prints each run on its own row, and returns the average
function Get-AvgTime([string]$label, [scriptblock]$cmd) {
    Write-Host "  $label" -ForegroundColor White
    $times = for ($r = 0; $r -lt $runs; $r++) {
        $out = & $cmd 2>&1
        if ($LASTEXITCODE -ne 0) { Write-Error "`nRun $($r+1) failed: $out"; exit 1 }
        $last = if ($out -is [array]) { $out[-1].ToString().Trim() } else { $out.ToString().Trim() }
        $val = [double]$last
        $runStr = ($r + 1).ToString("00")
        Write-Host "    - Run ${runStr}: $([math]::Round($val, 4)) ms" -ForegroundColor DarkGray
        $val
    }
    $avg = ($times | Measure-Object -Average).Average
    Write-Host "    ===> Average: $([math]::Round($avg, 4)) ms`n" -ForegroundColor Green
    return $avg
}

# 3. Benchmark runner for a test case
function Run-TestCase($name, $nVal, $mVal, $kVal, [int[]]$threadList) {
    Write-Host "`n============================================================" -ForegroundColor Cyan
    Write-Host "  $name - $nVal x $mVal matrix, kernel $kVal x $kVal ($runs iterations per test)" -ForegroundColor White
    Write-Host "  Testing all threads: p = $($threadList -join ', ')" -ForegroundColor Yellow
    Write-Host "============================================================" -ForegroundColor Cyan

    powershell -ExecutionPolicy Bypass -File .\generateMatrices.ps1 -n $nVal -m $mVal -k $kVal
    Remove-Item "$PWD\output_seq.txt" -ErrorAction SilentlyContinue

    Write-Host "`n--- [Sequential Baselines] ---" -ForegroundColor Yellow
    $jSeq = Get-AvgTime "Java 25 Sequential" { cmd /c "java -Xmx8g -cp `"$javaDir\bin`" Main sequential 1 `"$PWD\date.txt`"" }
    $cSeq = Get-AvgTime "C++26   Sequential" { cmd /c "`"$cppDir\Lab2InPlace.exe`" sequential 1 `"$PWD\date.txt`"" }

    Add-Content $csvJava "$($nVal)x$($mVal),$kVal,Sequential,1,$([math]::Round($jSeq, 4)),1.00"
    Add-Content $csvCpp  "$($nVal)x$($mVal),$kVal,Sequential,1,$([math]::Round($cSeq, 4)),1.00"

    Write-Host "--- [Parallel In-Place (Barrier Sync across all p)] ---" -ForegroundColor Yellow
    foreach ($p in $threadList) {
        $jTime = Get-AvgTime "Java 25 (p=$p)" { cmd /c "java -Xmx8g -cp `"$javaDir\bin`" Main parallel $p `"$PWD\date.txt`"" }
        $jSpd  = if ($jTime -gt 0) { [math]::Round($jSeq / $jTime, 2) } else { 1.00 }
        Add-Content $csvJava "$($nVal)x$($mVal),$kVal,Parallel_Barrier,$p,$([math]::Round($jTime, 4)),$jSpd"

        $cTime = Get-AvgTime "C++26   (p=$p)" { cmd /c "`"$cppDir\Lab2InPlace.exe`" parallel $p `"$PWD\date.txt`"" }
        $cSpd  = if ($cTime -gt 0) { [math]::Round($cSeq / $cTime, 2) } else { 1.00 }
        Add-Content $csvCpp "$($nVal)x$($mVal),$kVal,Parallel_Barrier,$p,$([math]::Round($cTime, 4)),$cSpd"

        Write-Host "  >> Speedup vs Sequential (p=$p): Java = $($jSpd)x | C++ = $($cSpd)x`n" -ForegroundColor Cyan
    }

    if ($nVal -le 10 -and $mVal -le 10) {
        Write-Host "============================================================" -ForegroundColor Cyan
        Write-Host "  Resulting Filtered Matrix output.txt ($nVal x $mVal):" -ForegroundColor Yellow
        Write-Host "============================================================" -ForegroundColor Cyan
        Get-Content "output.txt"
        Write-Host ""
    }
}

# Determine default thread list helper
function Get-DefaultThreads([int]$rowCount) {
    if ($rowCount -le 4) { return @(2) }
    if ($rowCount -le 6) { return @(2, 4) }
    if ($rowCount -le 10) { return @(2, 4, 8, 16) }
    return @(2, 4, 8, 16)
}

# Custom input mode (if user specified -n)
if ($n -gt 0) {
    if ($m -le 0) { $m = $n }
    $runThreads = if ($threads.Count -gt 0) { $threads } else { Get-DefaultThreads $n }
    Run-TestCase "CustomSmall" $n $m 3 $runThreads
    exit 0
}

# Small test case mode (-testCase small)
if ($testCase -eq "small" -or $testCase -eq "0") {
    $runThreads = if ($threads.Count -gt 0) { $threads } else { @(2, 4) }
    Run-TestCase "SmallDemo" 6 6 3 $runThreads
    exit 0
}

# Standard test cases
if ($testCase -eq "all" -or $testCase -eq "1" -or $testCase -eq "tc1") {
    $tc1Threads = if ($threads.Count -gt 0) { $threads } else { @(2, 4, 8, 16) }
    Run-TestCase "TC1" 10 10 3 $tc1Threads
}
if ($testCase -eq "all" -or $testCase -eq "2" -or $testCase -eq "tc2") {
    $tc2Threads = if ($threads.Count -gt 0) { $threads } else { @(2, 4, 8, 16) }
    Run-TestCase "TC2" 1000 1000 3 $tc2Threads
}
if ($testCase -eq "all" -or $testCase -eq "3" -or $testCase -eq "tc3") {
    $tc3Threads = if ($threads.Count -gt 0) { $threads } else { @(2, 4, 8, 16) }
    Run-TestCase "TC3" 10000 10000 3 $tc3Threads
}

Write-Host "All benchmarks completed! Results saved to:" -ForegroundColor Green
Write-Host "  - $csvJava"
Write-Host "  - $csvCpp"
