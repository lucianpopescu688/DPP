param (
    [int]$n = 100,
    [int]$m = 100,
    [int]$k = 3,
    [int]$maxVal = 100
)

$rand = [System.Random]::new()

Write-Host "Generating MatrixF.txt ($n x $m)..."
$fWriter = [System.IO.StreamWriter]::new("MatrixF.txt")
$fWriter.WriteLine("$n $m")

for ($i = 0; $i -lt $n; $i++) {
    $sb = [System.Text.StringBuilder]::new()
    for ($j = 0; $j -lt $m; $j++) {
        [void]$sb.Append($rand.Next(0, $maxVal)).Append(" ")
    }
    $fWriter.WriteLine($sb.ToString().TrimEnd())
}
$fWriter.Close()

Write-Host "Generating ConvolutionM.txt ($k x $k)..."
$cWriter = [System.IO.StreamWriter]::new("ConvolutionM.txt")
$cWriter.WriteLine("$k")

for ($i = 0; $i -lt $k; $i++) {
    $sb = [System.Text.StringBuilder]::new()
    for ($j = 0; $j -lt $k; $j++) {
        [void]$sb.Append($rand.Next(0, 10)).Append(" ")
    }
    $cWriter.WriteLine($sb.ToString().TrimEnd())
}
$cWriter.Close()

Write-Host "Files generated successfully!" -ForegroundColor Green