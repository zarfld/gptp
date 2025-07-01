# Simple PreSonus Live Network Test

Write-Host "[LIVE TEST] Starting PreSonus gPTP Network Testing" -ForegroundColor Green

# Get AVB interface MAC
$mac = (Get-NetAdapter | Where-Object { $_.Name -eq "AVB" } | Select-Object -First 1 -ExpandProperty MacAddress)
if (-not $mac) {
    Write-Host "[ERROR] AVB interface not found" -ForegroundColor Red
    exit 1
}

Write-Host "[INFO] Using AVB interface MAC: $mac" -ForegroundColor Yellow

# Use grandmaster config for strong master election
Write-Host "[CONFIG] Setting up as grandmaster (priority1=50, clockClass=6)" -ForegroundColor Cyan
cd "c:\Users\dzarf\source\repos\gptp-1\build\Debug"
Copy-Item "..\..\grandmaster_config.ini" "gptp_cfg.ini" -Force

# Start gPTP
Write-Host "[START] Starting gPTP daemon..." -ForegroundColor Green
Start-Process -FilePath "gptp.exe" -ArgumentList $mac -NoNewWindow

Write-Host "[INFO] gPTP is now running. You should:" -ForegroundColor Yellow
Write-Host "  1. Connect PreSonus devices to the AVB network" -ForegroundColor White
Write-Host "  2. Start Wireshark capture on AVB interface" -ForegroundColor White
Write-Host "  3. Use filter: eth.type == 0x88f7" -ForegroundColor White
Write-Host "  4. Look for gPTP Announce messages with our priority1=50" -ForegroundColor White
Write-Host "  5. Observe BMCA master election process" -ForegroundColor White

Write-Host "`n[WIRESHARK] Key things to watch for:" -ForegroundColor Cyan
Write-Host "  • PC should announce as master with priority1=50, clockClass=6" -ForegroundColor White
Write-Host "  • PreSonus devices likely announce with priority1=248, clockClass=248" -ForegroundColor White
Write-Host "  • PC should win master election and start sending Sync messages" -ForegroundColor White
Write-Host "  • Look for PDelay request/response exchanges" -ForegroundColor White
Write-Host "  • Monitor for any AVDECC discovery messages" -ForegroundColor White

Write-Host "`n[MANUAL] Manual steps to continue:" -ForegroundColor Yellow
Write-Host "1. Open Wireshark" -ForegroundColor White
Write-Host "2. Select 'AVB' or 'Intel(R) Ethernet I210-T1 GbE NIC' interface" -ForegroundColor White
Write-Host "3. Start capture with filter: eth.type == 0x88f7" -ForegroundColor White
Write-Host "4. Connect PreSonus devices to the same network" -ForegroundColor White
Write-Host "5. Watch for gPTP message exchanges" -ForegroundColor White
Write-Host "6. Press Ctrl+C to stop gPTP when done" -ForegroundColor White

Write-Host "`n[STATUS] Live testing ready - gPTP running with Milan grandmaster config" -ForegroundColor Green
