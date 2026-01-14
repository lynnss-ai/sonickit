# ============================================
# Add author information to all code files
# Author: wangxuebing <lynnss.codeai@gmail.com>
# ============================================

$ErrorActionPreference = "Stop"
$author = "wangxuebing <lynnss.codeai@gmail.com>"
$modified = 0
$skipped = 0
$errors = 0

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "SonicKit Source Code Author Information Tool" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Author: $author" -ForegroundColor Green
Write-Host ""

# Process a single file
function Add-AuthorToFile {
    param([string]$FilePath)
    
    try {
        $content = Get-Content $FilePath -Raw -Encoding UTF8
        
        # Check if @author tag already exists
        if ($content -match "@author") {
            Write-Host "  [Skip] Author exists: $(Split-Path $FilePath -Leaf)" -ForegroundColor Yellow
            return $false
        }
        
        # Find @brief line and add @author after it
        if ($content -match "(\*\s*@brief[^\r\n]*[\r\n]+)") {
            $newContent = $content -replace "(\*\s*@brief[^\r\n]*[\r\n]+)", "`$1 * @author $author`r`n"
            Set-Content -Path $FilePath -Value $newContent -NoNewline -Encoding UTF8
            Write-Host "  [Done] $(Split-Path $FilePath -Leaf)" -ForegroundColor Green
            return $true
        } else {
            Write-Host "  [Warn] No @brief tag found: $(Split-Path $FilePath -Leaf)" -ForegroundColor DarkYellow
            return $false
        }
    } catch {
        Write-Host "  [Error] Failed: $(Split-Path $FilePath -Leaf) - $($_.Exception.Message)" -ForegroundColor Red
        return $null
    }
}

# Process directory
function Process-Directory {
    param(
        [string]$Path,
        [string[]]$Extensions
    )
    
    if (-not (Test-Path $Path)) {
        Write-Host "Directory not found, skip: $Path" -ForegroundColor DarkYellow
        return
    }
    
    Write-Host "Processing directory: $Path" -ForegroundColor Cyan
    
    $files = Get-ChildItem -Path $Path -Recurse -Include $Extensions -File
    Write-Host "Found $($files.Count) files" -ForegroundColor Cyan
    Write-Host ""
    
    foreach ($file in $files) {
        $result = Add-AuthorToFile -FilePath $file.FullName
        
        if ($result -eq $true) {
            $script:modified++
        } elseif ($result -eq $false) {
            $script:skipped++
        } else {
            $script:errors++
        }
    }
    
    Write-Host ""
}

# Backup reminder
Write-Host "Warning: This script will modify source code files." -ForegroundColor Yellow
Write-Host "It's recommended to commit current changes to git first." -ForegroundColor Yellow
Write-Host ""
$confirm = Read-Host "Continue? (type yes to proceed)"

if ($confirm -ne "yes") {
    Write-Host "Operation cancelled." -ForegroundColor Yellow
    exit 0
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Processing files..." -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

# Process directories
Process-Directory -Path "src" -Extensions @("*.c", "*.h")
Process-Directory -Path "include" -Extensions @("*.h")
Process-Directory -Path "wasm" -Extensions @("*.c", "*.cpp", "*.h")
Process-Directory -Path "tests" -Extensions @("*.c", "*.h")
Process-Directory -Path "examples" -Extensions @("*.c")

# Show statistics
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Processing complete!" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "Modified: $modified files" -ForegroundColor Green
Write-Host "Skipped: $skipped files" -ForegroundColor Yellow
Write-Host "Errors: $errors files" -ForegroundColor Red
Write-Host ""

if ($modified -gt 0) {
    Write-Host "Please review changes:" -ForegroundColor Green
    Write-Host "  git diff" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Commit if everything looks good:" -ForegroundColor Green
    Write-Host "  git add ." -ForegroundColor Cyan
    Write-Host "  git commit -m `"docs: add author information to all source files`"" -ForegroundColor Cyan
    Write-Host "  git push" -ForegroundColor Cyan
} else {
    Write-Host "No files were modified." -ForegroundColor Yellow
}

Write-Host ""
