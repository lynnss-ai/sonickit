# Flatten nested single-child directories repeatedly for given paths
$paths = @('E:\code\voice\build_minimal','E:\code\voice\build_standalone')
Write-Host "Repairing nested directories for: $($paths -join ', ')"
foreach ($p in $paths) {
    if (-not (Test-Path $p)) { Write-Host "Path not found: $p"; continue }
    while ($true) {
        $children = Get-ChildItem -Path $p -Force -ErrorAction Stop
        if ($children.Count -eq 1 -and $children[0].PSIsContainer) {
            $inner = $children[0].FullName
            Write-Host ("Flattening {0} <- {1}" -f $p, $inner)
            Get-ChildItem -Force $inner | ForEach-Object {
                $dest = Join-Path $p $_.Name
                if (Test-Path $dest) { Write-Host ("Removing existing: {0}" -f $dest); Remove-Item -Recurse -Force $dest }
                Move-Item -Force $_.FullName $p
            }
            Remove-Item -Recurse -Force $inner
        } else { break }
    }
    Write-Host ("\nFinal contents of {0}:" -f $p)
    Get-ChildItem -Path $p -Force | Format-Table Name,Mode,LastWriteTime -AutoSize
}
