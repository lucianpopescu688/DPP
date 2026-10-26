param ([int]$runs = 10)

$jdk = "C:\Program Files\JetBrains\IntelliJ IDEA 2026.2.3\jbr\bin"
$mingw = "C:\Program Files\JetBrains\CLion 2026.2.3.1\bin\mingw\bin"
if (Test-Path $jdk)   { $env:PATH = "$jdk;$env:PATH" }
if (Test-Path $mingw) { $env:PATH = "$mingw;$env:PATH" }

$javaDir = "$PWD\Laboratory2_Java\Laboratory2_Java"
if (-not (Test-Path "$javaDir\src\Main.java")) { $javaDir = "$PWD\Laboratory2_Java" }
$cppDir  = "$PWD\Laboratory2_C++"
$csvJava = "$PWD\results_java_lab2.csv"
$csvCpp  = "$PWD\results_cpp_lab2.csv"

New-Item -ItemType Directory -Path "$javaDir\bin" -Force | Out-Null
Write-Host "Compiling Java and C++20..."
cmd /c "javac -d `"$javaDir\bin`" `"$javaDir\src\Main.java`""
$gpp = if (Test-Path "$mingw\g++.exe") { "$mingw\g++.exe" } else { "g++" }
& $gpp -O3 -std=c++20 "$cppDir\main.cpp" -o "$cppDir\Lab2InPlace.exe"
if ($LASTEXITCODE -ne 0) { Write-Error "Compilation failed!"; exit 1 }

Set-Content $csvJava "Matrix Dimensions,Kernel,Variant,Threads,Average Time (ms),Speedup"
Set-Content $csvCpp  "Matrix Dimensions,Kernel,Variant,Threads,Average Time (ms),Speedup"

function Get-AvgTime([scriptblock]$cmd) {
    $times = for ($r = 0; $r -lt $runs; $r++) {
        $out = & $cmd 2>&1
        if ($LASTEXITCODE -ne 0) { Write-Error "Run failed: $out"; exit 1 }
        $last = if ($out -is [array]) { $out[-1].ToString().Trim() } else { $out.ToString().Trim() }
        [double]$last
    }
    return ($times | Measure-Object -Average).Average
}

$variants = @("1b", "1f", "2b", "2f")

function Run-TestCase($name, $n, $m, $k, [int[]]$threads) {
    Write-Host "`n=== $name ($n x $m, k=$k) ===" -ForegroundColor Cyan
    powershell -ExecutionPolicy Bypass -File .\generateMatrices.ps1 -n $n -m $m -k $k
    Remove-Item "$PWD\output_seq.txt" -ErrorAction SilentlyContinue

    $jSeq = Get-AvgTime { cmd /c "java -Xmx8g -cp `"$javaDir\bin`" Main sequential 1 `"$PWD\date.txt`"" }
    $cSeq = Get-AvgTime { cmd /c "`"$cppDir\Lab2InPlace.exe`" sequential 1 `"$PWD\date.txt`"" }

    Add-Content $csvJava "$($n)x$($m),$k,Sequential,1,$([math]::Round($jSeq, 4)),1.00"
    Add-Content $csvCpp  "$($n)x$($m),$k,Sequential,1,$([math]::Round($cSeq, 4)),1.00"
    Write-Host "Sequential -> Java: $([math]::Round($jSeq,4))ms | C++: $([math]::Round($cSeq,4))ms"

    foreach ($p in $threads) {
        foreach ($v in $variants) {
            $jTime = Get-AvgTime { cmd /c "java -Xmx8g -cp `"$javaDir\bin`" Main $v $p `"$PWD\date.txt`"" }
            $jSpd  = if ($jTime -gt 0) { [math]::Round($jSeq / $jTime, 2) } else { 1.00 }
            Add-Content $csvJava "$($n)x$($m),$k,Variant_$v,$p,$([math]::Round($jTime, 4)),$jSpd"

            $cTime = Get-AvgTime { cmd /c "`"$cppDir\Lab2InPlace.exe`" $v $p `"$PWD\date.txt`"" }
            $cSpd  = if ($cTime -gt 0) { [math]::Round($cSeq / $cTime, 2) } else { 1.00 }
            Add-Content $csvCpp "$($n)x$($m),$k,Variant_$v,$p,$([math]::Round($cTime, 4)),$cSpd"

            Write-Host "p=$p [Variant $v] -> Java: $([math]::Round($jTime,2))ms ($($jSpd)x) | C++: $([math]::Round($cTime,2))ms ($($cSpd)x)"
        }
    }
}

# Official Laboratory 2 Test Cases
Run-TestCase "TC1" 10 10 3 @(2)
Run-TestCase "TC2" 1000 1000 3 @(2, 4, 8, 16)
Run-TestCase "TC3" 10000 10000 3 @(2, 4, 8, 16)
