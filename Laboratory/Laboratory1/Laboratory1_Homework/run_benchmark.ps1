param (
    [int]$runs = 10
)

$jdk_path = "C:\Program Files\JetBrains\IntelliJ IDEA 2026.2.3\jbr\bin"
if (Test-Path $jdk_path) {
    $env:PATH = "$jdk_path;" + $env:PATH
}

$mingw_path = "C:\Program Files\JetBrains\CLion 2026.2.3.1\bin\mingw\bin"
if (Test-Path $mingw_path) {
    $env:PATH = "$mingw_path;" + $env:PATH
}

$javaDir = "$PWD\ConvolutionMatrixJava"
$cppDir = "$PWD\ConvolutionMatrixC++"

if (-Not (Test-Path "$javaDir\bin")) {
    New-Item -ItemType Directory -Path "$javaDir\bin" -Force | Out-Null
}

Write-Host "Building Java..."
cmd /c "javac -d `"$javaDir\bin`" `"$javaDir\src\Main.java`""
if ($LASTEXITCODE -ne 0) {
    Write-Host "Java compilation failed!" -ForegroundColor Red
    exit 1
}

Write-Host "Building C++ (C++26)..."
$gpp = "$mingw_path\g++.exe"
& $gpp -O3 -std=c++26 "$cppDir\main.cpp" -o "$cppDir\ConvolutionDynamic.exe"
& $gpp -O3 -std=c++26 "$cppDir\main_static.cpp" -o "$cppDir\ConvolutionStatic.exe"
if ($LASTEXITCODE -ne 0) {
    Write-Host "C++ compilation failed!" -ForegroundColor Red
    exit 1
}

$matrixF = "$PWD\MatrixF.txt"
$matrixC = "$PWD\ConvolutionM.txt"
$outSeq = "$PWD\output_seq.txt"
$outTxt = "$PWD\output.txt"

function Generate-Inputs($n, $m, $k) {
    Write-Host "Generating inputs ($n x $m, k=$k)..."
    $rand = [System.Random]::new(42)

    $fWriter = [System.IO.StreamWriter]::new($matrixF)
    $fWriter.WriteLine("$n $m")
    for ($i = 0; $i -lt $n; $i++) {
        $sb = [System.Text.StringBuilder]::new()
        for ($j = 0; $j -lt $m; $j++) {
            [void]$sb.Append($rand.Next(0, 100)).Append(" ")
        }
        $fWriter.WriteLine($sb.ToString().TrimEnd())
    }
    $fWriter.Close()

    $cWriter = [System.IO.StreamWriter]::new($matrixC)
    $cWriter.WriteLine("$k")
    for ($i = 0; $i -lt $k; $i++) {
        $sb = [System.Text.StringBuilder]::new()
        for ($j = 0; $j -lt $k; $j++) {
            [void]$sb.Append($rand.Next(0, 10)).Append(" ")
        }
        $cWriter.WriteLine($sb.ToString().TrimEnd())
    }
    $cWriter.Close()
}

$csvJava = "$PWD\results_java.csv"
$csvCpp = "$PWD\results_cpp.csv"

Set-Content -Path $csvJava -Value "Matrix Dimensions,Kernel,Algorithm,Threads,Average Time (ms),Speedup"
Set-Content -Path $csvCpp -Value "Matrix Dimensions,Kernel,Allocation,Algorithm,Threads,Average Time (ms),Speedup"

function Parse-Time($out) {
    if ($null -eq $out) { return 0.0 }
    if ($out -is [array]) {
        $last = $out[$out.length - 1].ToString().Trim()
    } else {
        $last = $out.ToString().Trim()
    }
    return [double]$last
}

function Run-Java($mode, $p) {
    $out = (cmd /c "java -Xmx8g -cp `"$javaDir\bin`" Main $mode $p `"$matrixF`" `"$matrixC`"" 2>&1)
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Java error: $out" -ForegroundColor Red
        return $null
    }
    return Parse-Time $out
}

function Run-Cpp($exe, $mode, $p) {
    $out = (cmd /c "`"$cppDir\$exe`" $mode $p `"$matrixF`" `"$matrixC`"" 2>&1)
    if ($LASTEXITCODE -ne 0) {
        Write-Host "C++ error: $out" -ForegroundColor Red
        return $null
    }
    return Parse-Time $out
}

$algorithms = @("horizontal", "vertical", "block", "cyclic")

function Benchmark-Suite($name, $n, $m, $k, [int[]]$threadCounts) {
    Write-Host "`n=================================================="
    Write-Host "Test Case: $name ($n x $m, k=$k)"
    Write-Host "=================================================="
    Generate-Inputs -n $n -m $m -k $k

    if (Test-Path $outSeq) { Remove-Item $outSeq -Force }

    Write-Host "`n[Java] Running Sequential Baseline..."
    $jSeqTimes = @()
    for ($r = 0; $r -lt $runs; $r++) {
        $t = Run-Java -mode "sequential" -p 1
        $jSeqTimes += $t
    }
    Copy-Item $outTxt $outSeq -Force
    $jSeqAvg = ($jSeqTimes | Measure-Object -Average).Average
    Write-Host "[Java] Sequential Avg: $jSeqAvg ms"
    Add-Content -Path $csvJava -Value "$($n)x$($m),$k,Sequential,1,$([math]::Round($jSeqAvg, 4)),1.00"

    Write-Host "`n[C++ Dynamic] Running Sequential Baseline..."
    $cppDynSeqTimes = @()
    for ($r = 0; $r -lt $runs; $r++) {
        $t = Run-Cpp -exe "ConvolutionDynamic.exe" -mode "sequential" -p 1
        $cppDynSeqTimes += $t
    }
    $cppDynSeqAvg = ($cppDynSeqTimes | Measure-Object -Average).Average
    Write-Host "[C++ Dynamic] Sequential Avg: $cppDynSeqAvg ms"
    Add-Content -Path $csvCpp -Value "$($n)x$($m),$k,Dynamic,Sequential,1,$([math]::Round($cppDynSeqAvg, 4)),1.00"

    Write-Host "`n[C++ Static] Running Sequential Baseline..."
    $cppStatSeqTimes = @()
    for ($r = 0; $r -lt $runs; $r++) {
        $t = Run-Cpp -exe "ConvolutionStatic.exe" -mode "sequential" -p 1
        $cppStatSeqTimes += $t
    }
    $cppStatSeqAvg = ($cppStatSeqTimes | Measure-Object -Average).Average
    Write-Host "[C++ Static] Sequential Avg: $cppStatSeqAvg ms"
    Add-Content -Path $csvCpp -Value "$($n)x$($m),$k,Static,Sequential,1,$([math]::Round($cppStatSeqAvg, 4)),1.00"

    foreach ($p in $threadCounts) {
        Write-Host "`n--- Testing with p = $p threads ---"

        foreach ($algo in $algorithms) {
            $times = @()
            for ($r = 0; $r -lt $runs; $r++) {
                $t = Run-Java -mode $algo -p $p
                $times += $t
            }
            $avg = ($times | Measure-Object -Average).Average
            $speedup = if ($avg -gt 0) { [math]::Round(($jSeqAvg / $avg), 2) } else { 1.00 }
            Write-Host "[Java] $algo (p=$p): Avg $avg ms | Speedup: $speedup x"
            Add-Content -Path $csvJava -Value "$($n)x$($m),$k,$algo,$p,$([math]::Round($avg, 4)),$speedup"
        }

        foreach ($algo in $algorithms) {
            $times = @()
            for ($r = 0; $r -lt $runs; $r++) {
                $t = Run-Cpp -exe "ConvolutionDynamic.exe" -mode $algo -p $p
                $times += $t
            }
            $avg = ($times | Measure-Object -Average).Average
            $speedup = if ($avg -gt 0) { [math]::Round(($cppDynSeqAvg / $avg), 2) } else { 1.00 }
            Write-Host "[C++ Dynamic] $algo (p=$p): Avg $avg ms | Speedup: $speedup x"
            Add-Content -Path $csvCpp -Value "$($n)x$($m),$k,Dynamic,$algo,$p,$([math]::Round($avg, 4)),$speedup"
        }

        foreach ($algo in $algorithms) {
            $times = @()
            for ($r = 0; $r -lt $runs; $r++) {
                $t = Run-Cpp -exe "ConvolutionStatic.exe" -mode $algo -p $p
                $times += $t
            }
            $avg = ($times | Measure-Object -Average).Average
            $speedup = if ($avg -gt 0) { [math]::Round(($cppStatSeqAvg / $avg), 2) } else { 1.00 }
            Write-Host "[C++ Static] $algo (p=$p): Avg $avg ms | Speedup: $speedup x"
            Add-Content -Path $csvCpp -Value "$($n)x$($m),$k,Static,$algo,$p,$([math]::Round($avg, 4)),$speedup"
        }
    }
}

Benchmark-Suite -name "TC1" -n 10 -m 10 -k 3 -threadCounts @(4)
Benchmark-Suite -name "TC2" -n 1000 -m 1000 -k 3 -threadCounts @(2, 4, 8, 16)
Benchmark-Suite -name "TC3" -n 10 -m 10000 -k 5 -threadCounts @(2, 4, 8, 16)
Benchmark-Suite -name "TC4" -n 10000 -m 10 -k 5 -threadCounts @(2, 4, 8, 16)
Benchmark-Suite -name "TC5" -n 10000 -m 10000 -k 5 -threadCounts @(2, 4, 8, 16)

Write-Host "`nAll benchmark runs completed! Results exported to $csvJava and $csvCpp." -ForegroundColor Green
