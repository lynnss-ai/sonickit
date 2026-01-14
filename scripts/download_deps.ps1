# Download third-party dependencies (Windows PowerShell)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectDir = Split-Path -Parent $ScriptDir
$ThirdPartyDir = Join-Path $ProjectDir "third_party"

Write-Host "Downloading dependencies to $ThirdPartyDir"

# miniaudio
Write-Host "Downloading miniaudio..."
$MiniaudioDir = Join-Path $ThirdPartyDir "miniaudio"
New-Item -ItemType Directory -Force -Path $MiniaudioDir | Out-Null

$MiniaudioUrl = "https://raw.githubusercontent.com/mackron/miniaudio/master/miniaudio.h"
$MiniaudioPath = Join-Path $MiniaudioDir "miniaudio.h"

Invoke-WebRequest -Uri $MiniaudioUrl -OutFile $MiniaudioPath
Write-Host "  -> miniaudio.h downloaded"

Write-Host "Done!"
