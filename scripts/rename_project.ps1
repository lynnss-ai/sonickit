# Rename VOICE_ to SONICKIT_ in CMakeLists.txt
$file = 'E:\code\voice\CMakeLists.txt'
$content = Get-Content -Path $file -Raw -Encoding UTF8
$content = $content -replace 'VOICE_','SONICKIT_' -replace 'Voice Library','SonicKit'
Set-Content -Path $file -Value $content -Encoding UTF8 -NoNewline
Write-Host 'Updated CMakeLists.txt: replaced VOICE_ with SONICKIT_'
