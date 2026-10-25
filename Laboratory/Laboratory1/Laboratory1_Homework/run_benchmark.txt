param ([int]$runs = 10)

$jdk = "C:\Program Files\JetBrains\IntelliJ IDEA 2026.2.3\jbr\bin"
$mingw = "C:\Program Files\JetBrains\CLion 2026.2.3.1\bin\mingw\bin"
if (Test-Path $jdk)   { $env:PATH = "$jdk;$env:PATH" }
if (Test-Path $mingw) { $env:PATH = "$mingw;$env:PATH" }

$javaDir = "$PWD\ConvolutionMatrixJava"
$cppDir  = "$PWD\ConvolutionMatrixC++"
$csvJava = "$PWD\results_java.csv"
$csvCpp  = "$PWD\results_cpp.csv"

New-Item -ItemType Directory -Path "$javaDir\bin" -Force | Out-Null
Write-Host "Compiling Java and C++..."
cmd /c "javac -d `"$javaDir\bin`" `"$javaDir\src\Main.java`""
$gpp = "$mingw\g++.exe"
& $gpp -O3 -std=c++20 "$cppDir\main.cpp" -o "$cppDir\ConvolutionDynamic.exe"
& $gpp -O3 -std=c++20 "$cppDir\main_static.cpp" -o "$cppDir\ConvolutionStatic.exe"
if ($LASTEXITCODE -ne 0) { Write-Error "Compilation failed!"; exit 1 }

Set-Content $csvJava "Matrix Dimensions,Kernel,Algorithm,Threads,Average Time (ms),Speedup"
Set-Content $csvCpp  "Matrix Dimensions,Kernel,Allocation,Algorithm,Threads,Average Time (ms),Speedup"

function Get-AvgTime([scriptblock]$cmd) {
    $times = for ($r = 0; $r -lt $runs; $r++) {
        $out = & $cmd 2>&1
        if ($LASTEXITCODE -ne 0) { Write-Error "Run failed: $out"; exit 1 }
        $last = if ($out -is [array]) { $out[-1].ToString().Trim() } else { $out.ToString().Trim() }
        [double]$last
    }
    return ($times | Measure-Object -Average).Average
}

$algorithms = @("horizontal", "vertical", "block", "cyclic")

function Run-TestCase($name, $n, $m, $k, [int[]]$threads) {
    Write-Host "`n=== $name ($n x $m, k=$k) ===" -ForegroundColor Cyan
    powershell -ExecutionPolicy Bypass -File .\generateMatrices.ps1 -n $n -m $m -k $k
    Remove-Item "$PWD\output_seq.txt" -ErrorAction SilentlyContinue

    $jSeq    = Get-AvgTime { cmd /c "java -Xmx8g -cp `"$javaDir\bin`" Main sequential 1 `"$PWD\MatrixF.txt`" `"$PWD\ConvolutionM.txt`"" }
    $cDynSeq = Get-AvgTime { cmd /c "`"$cppDir\ConvolutionDynamic.exe`" sequential 1 `"$PWD\MatrixF.txt`" `"$PWD\ConvolutionM.txt`"" }
    $cStaSeq = Get-AvgTime { cmd /c "`"$cppDir\ConvolutionStatic.exe`" sequential 1 `"$PWD\MatrixF.txt`" `"$PWD\ConvolutionM.txt`"" }

    Add-Content $csvJava "$($n)x$($m),$k,Sequential,1,$([math]::Round($jSeq, 4)),1.00"
    Add-Content $csvCpp  "$($n)x$($m),$k,Dynamic,Sequential,1,$([math]::Round($cDynSeq, 4)),1.00"
    Add-Content $csvCpp  "$($n)x$($m),$k,Static,Sequential,1,$([math]::Round($cStaSeq, 4)),1.00"

    foreach ($p in $threads) {
        foreach ($algo in $algorithms) {
            $jTime = Get-AvgTime { cmd /c "java -Xmx8g -cp `"$javaDir\bin`" Main $algo $p `"$PWD\MatrixF.txt`" `"$PWD\ConvolutionM.txt`"" }
            $jSpd  = if ($jTime -gt 0) { [math]::Round($jSeq / $jTime, 2) } else { 1.00 }
            Add-Content $csvJava "$($n)x$($m),$k,$algo,$p,$([math]::Round($jTime, 4)),$jSpd"

            $dTime = Get-AvgTime { cmd /c "`"$cppDir\ConvolutionDynamic.exe`" $algo $p `"$PWD\MatrixF.txt`" `"$PWD\ConvolutionM.txt`"" }
            $dSpd  = if ($dTime -gt 0) { [math]::Round($cDynSeq / $dTime, 2) } else { 1.00 }
            Add-Content $csvCpp "$($n)x$($m),$k,Dynamic,$algo,$p,$([math]::Round($dTime, 4)),$dSpd"

            $sTime = Get-AvgTime { cmd /c "`"$cppDir\ConvolutionStatic.exe`" $algo $p `"$PWD\MatrixF.txt`" `"$PWD\ConvolutionM.txt`"" }
            $sSpd  = if ($sTime -gt 0) { [math]::Round($cStaSeq / $sTime, 2) } else { 1.00 }
            Add-Content $csvCpp "$($n)x$($m),$k,Static,$algo,$p,$([math]::Round($sTime, 4)),$sSpd"

            Write-Host "p=$p [$algo] -> Java: $([math]::Round($jTime,2))ms | C++ Dyn: $([math]::Round($dTime,2))ms | C++ Stat: $([math]::Round($sTime,2))ms"
        }
    }
}

Run-TestCase "TC1" 10 10 3 @(4)
Run-TestCase "TC2" 1000 1000 3 @(2, 4, 8, 16)
Run-TestCase "TC3" 10 10000 5 @(2, 4, 8, 16)
Run-TestCase "TC4" 10000 10 5 @(2, 4, 8, 16)
Run-TestCase "TC5" 10000 10000 5 @(2, 4, 8, 16)
