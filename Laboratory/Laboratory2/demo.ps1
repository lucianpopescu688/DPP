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

    Write-Host "`n[2/3] Running Java In-Place Implementation (p=$p)..." -ForegroundColor Green
    java -Xmx8g -cp "$javaDir\bin" Main all $p

    Write-Host "`n[3/3] Running C++ Dynamic In-Place Implementation (p=$p)..." -ForegroundColor Green
    & "$cppDir\Lab2InPlace.exe" all $p

    if ($n -le 10 -and $m -le 10) {
        Write-Host "`n--- Resulting Matrix output.txt ($n x $m) ---" -ForegroundColor Yellow
        Get-Content "output.txt"
    }
}

function Main-Menu {
    while ($true) {
        Show-Header
        Write-Host "Select a test case to demonstrate in front of the assistant:" -ForegroundColor White
        Write-Host "  [1] TC1: N = M = 10, k = 3, p = 2 (Live check + prints output.txt)"
        Write-Host "  [2] TC2: N = M = 1000, k = 3, p = 4 (Medium matrix verification)"
        Write-Host "  [3] TC3: N = M = 10000, k = 3, p = 8 (Large matrix stress test)"
        Write-Host "  [4] Exit"
        Write-Host "============================================================" -ForegroundColor Cyan

        $choice = Read-Host "Enter choice (1-4)"
        switch ($choice) {
            "1" { Run-Verification 10 10 3 2; pause }
            "2" { Run-Verification 1000 1000 3 4; pause }
            "3" { Run-Verification 10000 10000 3 8; pause }
            "4" { return }
            default { Write-Host "Invalid choice!" -ForegroundColor Red; Start-Sleep 1 }
        }
    }
}

Main-Menu
