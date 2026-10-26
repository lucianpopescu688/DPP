# ==============================================================================
# Interactive Lab 2 Demonstration Menu
# ==============================================================================

# Ensure JDK and CLion MinGW are in PATH if available
$jdk = "C:\Program Files\JetBrains\IntelliJ IDEA 2026.2.3\jbr\bin"
$mingw = "C:\Program Files\JetBrains\CLion 2026.2.3.1\bin\mingw\bin"
if (Test-Path $jdk)   { $env:PATH = "$jdk;$env:PATH" }
if (Test-Path $mingw) { $env:PATH = "$mingw;$env:PATH" }

$javaDir = "$PWD\Laboratory2_Java\Laboratory2_Java"
if (-not (Test-Path "$javaDir\src\Main.java")) { $javaDir = "$PWD\Laboratory2_Java" }
$cppDir  = "$PWD\Laboratory2_C++"

# Compile Java and C++ if binaries are missing
if (-not (Test-Path "$cppDir\Lab2InPlace.exe")) {
    $gpp = if (Test-Path "$mingw\g++.exe") { "$mingw\g++.exe" } else { "g++" }
    & $gpp -O3 -std=c++2c -pthread "$cppDir\main.cpp" -o "$cppDir\Lab2InPlace.exe"
}
if (-not (Test-Path "$javaDir\bin\Main.class")) {
    New-Item -ItemType Directory -Path "$javaDir\bin" -Force | Out-Null
    cmd /c "javac -d `"$javaDir\bin`" `"$javaDir\src\Main.java`""
}

function Show-Header {
    Clear-Host
    Write-Host "============================================================" -ForegroundColor Cyan
    Write-Host "   DPP Laboratory 2 - Interactive Lab Demonstration Menu    " -ForegroundColor White
    Write-Host "============================================================" -ForegroundColor Cyan
}

function Run-Verification($n, $m, $k, $p) {
    Write-Host "`n[1/3] Generating date.txt ($n x $m, kernel $k x $k)..." -ForegroundColor Yellow
    powershell -ExecutionPolicy Bypass -File .\generateMatrices.ps1 -n $n -m $m -k $k

    Remove-Item "output_seq.txt" -ErrorAction SilentlyContinue

    Write-Host "`n[2/3] Running Java Implementation (p=$p)..." -ForegroundColor Green
    java -Xmx8g -cp "$javaDir\bin" Main all $p

    Write-Host "`n[3/3] Running C++ Implementation (p=$p)..." -ForegroundColor Green
    & "$cppDir\Lab2InPlace.exe" all $p

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

    $p = Read-Host "Enter number of threads p [default 2]"
    if ([string]::IsNullOrWhiteSpace($p)) { $p = 2 } else { $p = [int]$p }

    Run-Verification $n $m 3 $p
}

function Main-Menu {
    while ($true) {
        Show-Header
        Write-Host "Select an option to demonstrate:" -ForegroundColor White
        Write-Host "  [1] Quick Demo: Small 5 x 5 Matrix (Kernel 3x3, p=2) - Shows all + output"
        Write-Host "  [2] Custom Input: Enter your own n, m, p in terminal"
        Write-Host "  [3] Run Official Test Case TC1 (10 x 10, k=3, p=2)"
        Write-Host "  [4] Run Official Test Case TC2 (1000 x 1000, k=3, p=4)"
        Write-Host "  [5] Run Official Test Case TC3 (10000 x 10000, k=3, p=8)"
        Write-Host "  [6] Exit"
        Write-Host "============================================================" -ForegroundColor Cyan

        $choice = Read-Host "Enter choice (1-6)"
        switch ($choice) {
            "1" { Run-Verification 5 5 3 2; pause }
            "2" { Run-Custom; pause }
            "3" { Run-Verification 10 10 3 2; pause }
            "4" { Run-Verification 1000 1000 3 4; pause }
            "5" { Run-Verification 10000 10000 3 8; pause }
            "6" { return }
            default { Write-Host "Invalid choice!" -ForegroundColor Red; Start-Sleep 1 }
        }
    }
}

Main-Menu
