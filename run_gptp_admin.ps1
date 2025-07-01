# Run gPTP as Administrator with Enhanced Monitoring
# This script will restart PowerShell with elevated privileges and provide detailed monitoring

param(
    [string]$MacAddress = "68-05-CA-8B-76-4E",
    [int]$Duration = 30
)

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

if (-not $isAdmin) {
    Write-Host "=== gPTP Administrator Mode Test ===" -ForegroundColor Yellow
    Write-Host "Requesting administrator privileges..." -ForegroundColor Yellow
    $scriptPath = $MyInvocation.MyCommand.Path
    Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoExit", "-File", "`"$scriptPath`"", "-MacAddress", $MacAddress, "-Duration", $Duration
    exit
}

# We're running as admin now
Write-Host "=== RUNNING AS ADMINISTRATOR ===" -ForegroundColor Green
Write-Host "Current user: $(whoami)" -ForegroundColor Green
Write-Host "Testing hardware PTP capabilities..." -ForegroundColor Green

# Stop any existing gPTP processes
Write-Host "`nStopping any existing gPTP processes..."
Stop-Process -Name "gptp" -Force -ErrorAction SilentlyContinue

# Navigate to the gPTP directory
Set-Location "C:\Users\dzarf\source\repos\gptp-1\build\Debug"

# Show system information
Write-Host "`n=== SYSTEM INFORMATION ===" -ForegroundColor Cyan
Write-Host "Network Adapter: Intel I210-T1 GbE NIC"
Write-Host "MAC Address: $MacAddress"
Write-Host "Privilege Level: Administrator" -ForegroundColor Green

# Show configuration
Write-Host "`n=== CONFIGURATION ===" -ForegroundColor Cyan
Get-Content "gptp_cfg.ini" | Select-Object -First 10

# Start gPTP with monitoring
Write-Host "`n=== STARTING GPTP WITH HARDWARE PTP ACCESS ===" -ForegroundColor Green
Write-Host "Duration: $Duration seconds"
Write-Host "Watch for 'Intel PTP network clock' messages..."
Write-Host "Expected improvements:"
Write-Host "  - Intel PTP clock should start (net_time > 0)"
Write-Host "  - Hardware timestamps should work"
Write-Host "  - Error rates should decrease significantly"
Write-Host "`nPress Ctrl+C to stop early...`n" -ForegroundColor Yellow

# Run gPTP and capture output
$process = Start-Process -FilePath ".\gptp.exe" -ArgumentList $MacAddress -PassThru -NoNewWindow

# Wait for specified duration or until process exits
$timeout = (Get-Date).AddSeconds($Duration)
while ((Get-Date) -lt $timeout -and !$process.HasExited) {
    Start-Sleep -Seconds 1
}

# Stop the process if still running
if (!$process.HasExited) {
    Write-Host "`n=== STOPPING GPTP AFTER $Duration SECONDS ===" -ForegroundColor Yellow
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
}

Write-Host "`n=== ADMINISTRATOR TEST COMPLETE ===" -ForegroundColor Green
Write-Host "Check the output above for hardware PTP status." -ForegroundColor Yellow
Write-Host "Look for changes in 'Intel PTP network clock' and timestamp messages." -ForegroundColor Yellow
Read-Host "`nPress Enter to close"
