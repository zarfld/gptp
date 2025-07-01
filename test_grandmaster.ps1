# Test gPTP as Grandmaster with Enhanced Logging
param(
    [int]$DurationSeconds = 60
)

Write-Host "🚀 Testing gPTP Grandmaster Configuration" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# Get network adapter MAC address
$adapter = Get-NetAdapter | Where-Object { $_.InterfaceDescription -like "*I210*" } | Select-Object -First 1
if (-not $adapter) {
    $adapter = Get-NetAdapter | Where-Object { $_.Status -eq "Up" } | Select-Object -First 1
}

if (-not $adapter) {
    Write-Host "❌ No suitable network adapter found" -ForegroundColor Red
    exit 1
}

$macAddress = $adapter.MacAddress
Write-Host "📶 Using adapter: $($adapter.Name) ($($adapter.InterfaceDescription))" -ForegroundColor Green
Write-Host "🔢 MAC Address: $macAddress" -ForegroundColor Green

# Change to build directory
Set-Location "c:\Users\dzarf\source\repos\gptp-1\build\Debug"

# Verify config file
if (Test-Path "..\..\gptp_cfg.ini") {
    Write-Host "📄 Using config: grandmaster_config.ini" -ForegroundColor Green
    Get-Content "..\..\gptp_cfg.ini" | Write-Host
} else {
    Write-Host "❌ Config file not found" -ForegroundColor Red
    exit 1
}

Write-Host "`n🎯 Expected Behavior:" -ForegroundColor Yellow
Write-Host "- Device should announce itself as grandmaster" -ForegroundColor Yellow
Write-Host "- Priority1=50, Priority2=1 (high priority)" -ForegroundColor Yellow
Write-Host "- ClockClass=6 (GPS quality)" -ForegroundColor Yellow
Write-Host "- Should send Announce messages" -ForegroundColor Yellow
Write-Host "- Should send Sync messages" -ForegroundColor Yellow

Write-Host "`n🔍 Starting gPTP with grandmaster configuration..." -ForegroundColor Cyan
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$logFile = "grandmaster_test_$timestamp.log"

Write-Host "📝 Logging to: $logFile" -ForegroundColor Green
Write-Host "⏱️  Running for $DurationSeconds seconds..." -ForegroundColor Green

# Start gPTP process
$process = Start-Process -FilePath ".\gptp.exe" -ArgumentList $macAddress -PassThru -NoNewWindow -RedirectStandardOutput $logFile -RedirectStandardError "error_$logFile"

Start-Sleep -Seconds $DurationSeconds

# Stop the process
$process.Kill()
$process.WaitForExit()

Write-Host "`n📊 Analysis Results:" -ForegroundColor Cyan
Write-Host "====================" -ForegroundColor Cyan

# Analyze the log
$logContent = Get-Content $logFile -ErrorAction SilentlyContinue
if ($logContent) {
    $announceCount = ($logContent | Select-String "Announce" | Measure-Object).Count
    $syncCount = ($logContent | Select-String "Sync" | Measure-Object).Count
    $grandmasterLines = $logContent | Select-String "grandmaster|master|BMCA" 
    $clockLines = $logContent | Select-String "clock.*class|priority"
    
    if ($announceCount -gt 0) {
        Write-Host "📤 Announce messages: $announceCount" -ForegroundColor Green
    } else {
        Write-Host "📤 Announce messages: $announceCount" -ForegroundColor Red
    }
    
    if ($syncCount -gt 0) {
        Write-Host "📤 Sync messages: $syncCount" -ForegroundColor Green
    } else {
        Write-Host "📤 Sync messages: $syncCount" -ForegroundColor Red
    }
    
    if ($grandmasterLines) {
        Write-Host "`n🎯 Grandmaster/BMCA Events:" -ForegroundColor Yellow
        $grandmasterLines | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
    }
    
    if ($clockLines) {
        Write-Host "`n⏰ Clock Quality Events:" -ForegroundColor Yellow
        $clockLines | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
    }
    
    # Show last 10 lines
    Write-Host "`n📋 Last 10 log lines:" -ForegroundColor Yellow
    $logContent | Select-Object -Last 10 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
} else {
    Write-Host "❌ No log output captured" -ForegroundColor Red
}

Write-Host "`n✅ Test completed. Log saved to: $logFile" -ForegroundColor Green
