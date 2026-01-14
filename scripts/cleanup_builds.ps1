<#
Safe cleanup script for build directories.

Usage examples:
  # 仅备份（默认，不删除）
  .\scripts\cleanup_builds.ps1

  # 备份并删除指定目录
  .\scripts\cleanup_builds.ps1 -Delete

  # 仅处理特定目录
  .\scripts\cleanup_builds.ps1 -Targets build_new,build_new,build_minimal -Delete
#>
param(
    [switch]$Delete,
    [string]$RepoRoot = (Get-Location).Path,
    [string]$BackupDir = "build_backups",
    [string[]]$Targets = @("build_new","build_minimal","build_standalone")
)

function TimeStamp { Get-Date -Format "yyyyMMdd_HHmmss" }

$BackupRoot = Join-Path -Path $RepoRoot -ChildPath $BackupDir
if (-not (Test-Path $BackupRoot)) { New-Item -Path $BackupRoot -ItemType Directory | Out-Null }

$results = @()
foreach ($t in $Targets) {
    $full = Join-Path -Path $RepoRoot -ChildPath $t
    if (Test-Path $full) {
        $ts = TimeStamp
        $zipName = "${t.Replace('\', '_')}_$ts.zip"
        $zipPath = Join-Path -Path $BackupRoot -ChildPath $zipName
        Write-Host "Backing up: $full -> $zipPath"
        try {
            Compress-Archive -Path $full -DestinationPath $zipPath -Force -ErrorAction Stop
            $backed = $true
        } catch {
            Write-Warning "Failed to compress $full : $_"
            $backed = $false
        }
        if ($Delete) {
            if ($backed) {
                Write-Host "Removing: $full"
                try { Remove-Item -Recurse -Force -Path $full -ErrorAction Stop; $removed = $true } catch { Write-Warning "Failed to remove $full : $_"; $removed = $false }
            } else {
                Write-Warning "Skipping removal because backup failed for $full"
                $removed = $false
            }
        } else { $removed = $false }
        $results += [pscustomobject]@{Target=$t;Path=$full;BackedUp=$backed;Zip=$zipPath;Removed=$removed}
    } else {
        Write-Host "Not found: $full"
        $results += [pscustomobject]@{Target=$t;Path=$full;BackedUp=$false;Zip=$null;Removed=$false}
    }
}

Write-Host "\nSummary:"
$results | Format-Table -AutoSize

Write-Host "\nBackup location: $BackupRoot"
if (-not $Delete) { Write-Host "Note: No deletions performed. Rerun with -Delete to remove after backup." }
