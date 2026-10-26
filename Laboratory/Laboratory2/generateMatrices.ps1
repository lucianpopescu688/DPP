param (
    [int]$n = 100,
    [int]$m = 100,
    [int]$k = 3,
    [int]$maxVal = 100,
    [int]$seed = 42
)

$rand = [System.Random]::new($seed)

function Write-RandomMatrix($path, $rows, $cols, $header, $limit) {
    $writer = [System.IO.StreamWriter]::new("$PWD\$path")
    $writer.WriteLine($header)
    for ($i = 0; $i -lt $rows; $i++) {
        $sb = [System.Text.StringBuilder]::new()
        for ($j = 0; $j -lt $cols; $j++) {
            [void]$sb.Append($rand.Next(0, $limit)).Append(" ")
        }
        $writer.WriteLine($sb.ToString().TrimEnd())
    }
    $writer.Close()
}

Write-Host "Generating date.txt ($n x $m)..."
Write-RandomMatrix "date.txt" $n $m "$n $m" $maxVal

# Copy to subdirectories so CLion and IntelliJ find the freshly generated data
if (Test-Path "$PWD\Laboratory2_C++") { Copy-Item "$PWD\date.txt" "$PWD\Laboratory2_C++\date.txt" -Force }
if (Test-Path "$PWD\Laboratory2_C++\cmake-build-debug") { Copy-Item "$PWD\date.txt" "$PWD\Laboratory2_C++\cmake-build-debug\date.txt" -Force }
if (Test-Path "$PWD\Laboratory2_Java") { Copy-Item "$PWD\date.txt" "$PWD\Laboratory2_Java\date.txt" -Force }
if (Test-Path "$PWD\Laboratory2_Java\Laboratory2_Java") { Copy-Item "$PWD\date.txt" "$PWD\Laboratory2_Java\Laboratory2_Java\date.txt" -Force }

Write-Host "Files generated successfully!" -ForegroundColor Green
