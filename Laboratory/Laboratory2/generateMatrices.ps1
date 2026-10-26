param (
    [int]$n = 1000,
    [int]$m = 1000,
    [int]$k = 3,
    [int]$maxVal = 100,
    [int]$seed = 42
)

$rand = [System.Random]::new($seed)

Write-Host "Generating date.txt ($n x $m matrix + $k x $k kernel)..."
$writer = [System.IO.StreamWriter]::new("$PWD\date.txt")
$writer.WriteLine("$n $m")
for ($i = 0; $i -lt $n; $i++) {
    $sb = [System.Text.StringBuilder]::new()
    for ($j = 0; $j -lt $m; $j++) {
        [void]$sb.Append($rand.Next(0, $maxVal)).Append(" ")
    }
    $writer.WriteLine($sb.ToString().TrimEnd())
}

# Write the k x k kernel at the end of date.txt (and also to ConvolutionM.txt for compatibility)
$kWriter = [System.IO.StreamWriter]::new("$PWD\ConvolutionM.txt")
$writer.WriteLine("$k $k")
$kWriter.WriteLine("$k")
for ($i = 0; $i -lt $k; $i++) {
    $sb = [System.Text.StringBuilder]::new()
    for ($j = 0; $j -lt $k; $j++) {
        [void]$sb.Append($rand.Next(0, 10)).Append(" ")
    }
    $line = $sb.ToString().TrimEnd()
    $writer.WriteLine($line)
    $kWriter.WriteLine($line)
}
$writer.Close()
$kWriter.Close()
Write-Host "date.txt generated successfully!" -ForegroundColor Green
