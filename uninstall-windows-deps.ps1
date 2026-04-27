# daoBase Windows Dependencies Uninstall Script
# This script removes all dependencies and configurations installed by install-windows-deps.ps1

param(
    [string]$InstallDir = "C:\daoBase-deps",
    [string]$DaoRoot = "C:\daoBase",
    [string]$DaoData = "C:\daoData",
    [switch]$Force = $false,
    [switch]$KeepMiniconda = $false
)

# Ensure script is run as Administrator for some operations
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

Write-Host "=== daoBase Windows Dependencies Uninstall ===" -ForegroundColor Red
Write-Host "This script will remove all daoBase dependencies and configurations." -ForegroundColor Yellow
Write-Host ""
Write-Host "Directories to remove:" -ForegroundColor Yellow
Write-Host "- Install Directory: $InstallDir" -ForegroundColor White
Write-Host "- DAO Root: $DaoRoot" -ForegroundColor White
Write-Host "- DAO Data: $DaoData" -ForegroundColor White
Write-Host ""
Write-Host "Environment variables to clean:" -ForegroundColor Yellow
Write-Host "- DAOROOT, DAODATA" -ForegroundColor White
Write-Host "- PATH (DAO-related entries)" -ForegroundColor White
Write-Host "- PYTHONPATH (DAO-related entries)" -ForegroundColor White
Write-Host "- PKG_CONFIG_PATH (DAO-related entries)" -ForegroundColor White
Write-Host ""

if (-not $Force) {
    $confirmation = Read-Host "Are you sure you want to proceed? Type 'YES' to continue"
    if ($confirmation -ne 'YES') {
        Write-Host "Uninstall cancelled." -ForegroundColor Green
        exit 0
    }
}

# Function to safely remove directory
function Remove-DirectorySafely {
    param(
        [string]$Path,
        [string]$Description
    )
    
    if (Test-Path $Path) {
        Write-Host "Removing $Description at: $Path" -ForegroundColor Yellow
        try {
            Remove-Item -Path $Path -Recurse -Force
            Write-Host "Successfully removed $Description" -ForegroundColor Green
        }
        catch {
            Write-Warning "Failed to remove $Description at $Path : $($_.Exception.Message)"
            Write-Host "You may need to remove this manually or restart and try again" -ForegroundColor Yellow
        }
    } else {
        Write-Host "$Description not found at: $Path" -ForegroundColor Gray
    }
}

# Function to clean environment variable paths
function Clean-EnvironmentPath {
    param(
        [string]$VariableName,
        [string[]]$PathsToRemove,
        [string]$Target = "User"
    )
    
    $currentValue = [Environment]::GetEnvironmentVariable($VariableName, $Target)
    if ($currentValue) {
        $paths = $currentValue -split ';' | Where-Object { $_ -ne '' }
        $cleanedPaths = @()
        
        foreach ($path in $paths) {
            $shouldKeep = $true
            foreach ($pathToRemove in $PathsToRemove) {
                if ($path -like "*$pathToRemove*" -or $path -eq $pathToRemove) {
                    $shouldKeep = $false
                    Write-Host "Removing from $VariableName : $path" -ForegroundColor Yellow
                    break
                }
            }
            if ($shouldKeep) {
                $cleanedPaths += $path
            }
        }
        
        if ($cleanedPaths.Count -gt 0) {
            $newValue = $cleanedPaths -join ';'
            [Environment]::SetEnvironmentVariable($VariableName, $newValue, $Target)
            Write-Host "Updated $VariableName environment variable" -ForegroundColor Green
        } else {
            [Environment]::SetEnvironmentVariable($VariableName, $null, $Target)
            Write-Host "Cleared $VariableName environment variable" -ForegroundColor Green
        }
    } else {
        Write-Host "$VariableName environment variable not set" -ForegroundColor Gray
    }
}

# 1. Remove environment variables
Write-Host "`n=== Cleaning Environment Variables ===" -ForegroundColor Cyan

try {
    # Remove DAOROOT and DAODATA
    Write-Host "Removing DAOROOT environment variable..." -ForegroundColor Yellow
    [Environment]::SetEnvironmentVariable("DAOROOT", $null, "User")
    
    Write-Host "Removing DAODATA environment variable..." -ForegroundColor Yellow
    [Environment]::SetEnvironmentVariable("DAODATA", $null, "User")
    
    # Clean PATH
    $pathsToRemove = @(
        "$DaoRoot\bin",
        "$InstallDir\protobuf\bin",
        "$InstallDir\pkg-config-lite\bin",
        "$InstallDir\miniconda3\Scripts"
    )
    Clean-EnvironmentPath -VariableName "PATH" -PathsToRemove $pathsToRemove
    
    # Clean PYTHONPATH
    $pythonPathsToRemove = @("$DaoRoot\python")
    Clean-EnvironmentPath -VariableName "PYTHONPATH" -PathsToRemove $pythonPathsToRemove
    
    # Clean PKG_CONFIG_PATH
    $pkgConfigPathsToRemove = @(
        "$DaoRoot\lib\pkgconfig",
        "$InstallDir\pkg-config\lib"
    )
    Clean-EnvironmentPath -VariableName "PKG_CONFIG_PATH" -PathsToRemove $pkgConfigPathsToRemove
    
    Write-Host "Environment variables cleaned successfully" -ForegroundColor Green
}
catch {
    Write-Warning "Failed to clean some environment variables: $($_.Exception.Message)"
}

# 2. Remove installed software (requires admin)
Write-Host "`n=== Removing Installed Software ===" -ForegroundColor Cyan

if ($isAdmin) {
    # Check if Visual Studio Build Tools was installed
    try {
        # Use the same detection method as install script
        $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
        $vsFound = $false
        
        if (Test-Path $vsWhere) {
            $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
            if ($vsPath) {
                $vsFound = $true
                Write-Host "Found Visual Studio Build Tools at: $vsPath" -ForegroundColor Yellow
                $removeVS = Read-Host "Do you want to uninstall Visual Studio Build Tools? (y/N)"
                if ($removeVS -eq 'y' -or $removeVS -eq 'Y') {
                    Write-Host "Uninstalling Visual Studio Build Tools..." -ForegroundColor Yellow
                    
                    # Try to use Visual Studio installer for proper uninstallation
                    $vsInstallerPath = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vs_installer.exe"
                    if (Test-Path $vsInstallerPath) {
                        $uninstallArgs = @('uninstall', '--installPath', $vsPath, '--quiet', '--wait')
                        $process = Start-Process -FilePath $vsInstallerPath -ArgumentList $uninstallArgs -Wait -PassThru
                        
                        if ($process.ExitCode -eq 0) {
                            Write-Host "Visual Studio Build Tools uninstalled successfully" -ForegroundColor Green
                        } else {
                            Write-Warning "Visual Studio uninstall may not have completed successfully (exit code: $($process.ExitCode))"
                        }
                    } else {
                        # Fallback to winget
                        winget uninstall Microsoft.VisualStudio.2022.BuildTools --silent --accept-source-agreements
                        Write-Host "Visual Studio Build Tools uninstalled via winget" -ForegroundColor Green
                    }
                } else {
                    Write-Host "Skipping Visual Studio Build Tools removal" -ForegroundColor Gray
                }
            }
        }
        
        if (-not $vsFound) {
            Write-Host "Visual Studio Build Tools not found or not properly installed" -ForegroundColor Gray
        }
    }
    catch {
        Write-Warning "Could not check/remove Visual Studio Build Tools: $($_.Exception.Message)"
    }
} else {
    Write-Warning "Not running as Administrator - cannot uninstall Visual Studio Build Tools"
    Write-Host "To remove Visual Studio Build Tools, run as Administrator or manually uninstall via:" -ForegroundColor Yellow
    Write-Host "  winget uninstall Microsoft.VisualStudio.2022.BuildTools" -ForegroundColor Cyan
}

# 3. Remove directories
Write-Host "`n=== Removing Directories ===" -ForegroundColor Cyan

# Remove main installation directory
Remove-DirectorySafely -Path $InstallDir -Description "Dependencies installation directory"

# Remove DAO directories
Remove-DirectorySafely -Path $DaoRoot -Description "DAO Root directory"
Remove-DirectorySafely -Path $DaoData -Description "DAO Data directory"

# 4. Handle Miniconda separately
Write-Host "`n=== Miniconda Handling ===" -ForegroundColor Cyan

if (-not $KeepMiniconda) {
    $minicondaPath = "$InstallDir\miniconda3"
    if (Test-Path $minicondaPath) {
        Write-Host "Miniconda was installed to: $minicondaPath" -ForegroundColor Yellow
        Write-Host "This has been removed with the main installation directory." -ForegroundColor Green
    }
    
    # Check for system-wide Miniconda installations
    $possibleMinicondaPaths = @(
        "$env:USERPROFILE\miniconda3",
        "$env:USERPROFILE\AppData\Local\miniconda3",
        "C:\ProgramData\miniconda3"
    )
    
    foreach ($path in $possibleMinicondaPaths) {
        if (Test-Path $path) {
            Write-Host "Found system Miniconda at: $path" -ForegroundColor Yellow
            $removeMini = Read-Host "Do you want to remove this Miniconda installation? (y/N)"
            if ($removeMini -eq 'y' -or $removeMini -eq 'Y') {
                Remove-DirectorySafely -Path $path -Description "Miniconda installation"
            }
        }
    }
} else {
    Write-Host "Keeping Miniconda as requested (--KeepMiniconda flag used)" -ForegroundColor Green
}

# 5. Clean up registry entries (if needed)
Write-Host "`n=== Registry Cleanup ===" -ForegroundColor Cyan
Write-Host "Note: Some applications may have left registry entries." -ForegroundColor Yellow
Write-Host "These are generally harmless but can be cleaned manually if desired." -ForegroundColor Yellow

# 6. Final cleanup verification
Write-Host "`n=== Cleanup Verification ===" -ForegroundColor Cyan

$remainingItems = @()

if (Test-Path $InstallDir) { $remainingItems += "Install directory: $InstallDir" }
if (Test-Path $DaoRoot) { $remainingItems += "DAO Root: $DaoRoot" }
if (Test-Path $DaoData) { $remainingItems += "DAO Data: $DaoData" }

$daoRootEnv = [Environment]::GetEnvironmentVariable("DAOROOT", "User")
$daoDataEnv = [Environment]::GetEnvironmentVariable("DAODATA", "User")

if ($daoRootEnv) { $remainingItems += "DAOROOT environment variable" }
if ($daoDataEnv) { $remainingItems += "DAODATA environment variable" }

if ($remainingItems.Count -gt 0) {
    Write-Host "Some items could not be completely removed:" -ForegroundColor Yellow
    foreach ($item in $remainingItems) {
        Write-Host "  - $item" -ForegroundColor Red
    }
    Write-Host "You may need to:" -ForegroundColor Yellow
    Write-Host "  1. Restart Windows and try running this script again" -ForegroundColor White
    Write-Host "  2. Manually remove remaining directories" -ForegroundColor White
    Write-Host "  3. Check for processes using these directories" -ForegroundColor White
} else {
    Write-Host "All daoBase dependencies have been successfully removed!" -ForegroundColor Green
}

# 7. Final instructions
Write-Host "`n=== Post-Uninstall Instructions ===" -ForegroundColor Cyan
Write-Host "1. Restart any open terminals to clear environment variables" -ForegroundColor White
Write-Host "2. If you have daoBase source code elsewhere, it has not been touched" -ForegroundColor White
Write-Host "3. Any manual configurations outside the standard installation are preserved" -ForegroundColor White
Write-Host ""
Write-Host "Uninstall process completed!" -ForegroundColor Green

# Return to original location
Set-Location $PSScriptRoot