# Run gptp with Milan profile as Administrator (Stable Mode - No Enhanced Debugging)
param()

# Get the script directory to ensure we work from the correct location
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Check if already running as Administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

if (-not $isAdmin) {
    Write-Host "Requesting Administrator privileges..." -ForegroundColor Yellow
    # Re-launch this script as Administrator with working directory preserved
    $scriptPath = $MyInvocation.MyCommand.Path
    Start-Process PowerShell -ArgumentList "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", "`"$scriptPath`"" -Verb RunAs -WorkingDirectory $scriptDir
    exit
}

# Ensure we're in the correct working directory
Set-Location $scriptDir
Write-Host "Running as Administrator from directory: $(Get-Location)" -ForegroundColor Green
Write-Host "Detecting network adapter..." -ForegroundColor Green
$mac = (Get-NetAdapter | Where-Object { $_.InterfaceDescription -like '*I21*' -or $_.InterfaceDescription -like '*I219*' } | Select-Object -First 1 -ExpandProperty MacAddress)

if (-not $mac) {
    Write-Host "ERROR: No I21* or I219* network adapter found" -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "Starting gptp with Milan profile (STABLE MODE), MAC: $mac" -ForegroundColor Green

# Generate timestamped log filename with full paths
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$logFile = Join-Path $scriptDir "build\Debug\gptp_milan_stable_$timestamp.log"
$errorFile = Join-Path $scriptDir "build\Debug\gptp_milan_stable_$timestamp.err"
$exePath = Join-Path $scriptDir "build\Debug\gptp.exe"

# Verify that the executable exists
if (-not (Test-Path $exePath)) {
    Write-Host "ERROR: gptp.exe not found at $exePath" -ForegroundColor Red
    Write-Host "Make sure to build the project first using 'Build project Debug' task" -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

try {
    Write-Host "Executing: $exePath -Milan $mac (NO debug-packets)" -ForegroundColor Cyan
    Write-Host "Log file: $logFile" -ForegroundColor Cyan
    Write-Host "Note: Enhanced debugging disabled to prevent crashes" -ForegroundColor Yellow
    
    # Run gptp WITHOUT -debug-packets to prevent crashes
    & $exePath -Milan $mac 2>&1 | Tee-Object -FilePath $logFile
    
    Write-Host "gptp execution completed. Log saved to $logFile" -ForegroundColor Green
} catch {
    $errorMessage = "Error running gptp: $_"
    Write-Host $errorMessage -ForegroundColor Red
    
    # Ensure error directory exists
    $errorDir = Split-Path $errorFile -Parent
    if (-not (Test-Path $errorDir)) {
        New-Item -ItemType Directory -Path $errorDir -Force | Out-Null
    }
    
    $errorMessage | Out-File -FilePath $errorFile -Append
    Write-Host "Error details saved to $errorFile" -ForegroundColor Red
}

Write-Host "Press Enter to exit..." -ForegroundColor Yellow
Read-Host
