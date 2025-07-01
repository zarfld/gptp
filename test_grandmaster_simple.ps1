# Test gPTP as Grandmaster
param([int]$DurationSeconds = 45)

Write-Host "🚀 Testing gPTP Grandmaster Configuration" -ForegroundColor Cyan

# Get network adapter MAC address
$adapter = Get-NetAdapter | Where-Object { $_.InterfaceDescription -like "*I210*" } | Select-Object -First 1
if (-not $adapter) {
    $adapter = Get-NetAdapter | Where-Object { $_.Status -eq "Up" } | Select-Object -First 1
}

$macAddress = $adapter.MacAddress
Write-Host "📶 Using adapter: $($adapter.Name)" -ForegroundColor Green
Write-Host "🔢 MAC Address: $macAddress" -ForegroundColor Green

# Change to build directory
Set-Location "c:\Users\dzarf\source\repos\gptp-1\build\Debug"

Write-Host "🔍 Starting gPTP with grandmaster configuration..." -ForegroundColor Cyan
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$logFile = "grandmaster_test_$timestamp.log"

# Start gPTP process
$process = Start-Process -FilePath ".\gptp.exe" -ArgumentList $macAddress -PassThru -NoNewWindow -RedirectStandardOutput $logFile

Start-Sleep -Seconds $DurationSeconds

# Stop the process
Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue

Write-Host "📊 Analysis Results:" -ForegroundColor Cyan

# Analyze the log
if (Test-Path $logFile) {
    $logContent = Get-Content $logFile
    $announceCount = ($logContent | Select-String "Announce").Count
    $syncCount = ($logContent | Select-String "Sync").Count
    
    Write-Host "📤 Announce messages: $announceCount"
    Write-Host "📤 Sync messages: $syncCount"
    
    Write-Host "`n📋 Last 15 log lines:"
    $logContent | Select-Object -Last 15 | ForEach-Object { Write-Host "  $_" }
    
    Write-Host "`n✅ Test completed. Full log: $logFile"
} else {
    Write-Host "❌ No log file created"
}
