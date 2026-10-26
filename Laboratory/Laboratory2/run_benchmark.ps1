param (
    [int]$runs = 10,
    [string]$testCase = "all"
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

# 2. Helper: Executes a command $runs times, prints each run live, and returns the average
function Get-AvgTime([string]$label, [scriptblock]$cmd) {
    Write-Host ("  {0,-20} : " -f $label) -NoNewline
    $times = for ($r = 0; $r -lt $runs; $r++) {
        $out = & $cmd 2>&1
        if ($LASTEXITCODE -ne 0) { Write-Error "`nRun $($r+1) failed: $out"; exit 1 }
        $last = if ($out -is [array]) { $out[-1].ToString().Trim() } else { $out.ToString().Trim() }
        $val = [double]$last
        Write-Host " [R$($r+1): $([math]::Round($val, 2))ms]" -NoNewline -ForegroundColor DarkGray
        $val
    }
    $avg = ($times | Measure-Object -Average).Average
    Write-Host " => Avg: $([math]::Round($avg, 4))ms" -ForegroundColor Green
    return $avg
}

# 3. Benchmark runner for a single test case
function Run-TestCase($name, $n, $m, $k, [int[]]$threads) {
    Write-Host "`n============================================================" -ForegroundColor Cyan
    Write-Host "  $name - $n x $m matrix, kernel $k x $k (Running $runs iterations each)" -ForegroundColor White
    Write-Host "============================================================" -ForegroundColor Cyan

    powershell -ExecutionPolicy Bypass -File .\generateMatrices.ps1 -n $n -m $m -k $k
    Remove-Item "$PWD\output_seq.txt" -ErrorAction SilentlyContinue

    Write-Host "`n[Sequential Baselines]" -ForegroundColor Yellow
    $jSeq = Get-AvgTime "Java 25 Sequential" { cmd /c "java -Xmx8g -cp `"$javaDir\bin`" Main sequential 1 `"$PWD\date.txt`"" }
    $cSeq = Get-AvgTime "C++26   Sequential" { cmd /c "`"$cppDir\Lab2InPlace.exe`" sequential 1 `"$PWD\date.txt`"" }

    Add-Content $csvJava "$($n)x$($m),$k,Sequential,1,$([math]::Round($jSeq, 4)),1.00"
    Add-Content $csvCpp  "$($n)x$($m),$k,Sequential,1,$([math]::Round($cSeq, 4)),1.00"

    Write-Host "`n[Parallel In-Place (Barrier Sync)]" -ForegroundColor Yellow
    foreach ($p in $threads) {
        $jTime = Get-AvgTime "Java 25 (p=$p)" { cmd /c "java -Xmx8g -cp `"$javaDir\bin`" Main parallel $p `"$PWD\date.txt`"" }
        $jSpd  = if ($jTime -gt 0) { [math]::Round($jSeq / $jTime, 2) } else { 1.00 }
        Add-Content $csvJava "$($n)x$($m),$k,Parallel_Barrier,$p,$([math]::Round($jTime, 4)),$jSpd"

        $cTime = Get-AvgTime "C++26   (p=$p)" { cmd /c "`"$cppDir\Lab2InPlace.exe`" parallel $p `"$PWD\date.txt`"" }
        $cSpd  = if ($cTime -gt 0) { [math]::Round($cSeq / $cTime, 2) } else { 1.00 }
        Add-Content $csvCpp "$($n)x$($m),$k,Parallel_Barrier,$p,$([math]::Round($cTime, 4)),$cSpd"

        Write-Host "  -> Speedup vs Seq: Java = $($jSpd)x | C++ = $($cSpd)x" -ForegroundColor Cyan
    }
}

# Run according to user selection
if ($testCase -eq "all" -or $testCase -eq "1" -or $testCase -eq "tc1") {
    Run-TestCase "TC1" 10 10 3 @(2)
}
if ($testCase -eq "all" -or $testCase -eq "2" -or $testCase -eq "tc2") {
    Run-TestCase "TC2" 1000 1000 3 @(2, 4, 8, 16)
}
if ($testCase -eq "all" -or $testCase -eq "3" -or $testCase -eq "tc3") {
    Run-TestCase "TC3" 10000 10000 3 @(2, 4, 8, 16)
}

Write-Host "`nAll benchmarks completed! Results saved to:" -ForegroundColor Green
Write-Host "  - $csvJava"
Write-Host "  - $csvCpp"
