# Live PreSonus Network Testing Guide
# Testing gPTP Milan Compliance with Real PreSonus Devices

Write-Host "🎵 === PRESONUS LIVE NETWORK TESTING FRAMEWORK ===" -ForegroundColor Green
Write-Host "Testing gPTP Milan compliance with actual PreSonus hardware" -ForegroundColor Cyan

# Network Interface Detection
$avbInterface = Get-NetAdapter | Where-Object { $_.Name -eq "AVB" -and $_.Status -eq "Up" }
if ($avbInterface) {
    Write-Host "[OK] AVB interface detected: $($avbInterface.InterfaceDescription)" -ForegroundColor Green
    $mac = $avbInterface.MacAddress
    Write-Host "[INFO] AVB MAC Address: $mac" -ForegroundColor Yellow
} else {
    Write-Host "[ERROR] AVB interface not found or not connected" -ForegroundColor Red
    exit 1
}

# Pre-Test Network Diagnostics
Write-Host "`n[DIAGNOSTICS] Network Interface Analysis:" -ForegroundColor Cyan
Write-Host "Interface: $($avbInterface.InterfaceDescription)" -ForegroundColor White
Write-Host "Link Speed: $($avbInterface.LinkSpeed)" -ForegroundColor White
Write-Host "Status: $($avbInterface.Status)" -ForegroundColor White

# Check for PreSonus devices on network
Write-Host "`n[DISCOVERY] Scanning for PreSonus devices..." -ForegroundColor Cyan
$networkPrefix = (Get-NetIPAddress -InterfaceAlias "AVB" -AddressFamily IPv4).IPAddress -replace '\.\d+$', ''
Write-Host "Scanning network: $networkPrefix.0/24" -ForegroundColor White

# Quick ping sweep for active devices
$activeDevices = @()
1..254 | ForEach-Object {
    $ip = "$networkPrefix.$_"
    if (Test-Connection -ComputerName $ip -Count 1 -Quiet -TimeoutSeconds 1) {
        $activeDevices += $ip
    }
}

if ($activeDevices.Count -gt 0) {
    Write-Host "[FOUND] Active devices on AVB network:" -ForegroundColor Green
    foreach ($device in $activeDevices) {
        Write-Host "  - $device" -ForegroundColor White
    }
} else {
    Write-Host "[INFO] No devices responding to ping on AVB network" -ForegroundColor Yellow
    Write-Host "This is normal for AVB-only devices that may not respond to IP pings" -ForegroundColor Yellow
}

# Test Phase 1: Grandmaster Mode
Write-Host "`n[TEST 1] Starting as Grandmaster Clock" -ForegroundColor Green
Write-Host "Using grandmaster_config.ini with priority1=50, clockClass=6" -ForegroundColor White

cd "c:\Users\dzarf\source\repos\gptp-1\build\Debug"

# Copy grandmaster config
Copy-Item "..\..\grandmaster_config.ini" "gptp_cfg.ini" -Force
Write-Host "[CONFIG] Grandmaster configuration applied" -ForegroundColor Green

# Start gPTP in grandmaster mode
Write-Host "[START] Launching gPTP as grandmaster..." -ForegroundColor Cyan
$gptpJob = Start-Job -ScriptBlock {
    param($mac)
    cd "c:\Users\dzarf\source\repos\gptp-1\build\Debug"
    & ".\gptp.exe" $mac 2>&1
} -ArgumentList $mac

Start-Sleep 3

# Check if gPTP started
$gptpProcess = Get-Process -Name "gptp" -ErrorAction SilentlyContinue
if ($gptpProcess) {
    Write-Host "[OK] gPTP grandmaster started (PID: $($gptpProcess.Id))" -ForegroundColor Green
} else {
    Write-Host "[ERROR] gPTP failed to start" -ForegroundColor Red
    exit 1
}

# Monitor for grandmaster behavior
Write-Host "`n[MONITOR] Monitoring grandmaster behavior (30 seconds)..." -ForegroundColor Cyan
Write-Host "Look for:" -ForegroundColor Yellow
Write-Host "  - MILAN PROFILE ENABLED messages" -ForegroundColor White
Write-Host "  - Priority1=50, ClockClass=6 announcements" -ForegroundColor White
Write-Host "  - PDelay request/response exchanges" -ForegroundColor White
Write-Host "  - BMCA master election (should win with priority1=50)" -ForegroundColor White

Start-Sleep 30

# Get initial output
$output1 = Receive-Job $gptpJob
Write-Host "`n[LOG] Recent gPTP output:" -ForegroundColor Cyan
$output1 | Select-Object -Last 10 | ForEach-Object { Write-Host "  $_" -ForegroundColor White }

# Test Phase 2: Network Detection
Write-Host "`n[TEST 2] PreSonus Device Detection Analysis" -ForegroundColor Green

# Check for announce messages from other devices
$announceMessages = $output1 | Select-String "Announce" -AllMatches
if ($announceMessages.Count -gt 0) {
    Write-Host "[DETECTED] Announce messages found - other gPTP devices present!" -ForegroundColor Green
    $announceMessages | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
} else {
    Write-Host "[INFO] No announce messages detected yet" -ForegroundColor Yellow
}

# Check for PDelay responses (indicates other devices)
$pdelayResponses = $output1 | Select-String "PDelay.*Response" -AllMatches
if ($pdelayResponses.Count -gt 0) {
    Write-Host "[DETECTED] PDelay responses - PreSonus devices responding!" -ForegroundColor Green
    $pdelayResponses | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
} else {
    Write-Host "[INFO] No PDelay responses yet - may need more time or devices not connected" -ForegroundColor Yellow
}

# Extended monitoring for PreSonus discovery
Write-Host "`n[EXTENDED] Extended monitoring for PreSonus device discovery..." -ForegroundColor Cyan
Write-Host "Monitoring for 60 more seconds - connect PreSonus devices now if not already connected" -ForegroundColor Yellow

$monitorStart = Get-Date
$deviceDetected = $false

while ((Get-Date) -lt $monitorStart.AddSeconds(60) -and -not $deviceDetected) {
    Start-Sleep 5
    $recentOutput = Receive-Job $gptpJob
    
    # Check for new device activity
    $newAnnounces = $recentOutput | Select-String "Announce|BMCA|Master|Slave" -AllMatches
    $newPDelay = $recentOutput | Select-String "PDelay.*Success|successful.*exchange" -AllMatches
    
    if ($newAnnounces.Count -gt 0 -or $newPDelay.Count -gt 0) {
        Write-Host "[ACTIVITY] New gPTP activity detected!" -ForegroundColor Green
        $newAnnounces | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
        $newPDelay | ForEach-Object { Write-Host "  $_" -ForegroundColor White }
        $deviceDetected = $true
    } else {
        Write-Host "." -NoNewline -ForegroundColor Gray
    }
}

Write-Host ""

# Final analysis
Write-Host "`n[ANALYSIS] Live Test Results Summary:" -ForegroundColor Cyan

$finalOutput = Receive-Job $gptpJob
$milanEnabled = $finalOutput | Select-String "MILAN PROFILE ENABLED"
$grandmasterActive = $finalOutput | Select-String "priority1.*50|clockClass.*6"
$pdelayActive = $finalOutput | Select-String "PDELAY_INTERVAL_TIMEOUT_EXPIRES"

if ($milanEnabled) {
    Write-Host "[OK] Milan profile successfully enabled" -ForegroundColor Green
} else {
    Write-Host "[ERROR] Milan profile not enabled" -ForegroundColor Red
}

if ($grandmasterActive) {
    Write-Host "[OK] Grandmaster parameters active (priority1=50, clockClass=6)" -ForegroundColor Green
} else {
    Write-Host "[ERROR] Grandmaster parameters not active" -ForegroundColor Red
}

if ($pdelayActive) {
    Write-Host "[OK] PDelay protocol active" -ForegroundColor Green
} else {
    Write-Host "[ERROR] PDelay protocol not active" -ForegroundColor Red
}

# Stop gPTP
Stop-Process -Name "gptp" -Force -ErrorAction SilentlyContinue
Remove-Job $gptpJob -Force

Write-Host "`n[NEXT STEPS] Wireshark Analysis Recommendations:" -ForegroundColor Yellow
Write-Host "1. Install Wireshark if not already available" -ForegroundColor White
Write-Host "2. Capture on AVB interface during gPTP operation" -ForegroundColor White
Write-Host "3. Filter for: 'eth.type == 0x88f7' (gPTP messages)" -ForegroundColor White
Write-Host "4. Look for Announce, Sync, Follow_Up, and PDelay messages" -ForegroundColor White
Write-Host "5. Analyze BMCA master election between PC and PreSonus devices" -ForegroundColor White

Write-Host "`n[SUMMARY] Live testing phase initiated successfully!" -ForegroundColor Green
Write-Host "Ready for Wireshark capture and detailed protocol analysis." -ForegroundColor Cyan
