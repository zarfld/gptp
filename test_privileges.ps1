# Compare Standard vs Administrator Mode for gPTP
# This script tests gPTP in current mode and provides guidance for admin testing

Write-Host "=== gPTP Standard vs Administrator Mode Comparison ===" -ForegroundColor Yellow

# Check current privileges
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

Write-Host "`n=== CURRENT SESSION INFO ===" -ForegroundColor Cyan
Write-Host "User: $(whoami)"
Write-Host "Administrator: $isAdmin"
if ($isAdmin) {
    Write-Host "Status: RUNNING AS ADMINISTRATOR" -ForegroundColor Green
} else {
    Write-Host "Status: Running as standard user" -ForegroundColor Yellow
}

# Test current mode first
Write-Host "`n=== TESTING CURRENT MODE ===" -ForegroundColor Cyan
Write-Host "Running gPTP for 15 seconds in current privilege mode..."

# Stop any existing processes
Stop-Process -Name "gptp" -Force -ErrorAction SilentlyContinue

# Navigate to build directory
Set-Location "C:\Users\dzarf\source\repos\gptp-1\build\Debug"

# Run gPTP and capture initial output
Write-Host "`nStarting gPTP - watching for Intel PTP hardware status..." -ForegroundColor Green

# Start gPTP process
$process = Start-Process -FilePath ".\gptp.exe" -ArgumentList "68-05-CA-8B-76-4E" -PassThru -NoNewWindow -RedirectStandardOutput "current_mode_output.txt" -RedirectStandardError "current_mode_errors.txt"

# Wait 15 seconds
Start-Sleep -Seconds 15

# Stop the process
if (!$process.HasExited) {
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
}

# Analyze output
Write-Host "`n=== ANALYZING CURRENT MODE RESULTS ===" -ForegroundColor Cyan

$output = Get-Content "current_mode_output.txt" -ErrorAction SilentlyContinue
$errors = Get-Content "current_mode_errors.txt" -ErrorAction SilentlyContinue

# Check for key indicators
$hardwareClockRunning = $output | Select-String "net_time=[^0]" -Quiet
$hardwareTimestamps = $output | Select-String "Intel PTP network clock is not running" -Quiet
$softwareTimestamps = $output | Select-String "software timestamping" -Quiet
$errorCount = ($output | Select-String "ERROR|WARNING").Count

Write-Host "Hardware PTP Clock Running: $(!$hardwareTimestamps)" -ForegroundColor $(if($hardwareTimestamps) { "Red" } else { "Green" })
Write-Host "Using Software Timestamps: $softwareTimestamps" -ForegroundColor $(if($softwareTimestamps) { "Yellow" } else { "Green" })
Write-Host "Error/Warning Count: $errorCount" -ForegroundColor $(if($errorCount -gt 10) { "Red" } elseif($errorCount -gt 5) { "Yellow" } else { "Green" })

if (!$isAdmin) {
    Write-Host "`n=== RECOMMENDATION ===" -ForegroundColor Yellow
    Write-Host "You are currently running as a standard user."
    Write-Host "To test administrator mode:"
    Write-Host "1. Right-click on PowerShell and 'Run as Administrator'"
    Write-Host "2. Navigate to: C:\Users\dzarf\source\repos\gptp-1"
    Write-Host "3. Run: .\run_gptp_admin.ps1"
    Write-Host "`nExpected improvements with admin privileges:"
    Write-Host "- Intel PTP hardware clock should start (net_time > 0)"
    Write-Host "- Hardware timestamps should work"
    Write-Host "- Fewer errors and warnings"
    Write-Host "- Better timestamp precision"
} else {
    Write-Host "`n=== ADMINISTRATOR MODE ACTIVE ===" -ForegroundColor Green
    Write-Host "This test was run with administrator privileges."
    if ($hardwareTimestamps) {
        Write-Host "Hardware PTP clock is still not running. This may indicate:"
        Write-Host "- Intel I210 driver issues"
        Write-Host "- Hardware configuration problems"
        Write-Host "- BIOS/UEFI settings for PTP"
    } else {
        Write-Host "Hardware PTP appears to be working correctly!" -ForegroundColor Green
    }
}

Write-Host "`n=== OUTPUT FILES CREATED ===" -ForegroundColor Cyan
Write-Host "- current_mode_output.txt (stdout)"
Write-Host "- current_mode_errors.txt (stderr)"
Write-Host "Review these files for detailed analysis."

Read-Host "`nPress Enter to continue"
