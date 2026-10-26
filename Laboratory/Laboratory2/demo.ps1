# ==============================================================================
# Interactive Lab 2 Demonstration Menu (In-Place Convolution O(M) Space)
# ==============================================================================

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
    Write-Host "   DPP Laboratory 2 - In-Place Convolution (Barrier Sync)   " -ForegroundColor White
    Write-Host "============================================================" -ForegroundColor Cyan
}

function Run-Verification($n, $m, $k, $p) {
    Write-Host "`n[1/3] Generating date.txt ($n x $m, kernel $k x $k)..." -ForegroundColor Yellow
    powershell -ExecutionPolicy Bypass -File .\generateMatrices.ps1 -n $n -m $m -k $k

    Remove-Item "output_seq.txt" -ErrorAction SilentlyContinue

    Write-Host "`n[2/3] Running Java 25 In-Place Implementation (p=$p)..." -ForegroundColor Green
    java -Xmx8g -cp "$javaDir\bin" Main all $p

    Write-Host "`n[3/3] Running C++26 Dynamic In-Place Implementation (p=$p)..." -ForegroundColor Green
    & "$cppDir\Lab2InPlace.exe" all $p

    if ($n -le 10 -and $m -le 10) {
        Write-Host "`n--- Resulting Filtered Matrix output.txt ($n x $m) ---" -ForegroundColor Yellow
        Get-Content "output.txt"
    }
}

function Main-Menu {
    while ($true) {
        Show-Header
        Write-Host "Select a demonstration option for the lab assistant:" -ForegroundColor White
        Write-Host "  [1] Small Input Demo (N = 6, M = 6, k = 3, p = 2)  <- RECOMMENDED FOR LAB" -ForegroundColor Green
        Write-Host "  [2] TC1 Official     (N = 10, M = 10, k = 3, p = 2)"
        Write-Host "  [3] TC2 Official     (N = 1000, M = 1000, k = 3, p = 4)"
        Write-Host "  [4] Custom Input     (Enter your own N, M, p)"
        Write-Host "  [5] Exit"
        Write-Host "============================================================" -ForegroundColor Cyan

        $choice = Read-Host "Enter choice (1-5)"
        switch ($choice) {
            "1" { Run-Verification 6 6 3 2; pause }
            "2" { Run-Verification 10 10 3 2; pause }
            "3" { Run-Verification 1000 1000 3 4; pause }
            "4" {
                $userN = [int](Read-Host "Enter N (rows)")
                $userM = [int](Read-Host "Enter M (columns)")
                $userP = [int](Read-Host "Enter p (threads)")
                Run-Verification $userN $userM 3 $userP
                pause
            }
            "5" { return }
            default { Write-Host "Invalid choice!" -ForegroundColor Red; Start-Sleep 1 }
        }
    }
}

Main-Menu
