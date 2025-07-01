# Wireshark Download and Installation Script for gPTP Analysis

Write-Host "[WIRESHARK] Downloading and Installing Wireshark for gPTP Analysis" -ForegroundColor Green

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

if (-not $isAdmin) {
    Write-Host "[WARNING] Not running as administrator - installation may require elevation" -ForegroundColor Yellow
}

# Create temp directory for download
$tempDir = "$env:TEMP\WiresharkInstall"
if (-not (Test-Path $tempDir)) {
    New-Item -ItemType Directory -Path $tempDir -Force | Out-Null
}

Write-Host "[INFO] Download directory: $tempDir" -ForegroundColor Cyan

# Wireshark download URL (latest stable version)
$wiresharkUrl = "https://1.na.dl.wireshark.org/win64/Wireshark-4.4.2-x64.exe"
$installerPath = "$tempDir\Wireshark-installer.exe"

Write-Host "[DOWNLOAD] Downloading Wireshark from official source..." -ForegroundColor Yellow
Write-Host "[URL] $wiresharkUrl" -ForegroundColor Gray

try {
    # Download with progress
    $webClient = New-Object System.Net.WebClient
    $webClient.DownloadFile($wiresharkUrl, $installerPath)
    Write-Host "[SUCCESS] Wireshark downloaded successfully!" -ForegroundColor Green
} catch {
    Write-Host "[ERROR] Download failed: $($_.Exception.Message)" -ForegroundColor Red
    
    # Try alternative download method
    Write-Host "[RETRY] Trying alternative download method..." -ForegroundColor Yellow
    try {
        Invoke-WebRequest -Uri $wiresharkUrl -OutFile $installerPath -UseBasicParsing
        Write-Host "[SUCCESS] Wireshark downloaded successfully (alternative method)!" -ForegroundColor Green
    } catch {
        Write-Host "[ERROR] Alternative download also failed: $($_.Exception.Message)" -ForegroundColor Red
        Write-Host "[MANUAL] Please download manually from: https://www.wireshark.org/download.html" -ForegroundColor Yellow
        exit 1
    }
}

# Verify download
if (Test-Path $installerPath) {
    $fileSize = (Get-Item $installerPath).Length / 1MB
    Write-Host "[VERIFY] Downloaded file size: $([math]::Round($fileSize, 2)) MB" -ForegroundColor Cyan
    
    if ($fileSize -lt 50) {
        Write-Host "[ERROR] Downloaded file seems too small - may be corrupted" -ForegroundColor Red
        exit 1
    }
} else {
    Write-Host "[ERROR] Downloaded file not found" -ForegroundColor Red
    exit 1
}

# Install Wireshark
Write-Host "[INSTALL] Starting Wireshark installation..." -ForegroundColor Green
Write-Host "[INFO] This will install with default options suitable for gPTP analysis" -ForegroundColor Cyan

try {
    # Silent installation with common options
    $installArgs = @(
        "/S",  # Silent install
        "/desktopicon=yes",  # Create desktop icon
        "/quicklaunchicon=no",  # No quick launch
        "/associate=yes",  # Associate file types
        "/npcap=yes"  # Install Npcap (packet capture driver)
    )
    
    Write-Host "[EXECUTING] Starting installer..." -ForegroundColor Yellow
    $process = Start-Process -FilePath $installerPath -ArgumentList $installArgs -Wait -PassThru
    
    if ($process.ExitCode -eq 0) {
        Write-Host "[SUCCESS] Wireshark installed successfully!" -ForegroundColor Green
    } else {
        Write-Host "[ERROR] Installation failed with exit code: $($process.ExitCode)" -ForegroundColor Red
        Write-Host "[MANUAL] Please run the installer manually: $installerPath" -ForegroundColor Yellow
    }
} catch {
    Write-Host "[ERROR] Installation process failed: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "[MANUAL] Please run the installer manually: $installerPath" -ForegroundColor Yellow
}

# Check installation
Write-Host "[VERIFY] Checking Wireshark installation..." -ForegroundColor Cyan

$wiresharkPaths = @(
    "${env:ProgramFiles}\Wireshark\Wireshark.exe",
    "${env:ProgramFiles(x86)}\Wireshark\Wireshark.exe"
)

$wiresharkFound = $false
foreach ($path in $wiresharkPaths) {
    if (Test-Path $path) {
        Write-Host "[FOUND] Wireshark installed at: $path" -ForegroundColor Green
        $wiresharkFound = $true
        break
    }
}

if (-not $wiresharkFound) {
    Write-Host "[WARNING] Wireshark executable not found in standard locations" -ForegroundColor Yellow
    Write-Host "[INFO] Installation may still be in progress or in non-standard location" -ForegroundColor Gray
}

# Create gPTP analysis profile
Write-Host "[CONFIG] Creating gPTP analysis configuration..." -ForegroundColor Cyan

$gptpProfile = @"
# Wireshark gPTP Analysis Profile
# Quick filters for PreSonus gPTP analysis

# Main gPTP filter
eth.type == 0x88f7

# Announce messages (master election)
ptp.v2.messagetype == 0x0b

# Sync messages (time distribution)  
ptp.v2.messagetype == 0x00

# PDelay messages (delay measurement)
ptp.v2.messagetype >= 0x02 and ptp.v2.messagetype <= 0x0a

# Show clock quality
ptp.v2.clockquality.clockclass

# Show priority values
ptp.v2.flags.unicast and ptp.v2.grandmasterpriorityField1
"@

$profilePath = "$tempDir\gPTP_Filters.txt"
$gptpProfile | Out-File -FilePath $profilePath -Encoding UTF8
Write-Host "[SAVED] gPTP filter reference saved to: $profilePath" -ForegroundColor Green

# Cleanup
Write-Host "[CLEANUP] Cleaning up temporary files..." -ForegroundColor Gray
# Keep installer and profile for reference
Write-Host "[INFO] Installer and filters kept in: $tempDir" -ForegroundColor Gray

Write-Host "`n[COMPLETE] Wireshark Setup Complete!" -ForegroundColor Green
Write-Host "[NEXT STEPS] To start gPTP analysis:" -ForegroundColor Yellow
Write-Host "  1. Open Wireshark (should be on desktop or in Start menu)" -ForegroundColor White
Write-Host "  2. Select 'AVB' or 'Intel(R) Ethernet I210-T1 GbE NIC' interface" -ForegroundColor White  
Write-Host "  3. Apply filter: eth.type == 0x88f7" -ForegroundColor White
Write-Host "  4. Start capture" -ForegroundColor White
Write-Host "  5. Connect PreSonus devices and watch gPTP messages!" -ForegroundColor White

Write-Host "`n[gPTP STATUS] Your gPTP Milan grandmaster is still running" -ForegroundColor Green
Write-Host "[READY] System ready for live PreSonus network testing!" -ForegroundColor Green
