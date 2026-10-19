# ==============================================================================
# Interactive Lab 1 Demonstration Menu
# ==============================================================================

# Ensure JDK and CLion MinGW are in PATH if available
$jdk = "C:\Program Files\JetBrains\IntelliJ IDEA 2026.2.3\jbr\bin"
$mingw = "C:\Program Files\JetBrains\CLion 2026.2.3.1\bin\mingw\bin"
if (Test-Path $jdk)   { $env:PATH = "$jdk;$env:PATH" }
if (Test-Path $mingw) { $env:PATH = "$mingw;$env:PATH" }

$javaDir = "$PWD\ConvolutionMatrixJava"
$cppDir  = "$PWD\ConvolutionMatrixC++"

function Show-Header {
    Clear-Host
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "   DPP Laboratory 1 - Interactive Lab Demonstration Menu    " -ForegroundColor White
    Write-Host "============================================================" -ForegroundColor Cyan
}

function Run-Verification($n, $m, $k, $p) {
    Write-Host "`n[1/3] Generating random matrices ($n x $m, kernel $k x $k)..." -ForegroundColor Yellow
    powershell -ExecutionPolicy Bypass -File .\generateMatrices.ps1 -n $n -m $m -k $k

    Remove-Item "output_seq.txt" -ErrorAction SilentlyContinue

    Write-Host "`n[2/3] Running Java Implementation (p=$p)..." -ForegroundColor Green
    java -cp "$javaDir\bin" Main all $p

    Write-Host "`n[3/3] Running C++ Implementation (p=$p)..." -ForegroundColor Green
    Write-Host "-- C++ Dynamic Allocation (int**) --" -ForegroundColor DarkCyan
    & "$cppDir\ConvolutionDynamic.exe" all $p

    Write-Host "`n-- C++ Static Allocation (compile-time .bss) --" -ForegroundColor DarkCyan
    & "$cppDir\ConvolutionStatic.exe" all $p

    if ($n -le 10 -and $m -le 10) {
        Write-Host "`n--- Resulting Matrix output.txt ($n x $m) ---" -ForegroundColor Yellow
        Get-Content "output.txt"
    }
}

function Run-Custom {
    Show-Header
    Write-Host "--- CUSTOM TEST CONFIGURATION ---`n" -ForegroundColor White

    $n = Read-Host "Enter number of rows n [default 10]"
    if ([string]::IsNullOrWhiteSpace($n)) { $n = 10 } else { $n = [int]$n }

    $m = Read-Host "Enter number of columns m [default 10]"
    if ([string]::IsNullOrWhiteSpace($m)) { $m = 10 } else { $m = [int]$m }

    $k = Read-Host "Enter kernel size k (odd: 3, 5) [default 3]"
    if ([string]::IsNullOrWhiteSpace($k)) { $k = 3 } else { $k = [int]$k }

    $p = Read-Host "Enter number of threads p [default 4]"
    if ([string]::IsNullOrWhiteSpace($p)) { $p = 4 } else { $p = [int]$p }

    Run-Verification $n $m $k $p
}

function Main-Menu {
    while ($true) {
        Show-Header
        Write-Host "Select an option to demonstrate:" -ForegroundColor White
        Write-Host "  [1] Quick Demo: Small 5 x 5 Matrix (Kernel 3x3, p=4) - Shows all + output"
        Write-Host "  [2] Custom Input: Enter your own n, m, k, p in terminal"
        Write-Host "  [3] Run Official Test Case TC1 (10 x 10, k=3, p=4)"
        Write-Host "  [4] Run Official Test Case TC2 (1000 x 1000, k=3, p=4)"
        Write-Host "  [5] Run Official Test Case TC3 (10 x 10000 - Wide, k=5, p=4)"
        Write-Host "  [6] Run Official Test Case TC4 (10000 x 10 - Tall, k=5, p=4)"
        Write-Host "  [7] Exit"
        Write-Host "============================================================" -ForegroundColor Cyan

        $choice = Read-Host "Enter choice (1-7)"
        switch ($choice) {
            "1" { Run-Verification 5 5 3 4; pause }
            "2" { Run-Custom; pause }
            "3" { Run-Verification 10 10 3 4; pause }
            "4" { Run-Verification 1000 1000 3 4; pause }
            "5" { Run-Verification 10 10000 5 4; pause }
            "6" { Run-Verification 10000 10 5 4; pause }
            "7" { return }
            default { Write-Host "Invalid choice!" -ForegroundColor Red; Start-Sleep 1 }
        }
    }
}

Main-Menu
