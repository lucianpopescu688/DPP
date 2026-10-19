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

Write-Host "Generating MatrixF.txt ($n x $m) and ConvolutionM.txt ($k x $k)..."
Write-RandomMatrix "MatrixF.txt" $n $m "$n $m" $maxVal
Write-RandomMatrix "ConvolutionM.txt" $k $k "$k" 10
Write-Host "Files generated successfully!" -ForegroundColor Green