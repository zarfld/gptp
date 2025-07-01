# Start Wireshark Capture for gPTP Verification
# This script starts Wireshark with the correct interface and filters for gPTP analysis

Write-Host "=== Starting Wireshark for gPTP Analysis ===" -ForegroundColor Green

# Find the Intel I210 network interface
$adapter = Get-NetAdapter | Where-Object { $_.InterfaceDescription -like '*I210*' } | Select-Object -First 1

if (-not $adapter) {
    Write-Error "No Intel I210 adapter found!"
    exit 1
}

Write-Host "Target Interface: $($adapter.InterfaceDescription)" -ForegroundColor Yellow
Write-Host "Interface Index: $($adapter.InterfaceIndex)" -ForegroundColor Yellow
Write-Host "MAC Address: $($adapter.MacAddress)" -ForegroundColor Yellow

# Check if Wireshark is installed
$wiresharkPath = "C:\Program Files\Wireshark\Wireshark.exe"
if (-not (Test-Path $wiresharkPath)) {
    Write-Error "Wireshark not found at: $wiresharkPath"
    Write-Host "Please install Wireshark or update the path"
    exit 1
}

# Create Wireshark display filter for comprehensive gPTP analysis
$displayFilter = "ptp or (eth.type == 0x88f7)"

Write-Host ""
Write-Host "=== Starting Wireshark Capture ===" -ForegroundColor Cyan
Write-Host "Filter: $displayFilter"
Write-Host "Interface: $($adapter.InterfaceDescription)"

# Start Wireshark with the specific interface and filter
# Note: We use the interface name that Wireshark recognizes
try {
    Start-Process -FilePath $wiresharkPath -ArgumentList @(
        "-i", $adapter.InterfaceIndex,
        "-k",  # Start capture immediately
        "-Y", $displayFilter  # Display filter for gPTP
    )
    
    Write-Host ""
    Write-Host "✅ Wireshark started successfully!" -ForegroundColor Green
    Write-Host ""
    Write-Host "=== What to Look For in Wireshark ===" -ForegroundColor Yellow
    Write-Host "1. PDelay Request messages FROM PreSonus devices (should appear regularly)"
    Write-Host "2. PDelay Response messages FROM your PC (should appear in response to #1)"
    Write-Host "3. PDelay Response FollowUp messages FROM your PC"
    Write-Host "4. PDelay Request messages FROM your PC (should appear every 1-3 seconds)"
    Write-Host "5. Announce messages (master election and clock quality)"
    Write-Host "6. Sync messages (timing synchronization)"
    Write-Host ""
    Write-Host "=== Next Steps ===" -ForegroundColor Cyan
    Write-Host "1. Wait for Wireshark to fully load (about 10-15 seconds)"
    Write-Host "2. Run the gPTP daemon: .\verify_pdelay_exchange.ps1"
    Write-Host "3. Watch for bidirectional PDelay exchange in Wireshark"
    Write-Host "4. Look for 'Info' column showing message types and sequence numbers"
    
} catch {
    Write-Error "Failed to start Wireshark: $($_.Exception.Message)"
    Write-Host "Try running Wireshark manually and select interface: $($adapter.InterfaceDescription)"
}

Write-Host ""
Write-Host "Press Enter when ready to proceed with gPTP testing..." -ForegroundColor Green
Read-Host
