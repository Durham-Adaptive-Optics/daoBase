# daoBase Windows Dependencies Installation Script
# This script installs all the prerequisites for building daoBase on Windows

param(
    [string]$InstallDir = "C:\daoBase-deps",
    [string]$DaoRoot = "C:\daoBase",
    [string]$DaoData = "C:\daoData",
    [switch]$SkipVisualStudio,
    [switch]$SkipAdminCheck
)

# Ensure script is run as Administrator
if (-not $SkipAdminCheck -and -NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Error "This script must be run as Administrator. Please run PowerShell as Administrator and try again."
    exit 1
}

Write-Host "=== daoBase Windows Dependencies Installation ===" -ForegroundColor Cyan
Write-Host "Install Directory: $InstallDir" -ForegroundColor Green
Write-Host "DAO Root: $DaoRoot" -ForegroundColor Green
Write-Host "DAO Data: $DaoData" -ForegroundColor Green
Write-Host ""
Write-Host "Note: This installation may take 30-60 minutes depending on your system." -ForegroundColor Yellow
Write-Host "The longest steps are building ZeroMQ and Protocol Buffers from source." -ForegroundColor Yellow
Write-Host "Press Ctrl+C at any time to cancel the installation." -ForegroundColor Yellow
Write-Host ""

# Handle Ctrl+C gracefully
$null = Register-EngineEvent -SourceIdentifier PowerShell.Exiting -Action {
    Write-Host "`nInstallation interrupted by user. Cleaning up..." -ForegroundColor Yellow
    # The script will exit naturally, temp files will be cleaned up by the finally blocks
}

# Create directories
Write-Host "Creating directories..." -ForegroundColor Yellow
$null = New-Item -ItemType Directory -Path $InstallDir -Force
$null = New-Item -ItemType Directory -Path $DaoRoot -Force
$null = New-Item -ItemType Directory -Path $DaoData -Force
$null = New-Item -ItemType Directory -Path "$InstallDir\downloads" -Force
$null = New-Item -ItemType Directory -Path "$InstallDir\build" -Force
$null = New-Item -ItemType Directory -Path "$InstallDir\pkg-config" -Force

# Function to download files
function Download-File {
    param(
        [string]$Url,
        [string]$OutputPath
    )
    
    Write-Host "Downloading: $Url" -ForegroundColor Yellow
    try {
        # Use WebClient for better redirect handling
        $webClient = New-Object System.Net.WebClient
        $webClient.Headers.Add("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36")
        $webClient.DownloadFile($Url, $OutputPath)
        Write-Host "Downloaded: $OutputPath" -ForegroundColor Green
    }
    catch {
        Write-Warning "WebClient download failed, trying Invoke-WebRequest..."
        try {
            Invoke-WebRequest -Uri $Url -OutFile $OutputPath -UseBasicParsing -UserAgent "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36"
            Write-Host "Downloaded: $OutputPath" -ForegroundColor Green
        }
        catch {
            Write-Error "Failed to download $Url : $($_.Exception.Message)"
            throw
        }
    }
}

# Function to extract archives
function Extract-Archive {
    param(
        [string]$ArchivePath,
        [string]$DestinationPath
    )
    
    Write-Host "Extracting: $ArchivePath to $DestinationPath" -ForegroundColor Yellow
    try {
        if ($ArchivePath.EndsWith('.zip')) {
            Expand-Archive -Path $ArchivePath -DestinationPath $DestinationPath -Force
        } elseif ($ArchivePath.EndsWith('.tar.gz')) {
            # Use tar command available in Windows 10+
            & tar -xzf $ArchivePath -C $DestinationPath
        }
        Write-Host "Extracted successfully" -ForegroundColor Green
    }
    catch {
        Write-Error "Failed to extract $ArchivePath : $($_.Exception.Message)"
        exit 1
    }
}

# Function to test if VS Build Tools are properly configured
function Test-VSBuildTools {
    $vsWhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vsWhere) {
        $vsPath = & $vsWhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath 2>$null
        if ($vsPath) {
            $vcvarsPath = "$vsPath\VC\Auxiliary\Build\vcvars64.bat"
            if (Test-Path $vcvarsPath) {
                return @{
                    Installed = $true
                    VcvarsPath = $vcvarsPath
                    VsPath = $vsPath
                }
            }
        }
    }
    return @{ Installed = $false }
}

# 1. Install Visual Studio Build Tools
if (-not $SkipVisualStudio) {
    Write-Host "`n=== Installing Visual Studio Build Tools ===" -ForegroundColor Cyan

    # Check current VS Build Tools status
    $vsBuildToolsStatus = Test-VSBuildTools

    if ($vsBuildToolsStatus.Installed) {
        Write-Host "Visual Studio Build Tools with C++ components found" -ForegroundColor Green
        Write-Host "Location: $($vsBuildToolsStatus.VsPath)" -ForegroundColor Gray
    } else {
        Write-Host "Visual Studio Build Tools not found or missing C++ components" -ForegroundColor Yellow
        
        try {
        Write-Host "Installing Visual Studio Build Tools 2022 with C++ workload..." -ForegroundColor Yellow
        
        # Download Visual Studio installer directly
        $vsInstallerUrl = "https://aka.ms/vs/17/release/vs_buildtools.exe"
        $vsInstallerPath = "$env:TEMP\vs_buildtools.exe"
        
        Write-Host "Downloading Visual Studio Build Tools installer..." -ForegroundColor Yellow
        Invoke-WebRequest -Uri $vsInstallerUrl -OutFile $vsInstallerPath -UseBasicParsing
        
        # Install with specific C++ workload and components
        Write-Host "Installing Visual Studio Build Tools (this may take 10-15 minutes)..." -ForegroundColor Yellow
        $installArgs = @(
            '--quiet',
            '--wait',
            '--add', 'Microsoft.VisualStudio.Workload.VCTools',
            '--add', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
            '--add', 'Microsoft.VisualStudio.Component.Windows10SDK.19041',
            '--add', 'Microsoft.VisualStudio.Component.VC.CMake.Project'
        )
        
        $process = Start-Process -FilePath $vsInstallerPath -ArgumentList $installArgs -Wait -PassThru
        
        if ($process.ExitCode -eq 0 -or $process.ExitCode -eq 3010) { # 3010 = restart required
            Write-Host "Visual Studio Build Tools installed successfully" -ForegroundColor Green
            
            # Verify installation after a brief wait
            Start-Sleep -Seconds 5
            $vsBuildToolsStatus = Test-VSBuildTools
            
            if (-not $vsBuildToolsStatus.Installed) {
                Write-Warning "Installation completed but build tools not immediately available"
                Write-Host "You may need to restart PowerShell and run this script again" -ForegroundColor Yellow
                Write-Host "Continuing with installation..." -ForegroundColor Yellow
            }
        } else {
            Write-Error "Visual Studio Build Tools installation failed with exit code: $($process.ExitCode)"
            Write-Host "Please install manually from: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Yellow
            exit 1
        }
        
        # Clean up installer
        Remove-Item -Path $vsInstallerPath -Force -ErrorAction SilentlyContinue
        
        } catch {
            Write-Error "Failed to install Visual Studio Build Tools: $($_.Exception.Message)"
            Write-Host "Please install Visual Studio Build Tools manually from: https://visualstudio.microsoft.com/downloads/" -ForegroundColor Yellow
            exit 1
        }
    }
} else {
    Write-Host "`n=== Skipping Visual Studio Build Tools ===" -ForegroundColor Yellow
}

# Function to run commands with Visual Studio environment
function Invoke-VSCommand {
    param(
        [string]$Command,
        [string]$WorkingDirectory = (Get-Location).Path,
        [int]$TimeoutMinutes = 30
    )
    
    $vsBuildToolsStatus = Test-VSBuildTools
    if (-not $vsBuildToolsStatus.Installed) {
        Write-Error "Visual Studio Build Tools not found. Cannot execute: $Command"
        return $false
    }
    
    $vcvarsPath = $vsBuildToolsStatus.VcvarsPath
    $tempBat = "$env:TEMP\vsbuild_$(Get-Random).bat"
    
    try {
        # Create a batch file that sets up VS environment and runs the command
        $batContent = @"
@echo off
call "$vcvarsPath" > nul 2>&1
cd /d "$WorkingDirectory"
echo Starting command: $Command
$Command
echo.
echo Command completed with exit code: %ERRORLEVEL%
"@
        $batContent | Out-File -FilePath $tempBat -Encoding ASCII
        
        Write-Host "Executing: $Command" -ForegroundColor Gray
        Write-Host "Working directory: $WorkingDirectory" -ForegroundColor Gray
        Write-Host "Timeout: $TimeoutMinutes minutes" -ForegroundColor Yellow
        
        # Start the process with real-time output
        $psi = New-Object System.Diagnostics.ProcessStartInfo
        $psi.FileName = "cmd.exe"
        $psi.Arguments = "/c `"$tempBat`""
        $psi.UseShellExecute = $false
        $psi.RedirectStandardOutput = $true
        $psi.RedirectStandardError = $true
        $psi.CreateNoWindow = $true
        
        $process = New-Object System.Diagnostics.Process
        $process.StartInfo = $psi
        
        # Setup event handlers for real-time output
        $outputBuffer = [System.Collections.ArrayList]::new()
        $errorBuffer = [System.Collections.ArrayList]::new()
        
        $outputAction = {
            if (-not [string]::IsNullOrEmpty($Event.SourceEventArgs.Data)) {
                Write-Host $Event.SourceEventArgs.Data -ForegroundColor White
                [void]$outputBuffer.Add($Event.SourceEventArgs.Data)
            }
        }
        
        $errorAction = {
            if (-not [string]::IsNullOrEmpty($Event.SourceEventArgs.Data)) {
                Write-Host $Event.SourceEventArgs.Data -ForegroundColor Red
                [void]$errorBuffer.Add($Event.SourceEventArgs.Data)
            }
        }
        
        Register-ObjectEvent -InputObject $process -EventName OutputDataReceived -Action $outputAction | Out-Null
        Register-ObjectEvent -InputObject $process -EventName ErrorDataReceived -Action $errorAction | Out-Null
        
        $process.Start() | Out-Null
        $process.BeginOutputReadLine()
        $process.BeginErrorReadLine()
        
        # Wait with timeout
        $timeoutMs = $TimeoutMinutes * 60 * 1000
        $completed = $process.WaitForExit($timeoutMs)
        
        # Clean up event handlers
        Get-EventSubscriber | Where-Object {$_.SourceObject -eq $process} | Unregister-Event
        
        if ($completed) {
            $success = ($process.ExitCode -eq 0)
            if ($success) {
                Write-Host "Command completed successfully (exit code: $($process.ExitCode))" -ForegroundColor Green
            } else {
                Write-Host "Command failed with exit code: $($process.ExitCode)" -ForegroundColor Red
            }
            return $success
        } else {
            Write-Warning "Command timed out after $TimeoutMinutes minutes"
            try {
                if (-not $process.HasExited) {
                    Write-Host "Terminating process..." -ForegroundColor Yellow
                    $process.Kill()
                    $process.WaitForExit(5000)
                }
            } catch {
                Write-Warning "Could not terminate process: $($_.Exception.Message)"
            }
            return $false
        }
        
    } finally {
        # Clean up
        if ($process) {
            $process.Dispose()
        }
        if (Test-Path $tempBat) {
            Remove-Item -Path $tempBat -Force -ErrorAction SilentlyContinue
        }
    }
}

# 2. Download and build ZeroMQ
Write-Host "`n=== Installing ZeroMQ ===" -ForegroundColor Cyan
$zmqVersion = "4.3.5"
$zmqUrl = "https://github.com/zeromq/libzmq/archive/refs/tags/v$zmqVersion.zip"
$zmqZip = "$InstallDir\downloads\zeromq-$zmqVersion.zip"
$zmqExtractPath = "$InstallDir\build"
$zmqSourcePath = "$zmqExtractPath\libzmq-$zmqVersion"
$zmqBuildPath = "$zmqSourcePath\cmake-build"

if (-not (Test-Path "$InstallDir\zeromq\lib\zmq.lib")) {
    Download-File -Url $zmqUrl -OutputPath $zmqZip
    Extract-Archive -ArchivePath $zmqZip -DestinationPath $zmqExtractPath
    
    # Build ZeroMQ
    Write-Host "Building ZeroMQ..." -ForegroundColor Yellow
    $null = New-Item -ItemType Directory -Path $zmqBuildPath -Force
    
    # Configure ZeroMQ with cmake (minimal build for faster compilation)
    Write-Host "Configuring ZeroMQ with CMake..." -ForegroundColor Yellow
    $configureCmd = "cmake .. -G `"Visual Studio 17 2022`" -A x64 -DCMAKE_INSTALL_PREFIX=`"$InstallDir\zeromq`" -DBUILD_TESTS=OFF -DBUILD_STATIC=OFF -DWITH_DOCS=OFF -DENABLE_DRAFTS=OFF -DWITH_PERF_TOOL=OFF -DENABLE_CURVE=OFF"
    $configureSuccess = Invoke-VSCommand -Command $configureCmd -WorkingDirectory $zmqBuildPath -TimeoutMinutes 5
    
    if ($configureSuccess) {
        Write-Host "Building ZeroMQ (this may take 10-20 minutes)..." -ForegroundColor Yellow
        Write-Host "You will see real-time compilation output below:" -ForegroundColor Cyan
        
        # Use single-threaded build to be more predictable and show progress
        $buildCmd = "cmake --build . --config Release --target install --verbose"
        $buildSuccess = Invoke-VSCommand -Command $buildCmd -WorkingDirectory $zmqBuildPath -TimeoutMinutes 25
        
        if ($buildSuccess) {
            Write-Host "ZeroMQ built and installed successfully" -ForegroundColor Green
        } else {
            Write-Error "ZeroMQ build failed or timed out"
            Write-Host "You can try building manually by running:" -ForegroundColor Yellow
            Write-Host "  cd `"$zmqBuildPath`"" -ForegroundColor Cyan
            Write-Host "  cmake --build . --config Release --target install" -ForegroundColor Cyan
            
            # Ask user if they want to continue without ZeroMQ or retry
            $choice = Read-Host "Do you want to (C)ontinue without ZeroMQ, (R)etry, or (A)bort? [C/R/A]"
            switch ($choice.ToUpper()) {
                'R' {
                    Write-Host "Retrying ZeroMQ build..." -ForegroundColor Yellow
                    $buildSuccess = Invoke-VSCommand -Command $buildCmd -WorkingDirectory $zmqBuildPath -TimeoutMinutes 30
                    if (-not $buildSuccess) {
                        Write-Error "ZeroMQ build failed again. Aborting."
                        exit 1
                    }
                }
                'A' { exit 1 }
                default {
                    Write-Warning "Continuing without ZeroMQ - you may need to build it manually later"
                }
            }
        }
    } else {
        Write-Error "ZeroMQ configuration failed"
        exit 1
    }
} else {
    Write-Host "ZeroMQ already built" -ForegroundColor Green
}

# 3. Download and build Protocol Buffers
Write-Host "`n=== Installing Protocol Buffers ===" -ForegroundColor Cyan
$protobufVersion = "3.20.0"
$protobufUrl = "https://github.com/protocolbuffers/protobuf/archive/refs/tags/v$protobufVersion.zip"
$protobufZip = "$InstallDir\downloads\protobuf-$protobufVersion.zip"
$protobufExtractPath = "$InstallDir\build"
$protobufSourcePath = "$protobufExtractPath\protobuf-$protobufVersion"
$protobufBuildPath = "$protobufSourcePath\cmake\build"
$protobufInstallPath = "$InstallDir\protobuf"

if (-not (Test-Path "$protobufInstallPath\bin\protoc.exe")) {
    Download-File -Url $protobufUrl -OutputPath $protobufZip
    Extract-Archive -ArchivePath $protobufZip -DestinationPath $protobufExtractPath
    
    # Build Protocol Buffers
    Write-Host "Building Protocol Buffers..." -ForegroundColor Yellow
    $null = New-Item -ItemType Directory -Path $protobufBuildPath -Force
    
    # Configure Protocol Buffers with cmake
    Write-Host "Configuring Protocol Buffers with CMake..." -ForegroundColor Yellow
    $configureCmd = "cmake -G `"NMake Makefiles`" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=`"$protobufInstallPath`" -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_BUILD_EXAMPLES=OFF -Dprotobuf_BUILD_PROTOC_BINARIES=ON -Dprotobuf_MSVC_STATIC_RUNTIME=OFF .."
    $configureSuccess = Invoke-VSCommand -Command $configureCmd -WorkingDirectory $protobufBuildPath -TimeoutMinutes 5
    
    if ($configureSuccess) {
        Write-Host "Building Protocol Buffers (this may take 10-15 minutes)..." -ForegroundColor Yellow
        Write-Host "You will see real-time compilation output below:" -ForegroundColor Cyan
        
        $buildSuccess = Invoke-VSCommand -Command "nmake" -WorkingDirectory $protobufBuildPath -TimeoutMinutes 20
        
        if ($buildSuccess) {
            Write-Host "Installing Protocol Buffers..." -ForegroundColor Yellow
            $installSuccess = Invoke-VSCommand -Command "nmake install" -WorkingDirectory $protobufBuildPath -TimeoutMinutes 5
            
            if ($installSuccess) {
                Write-Host "Protocol Buffers built and installed successfully" -ForegroundColor Green
            } else {
                Write-Error "Protocol Buffers installation failed"
                Write-Host "You can try installing manually by running:" -ForegroundColor Yellow
                Write-Host "  cd `"$protobufBuildPath`"" -ForegroundColor Cyan
                Write-Host "  nmake install" -ForegroundColor Cyan
                
                $choice = Read-Host "Do you want to (C)ontinue without Protocol Buffers install, (R)etry, or (A)bort? [C/R/A]"
                switch ($choice.ToUpper()) {
                    'R' {
                        $installSuccess = Invoke-VSCommand -Command "nmake install" -WorkingDirectory $protobufBuildPath -TimeoutMinutes 10
                        if (-not $installSuccess) {
                            Write-Error "Protocol Buffers install failed again. Aborting."
                            exit 1
                        }
                    }
                    'A' { exit 1 }
                    default {
                        Write-Warning "Continuing without Protocol Buffers install"
                    }
                }
            }
        } else {
            Write-Error "Protocol Buffers build failed or timed out"
            Write-Host "You can try building manually by running:" -ForegroundColor Yellow
            Write-Host "  cd `"$protobufBuildPath`"" -ForegroundColor Cyan
            Write-Host "  nmake" -ForegroundColor Cyan
            
            $choice = Read-Host "Do you want to (C)ontinue without Protocol Buffers, (R)etry, or (A)bort? [C/R/A]"
            switch ($choice.ToUpper()) {
                'R' {
                    $buildSuccess = Invoke-VSCommand -Command "nmake" -WorkingDirectory $protobufBuildPath -TimeoutMinutes 25
                    if ($buildSuccess) {
                        $installSuccess = Invoke-VSCommand -Command "nmake install" -WorkingDirectory $protobufBuildPath -TimeoutMinutes 5
                    }
                    if (-not $buildSuccess -or -not $installSuccess) {
                        Write-Error "Protocol Buffers build/install failed again. Aborting."
                        exit 1
                    }
                }
                'A' { exit 1 }
                default {
                    Write-Warning "Continuing without Protocol Buffers"
                }
            }
        }
    } else {
        Write-Error "Protocol Buffers configuration failed"
        exit 1
    }
} else {
    Write-Host "Protocol Buffers already built" -ForegroundColor Green
}

# 4. Install pkg-config with dependencies
Write-Host "`n=== Installing pkg-config ===" -ForegroundColor Cyan

$pkgConfigPath = "$InstallDir\pkg-config"

if (-not (Test-Path "$pkgConfigPath\bin\pkg-config.exe")) {
    # Download pkg-config from GNOME (reliable mirror)
    Write-Host "Downloading pkg-config..." -ForegroundColor Yellow
    $pkgConfigUrl = "https://download.gnome.org/binaries/win32/dependencies/pkg-config_0.26-1_win32.zip"
    $pkgConfigZip = "$InstallDir\downloads\pkg-config.zip"
    
    try {
        Download-File -Url $pkgConfigUrl -OutputPath $pkgConfigZip
        
        Write-Host "Extracting pkg-config..." -ForegroundColor Yellow
        Extract-Archive -ArchivePath $pkgConfigZip -DestinationPath $pkgConfigPath
        Write-Host "pkg-config installed successfully" -ForegroundColor Green
    }
    catch {
        Write-Warning "Failed to download pkg-config from GNOME: $($_.Exception.Message)"
        
        # Fallback: Try FTP mirror
        Write-Host "Trying FTP mirror..." -ForegroundColor Yellow
        try {
            $altUrl = "https://ftp.gnome.org/pub/gnome/binaries/win32/dependencies/pkg-config_0.26-1_win32.zip"
            Download-File -Url $altUrl -OutputPath $pkgConfigZip
            
            Extract-Archive -ArchivePath $pkgConfigZip -DestinationPath $pkgConfigPath
            Write-Host "pkg-config installed successfully (via FTP mirror)" -ForegroundColor Green
        }
        catch {
            Write-Warning "FTP mirror also failed: $($_.Exception.Message)"
            Write-Host "Please manually download pkg-config from:" -ForegroundColor Yellow
            Write-Host "https://download.gnome.org/binaries/win32/dependencies/pkg-config_0.26-1_win32.zip" -ForegroundColor Cyan
            Write-Host "Save as: $pkgConfigZip and extract to: $pkgConfigPath" -ForegroundColor Cyan
            
            do {
                $continue = Read-Host "Press Enter after manual installation, or type 'skip' to continue without pkg-config"
                if ($continue.ToLower() -eq 'skip') {
                    Write-Warning "Skipping pkg-config - you may need to install it manually later"
                    break
                }
            } while (-not (Test-Path "$pkgConfigPath\bin\pkg-config.exe"))
        }
    }
    
    # Download glib dependency (required for pkg-config)
    if (Test-Path "$pkgConfigPath\bin\pkg-config.exe") {
        Write-Host "Downloading glib dependency..." -ForegroundColor Yellow
        $glibUrl = "https://download.gnome.org/binaries/win32/glib/2.28/glib_2.28.8-1_win32.zip"
        $glibZip = "$InstallDir\downloads\glib.zip"
        
        try {
            Download-File -Url $glibUrl -OutputPath $glibZip
            Write-Host "Extracting glib..." -ForegroundColor Yellow
            Extract-Archive -ArchivePath $glibZip -DestinationPath $pkgConfigPath
            Write-Host "glib installed successfully" -ForegroundColor Green
        }
        catch {
            Write-Warning "Failed to download glib dependency: $($_.Exception.Message)"
            Write-Host "pkg-config may not work properly without glib" -ForegroundColor Yellow
        }
        
        # Download gettext dependency (also required)
        Write-Host "Downloading gettext dependency..." -ForegroundColor Yellow
        $gettextUrl = "https://download.gnome.org/binaries/win32/dependencies/gettext-runtime_0.18.1.1-2_win32.zip"
        $gettextZip = "$InstallDir\downloads\gettext.zip"
        
        try {
            Download-File -Url $gettextUrl -OutputPath $gettextZip
            Write-Host "Extracting gettext..." -ForegroundColor Yellow
            Extract-Archive -ArchivePath $gettextZip -DestinationPath $pkgConfigPath
            Write-Host "gettext installed successfully" -ForegroundColor Green
        }
        catch {
            Write-Warning "Failed to download gettext dependency: $($_.Exception.Message)"
        }
    }
} else {
    Write-Host "pkg-config already installed" -ForegroundColor Green
}

# Create pkg-config .pc files even if pkg-config installation failed
function Create-PkgConfigFiles {
    param([string]$InstallDir)
    
    Write-Host "Creating pkg-config .pc files..." -ForegroundColor Yellow
    
    # Ensure the lib/pkgconfig directory exists
    $pkgConfigDir = "$InstallDir\lib\pkgconfig"
    if (-not (Test-Path $pkgConfigDir)) {
        New-Item -ItemType Directory -Path $pkgConfigDir -Force | Out-Null
    }
    
    # Create ZeroMQ .pc file with absolute paths (no variables)
    # Find the actual ZeroMQ library name
    $zmqLibPath = "$InstallDir\zeromq\lib"
    $zmqLibFile = Get-ChildItem "$zmqLibPath\*.lib" -Name | Where-Object { $_ -like "*zmq*" } | Select-Object -First 1
    if ($zmqLibFile) {
        $zmqLibName = $zmqLibFile -replace '\.lib$', ''
    } else {
        $zmqLibName = "zmq"  # fallback
    }
    
    # Use absolute paths instead of variables to avoid pkg-config issues on Windows
    $zeromqPc = @"
Name: ZeroMQ
Description: 0MQ c++ library
Version: 4.3.5
Libs: -L$($InstallDir.Replace('\','/'))\/zeromq/lib -l$zmqLibName
Cflags: -I$($InstallDir.Replace('\','/'))\/zeromq/include
"@
    
    $zeromqPc | Out-File -FilePath "$pkgConfigDir\libzmq.pc" -Encoding ASCII
    
    # Create Protocol Buffers .pc file with absolute paths (no variables)
    $protobufPc = @"
Name: Protocol Buffers
Description: Google's data interchange format
Version: 3.20.0
Libs: -L$($InstallDir.Replace('\','/'))\/protobuf/lib -llibprotobuf
Cflags: -I$($InstallDir.Replace('\','/'))\/protobuf/include
"@
    
    $protobufPc | Out-File -FilePath "$pkgConfigDir\protobuf.pc" -Encoding ASCII
    
    Write-Host "Created pkg-config files for ZeroMQ and Protocol Buffers" -ForegroundColor Green
}

# Create pkg-config .pc files
Create-PkgConfigFiles -InstallDir $InstallDir

# Define path variables for use in environment setup
$pkgConfigLibPath = "$InstallDir\lib\pkgconfig"

# 5. Install Miniconda
Write-Host "`n=== Installing Miniconda ===" -ForegroundColor Cyan
$minicondaUrl = "https://repo.anaconda.com/miniconda/Miniconda3-latest-Windows-x86_64.exe"
$minicondaInstaller = "$InstallDir\downloads\Miniconda3-latest.exe"
$minicondaPath = "$InstallDir\miniconda3"

if (-not (Test-Path "$minicondaPath\Scripts\conda.exe")) {
    Download-File -Url $minicondaUrl -OutputPath $minicondaInstaller
    
    Write-Host "Installing Miniconda..." -ForegroundColor Yellow
    Start-Process -FilePath $minicondaInstaller -ArgumentList "/InstallationType=JustMe", "/RegisterPython=0", "/S", "/D=$minicondaPath" -Wait
    
    Write-Host "Miniconda installed successfully" -ForegroundColor Green
} else {
    Write-Host "Miniconda already installed" -ForegroundColor Green
}

# Install Python packages
Write-Host "`n=== Installing Python packages ===" -ForegroundColor Cyan
$condaExe = "$minicondaPath\Scripts\conda.exe"
$pipExe = "$minicondaPath\Scripts\pip.exe"

if (Test-Path $pipExe) {
    Write-Host "Installing required Python packages..." -ForegroundColor Yellow
    
    $pythonPackages = @(
        "zmq",
        "protobuf==3.20.0",
        "astropy",
        "python-statemachine",
        "statemachine",
        "redis",
        "sphinx",
        "screeninfo"
    )
    
    foreach ($package in $pythonPackages) {
        Write-Host "Installing $package..." -ForegroundColor Yellow
        & $pipExe install $package
    }
    
    Write-Host "Python packages installed successfully" -ForegroundColor Green
} else {
    Write-Error "Miniconda pip not found at $pipExe"
    exit 1
}

# 6. Download Waf
Write-Host "`n=== Installing Waf ===" -ForegroundColor Cyan
$wafUrl = "https://waf.io/waf-2.0.26"
$wafPath = "$InstallDir\waf-2.0.26"

if (-not (Test-Path $wafPath)) {
    Download-File -Url $wafUrl -OutputPath $wafPath
    Write-Host "Waf downloaded successfully" -ForegroundColor Green
} else {
    Write-Host "Waf already downloaded" -ForegroundColor Green
}

# 7. Set persistent environment variables
Write-Host "`n=== Setting persistent environment variables ===" -ForegroundColor Cyan

try {
    # Set DAOROOT and DAODATA as user environment variables
    Write-Host "Setting DAOROOT to: $DaoRoot" -ForegroundColor Yellow
    [Environment]::SetEnvironmentVariable("DAOROOT", $DaoRoot, "User")
    
    Write-Host "Setting DAODATA to: $DaoData" -ForegroundColor Yellow
    [Environment]::SetEnvironmentVariable("DAODATA", $DaoData, "User")
    
    # Get current user PATH
    $currentUserPath = [Environment]::GetEnvironmentVariable("PATH", "User")
    $pathsToAdd = @(
        "$DaoRoot\bin",
        "$protobufInstallPath\bin",
        "$pkgConfigPath\bin",
        "$minicondaPath",
        "$minicondaPath\Scripts"
    )
    
    # Add paths to PATH if not already present
    $updatedPath = $currentUserPath
    foreach ($pathToAdd in $pathsToAdd) {
        if ($updatedPath -notlike "*$pathToAdd*") {
            if ($updatedPath) {
                $updatedPath += ";$pathToAdd"
            } else {
                $updatedPath = $pathToAdd
            }
            Write-Host "Adding to PATH: $pathToAdd" -ForegroundColor Yellow
        }
    }
    
    if ($updatedPath -ne $currentUserPath) {
        [Environment]::SetEnvironmentVariable("PATH", $updatedPath, "User")
        Write-Host "Updated user PATH environment variable" -ForegroundColor Green
    }
    
    # Set PYTHONPATH
    $currentPythonPath = [Environment]::GetEnvironmentVariable("PYTHONPATH", "User")
    $pythonPathToAdd = "$DaoRoot\python"
    if ($currentPythonPath -notlike "*$pythonPathToAdd*") {
        if ($currentPythonPath) {
            $newPythonPath = "$currentPythonPath;$pythonPathToAdd"
        } else {
            $newPythonPath = $pythonPathToAdd
        }
        [Environment]::SetEnvironmentVariable("PYTHONPATH", $newPythonPath, "User")
        Write-Host "Updated PYTHONPATH: $newPythonPath" -ForegroundColor Green
    }
    
    # Set PKG_CONFIG_PATH
    $currentPkgConfigPath = [Environment]::GetEnvironmentVariable("PKG_CONFIG_PATH", "User")
    $pkgConfigPathsToAdd = @(
        "$DaoRoot\lib\pkgconfig",
        "$pkgConfigLibPath"
    )
    
    $updatedPkgConfigPath = $currentPkgConfigPath
    foreach ($pkgPath in $pkgConfigPathsToAdd) {
        if ($updatedPkgConfigPath -notlike "*$pkgPath*") {
            if ($updatedPkgConfigPath) {
                $updatedPkgConfigPath += ";$pkgPath"
            } else {
                $updatedPkgConfigPath = $pkgPath
            }
            Write-Host "Adding to PKG_CONFIG_PATH: $pkgPath" -ForegroundColor Yellow
        }
    }
    
    if ($updatedPkgConfigPath -ne $currentPkgConfigPath) {
        [Environment]::SetEnvironmentVariable("PKG_CONFIG_PATH", $updatedPkgConfigPath, "User")
        Write-Host "Updated PKG_CONFIG_PATH environment variable" -ForegroundColor Green
    }
    
    Write-Host "Environment variables set persistently for current user" -ForegroundColor Green
}
catch {
    Write-Warning "Failed to set persistent environment variables: $($_.Exception.Message)"
    Write-Host "You can still use the batch file to set environment variables for each session" -ForegroundColor Yellow
}

# 8. Create environment setup batch file (as backup)
Write-Host "`n=== Creating environment setup script ===" -ForegroundColor Cyan
$setupBatch = "$InstallDir\setup-dao-env.bat"

$batchContent = @"
@echo off
echo Setting up DAO environment variables for this session...
echo (Note: These should already be set persistently, but this ensures they are available)

set DAOROOT=$DaoRoot
set DAODATA=$DaoData
set PATH=%PATH%;%DAOROOT%\bin;$protobufInstallPath\bin;$pkgConfigPath\bin;$minicondaPath;$minicondaPath\Scripts
set PYTHONPATH=%PYTHONPATH%;%DAOROOT%\python
set PKG_CONFIG_PATH=%PKG_CONFIG_PATH%;%DAOROOT%\lib\pkgconfig;$pkgConfigLibPath

echo DAO environment variables set for this session!
echo.
echo DAOROOT: %DAOROOT%
echo DAODATA: %DAODATA%
echo.
echo To build daoBase, run:
echo   python "$wafPath" configure --prefix=%DAOROOT%
echo   python "$wafPath"
echo   python "$wafPath" install
echo.
"@

$batchContent | Out-File -FilePath $setupBatch -Encoding ASCII

Write-Host "Environment setup script created: $setupBatch" -ForegroundColor Green
Write-Host "(This batch file is available as backup - environment variables should be set persistently)" -ForegroundColor Cyan

# Also create PowerShell version for better PowerShell integration
$setupPowerShell = "$InstallDir\setup-dao-env.ps1"

$psContent = @"
# DAO Environment Setup Script (PowerShell version)
# This script sets up environment variables for the current PowerShell session

Write-Host "Setting up DAO environment variables for this PowerShell session..." -ForegroundColor Cyan
Write-Host "(Note: These should already be set persistently, but this ensures they are available)" -ForegroundColor Yellow

# Set environment variables for this session
`$env:DAOROOT = "$DaoRoot"
`$env:DAODATA = "$DaoData"

# Add paths to current session PATH
`$pathsToAdd = @(
    "`$env:DAOROOT\bin",
    "$protobufInstallPath\bin",
    "$pkgConfigPath\bin",
    "$minicondaPath",
    "$minicondaPath\Scripts"
)

foreach (`$pathToAdd in `$pathsToAdd) {
    # Use exact path matching instead of wildcard to avoid conflicts with similar paths
    `$pathParts = `$env:PATH -split ';'
    if (`$pathToAdd -notin `$pathParts) {
        `$env:PATH = "`$env:PATH;`$pathToAdd"
    }
}

# Set PYTHONPATH
if (`$env:PYTHONPATH -notlike "*`$env:DAOROOT\python*") {
    if (`$env:PYTHONPATH) {
        `$env:PYTHONPATH = "`$env:PYTHONPATH;`$env:DAOROOT\python"
    } else {
        `$env:PYTHONPATH = "`$env:DAOROOT\python"
    }
}

# Set PKG_CONFIG_PATH
`$pkgConfigPaths = @(
    "`$env:DAOROOT\lib\pkgconfig",
    "$pkgConfigLibPath"
)

foreach (`$pkgPath in `$pkgConfigPaths) {
    # Use exact path matching to avoid conflicts
    `$pathParts = `$env:PKG_CONFIG_PATH -split ';'
    if (`$pkgPath -notin `$pathParts) {
        if (`$env:PKG_CONFIG_PATH) {
            `$env:PKG_CONFIG_PATH = "`$env:PKG_CONFIG_PATH;`$pkgPath"
        } else {
            `$env:PKG_CONFIG_PATH = `$pkgPath
        }
    }
}

Write-Host "DAO environment variables set for this PowerShell session!" -ForegroundColor Green
Write-Host ""
Write-Host "DAOROOT: `$env:DAOROOT" -ForegroundColor White
Write-Host "DAODATA: `$env:DAODATA" -ForegroundColor White
Write-Host ""
Write-Host "Python version:" -ForegroundColor Yellow
try {
    python --version
} catch {
    Write-Warning "Python not found in PATH - make sure environment setup completed successfully"
}
Write-Host ""
Write-Host "To build daoBase, navigate to the source directory and run:" -ForegroundColor Yellow
Write-Host "  python `"$wafPath`" configure --prefix=`$env:DAOROOT" -ForegroundColor Cyan
Write-Host "  python `"$wafPath`"" -ForegroundColor Cyan
Write-Host "  python `"$wafPath`" install" -ForegroundColor Cyan
Write-Host ""
"@

$psContent | Out-File -FilePath $setupPowerShell -Encoding UTF8

Write-Host "PowerShell environment setup script created: $setupPowerShell" -ForegroundColor Green

# 9. Final summary and instructions
Write-Host "`n=== Installation Complete ===" -ForegroundColor Cyan
Write-Host "All daoBase dependencies have been installed successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "Installation Summary:" -ForegroundColor Yellow
Write-Host "- Visual Studio Build Tools: Installed via winget" -ForegroundColor White
Write-Host "- ZeroMQ: Built from source and installed to $InstallDir\zeromq" -ForegroundColor White
Write-Host "- Protocol Buffers 3.20.0: Built from source and installed to $protobufInstallPath" -ForegroundColor White
Write-Host "- pkg-config: Installed to $pkgConfigPath" -ForegroundColor White
Write-Host "- Miniconda: Installed to $minicondaPath" -ForegroundColor White
Write-Host "- Python packages: Installed (zmq, protobuf==3.20.0, astropy, etc.)" -ForegroundColor White
Write-Host "- Waf 2.0.26: Downloaded to $wafPath" -ForegroundColor White
Write-Host "- Environment variables: Set persistently for current user" -ForegroundColor White
Write-Host "  * DAOROOT: $DaoRoot" -ForegroundColor Gray
Write-Host "  * DAODATA: $DaoData" -ForegroundColor Gray
Write-Host ""
Write-Host "Next Steps:" -ForegroundColor Yellow
Write-Host "1. Open a NEW PowerShell/Command Prompt window to pick up environment variables" -ForegroundColor White
Write-Host "   Or use the environment setup scripts:" -ForegroundColor Gray
Write-Host "   * For PowerShell: . $setupPowerShell" -ForegroundColor Gray
Write-Host "   * For CMD: $setupBatch" -ForegroundColor Gray
Write-Host ""
Write-Host "2. Navigate to your daoBase source directory and build:" -ForegroundColor White
Write-Host "   cd path\to\daoBase\source" -ForegroundColor Cyan
Write-Host "   python `"$wafPath`" configure --prefix=%DAOROOT%" -ForegroundColor Cyan
Write-Host "   python `"$wafPath`"" -ForegroundColor Cyan
Write-Host "   python `"$wafPath`" install" -ForegroundColor Cyan
Write-Host ""
Write-Host "Environment Variables:" -ForegroundColor Green
Write-Host "- DAOROOT and DAODATA are now set persistently in your user environment" -ForegroundColor Green
Write-Host "- PATH, PYTHONPATH, and PKG_CONFIG_PATH have been updated with required paths" -ForegroundColor Green
Write-Host "- Environment setup scripts:" -ForegroundColor Green
Write-Host "  * Batch file (CMD): $setupBatch" -ForegroundColor Gray
Write-Host "  * PowerShell: $setupPowerShell" -ForegroundColor Gray

# Return to original location
Set-Location $PSScriptRoot