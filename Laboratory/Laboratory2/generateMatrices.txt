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

Write-Host "Generating date.txt ($n x $m) and ConvolutionM.txt ($k x $k)..."
Write-RandomMatrix "date.txt" $n $m "$n $m" $maxVal
Write-RandomMatrix "ConvolutionM.txt" $k $k "$k" 10

# Copy to subdirectories so CLion and IntelliJ find the freshly generated data
foreach ($sub in @("Laboratory2_C++", "Laboratory2_C++\cmake-build-debug", "Laboratory2_Java", "Laboratory2_Java\Laboratory2_Java")) {
    if (Test-Path "$PWD\$sub") {
        Copy-Item "$PWD\date.txt" "$PWD\$sub\date.txt" -Force
        Copy-Item "$PWD\ConvolutionM.txt" "$PWD\$sub\ConvolutionM.txt" -Force
    }
}

Write-Host "Files generated successfully!" -ForegroundColor Green
