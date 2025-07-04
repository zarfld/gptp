#!/usr/bin/env powershell
# Test script for Automotive Profile - AVB Specification 1.6 Compliance
# Tests the automotive gPTP implementation per automotive_config.ini

param(
    [string]$Interface = "",
    [switch]$Debug = $false,
    [switch]$Verbose = $false,
    [int]$Duration = 300  # 5 minutes default
)

Write-Host "=== Automotive Profile Test - AVB Spec 1.6 ===" -ForegroundColor Green
Write-Host "Testing automotive gPTP implementation compliance" -ForegroundColor Green
Write-Host ""

# Get timestamp for log files
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"

# Detect network interface if not specified
if ([string]::IsNullOrEmpty($Interface)) {
    Write-Host "Detecting network interface..." -ForegroundColor Yellow
    $adapter = Get-NetAdapter | Where-Object { $_.InterfaceDescription -like '*I21*' -or $_.Name -like '*Ethernet*' } | Select-Object -First 1
    if ($adapter) {
        $Interface = $adapter.MacAddress
        Write-Host "Detected interface: $($adapter.Name) ($Interface)" -ForegroundColor Green
    } else {
        Write-Host "No suitable interface found, using fallback MAC" -ForegroundColor Yellow
        $Interface = "00:11:22:33:44:55"
    }
}

# Check if gptp.exe exists
$gptpPath = "build\Release\gptp.exe"
if (-not (Test-Path $gptpPath)) {
    Write-Host "ERROR: gptp.exe not found at $gptpPath" -ForegroundColor Red
    Write-Host "Please build the project first using: cmake --build build --config Release" -ForegroundColor Red
    exit 1
}

# Check if automotive config exists
$configPath = "automotive_config.ini"
if (-not (Test-Path $configPath)) {
    Write-Host "ERROR: automotive_config.ini not found" -ForegroundColor Red
    exit 1
}

Write-Host "Configuration:" -ForegroundColor Cyan
Write-Host "  Interface: $Interface" -ForegroundColor White
Write-Host "  Config: $configPath" -ForegroundColor White
Write-Host "  Duration: $Duration seconds" -ForegroundColor White
Write-Host "  Debug: $Debug" -ForegroundColor White
Write-Host ""

# Prepare command arguments
$cmdArgs = @(
    $Interface,
    "-f", $configPath,
    "-automotive"  # Enable automotive profile
)

if ($Debug) {
    $cmdArgs += "-debug-packets"
}

if ($Verbose) {
    $cmdArgs += "-verbose"
}

# Create log file path
$logFile = "build\Release\automotive_test_$timestamp.log"
$errFile = "build\Release\automotive_test_$timestamp.err"

Write-Host "Starting automotive gPTP test..." -ForegroundColor Green
Write-Host "Log file: $logFile" -ForegroundColor Gray
Write-Host "Error file: $errFile" -ForegroundColor Gray
Write-Host ""

Write-Host "Expected automotive behaviors:" -ForegroundColor Yellow
Write-Host "1. No BMCA execution" -ForegroundColor White
Write-Host "2. No announce messages sent/expected" -ForegroundColor White  
Write-Host "3. asCapable=true on link up" -ForegroundColor White
Write-Host "4. Initial intervals: sync=125ms, pdelay=1s" -ForegroundColor White
Write-Host "5. Operational intervals: sync=1s, pdelay=8s (after 60s)" -ForegroundColor White
Write-Host "6. gPTP signaling message support" -ForegroundColor White
Write-Host "7. Follow_Up TLVs included" -ForegroundColor White
Write-Host "8. No sourcePortIdentity verification" -ForegroundColor White
Write-Host ""

# Start the test with timeout
try {
    Write-Host "Running: $gptpPath $($cmdArgs -join ' ')" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Press Ctrl+C to stop the test..." -ForegroundColor Yellow
    Write-Host ""
    
    # Run with timeout
    $process = Start-Process -FilePath $gptpPath -ArgumentList $cmdArgs -NoNewWindow -PassThru -RedirectStandardOutput $logFile -RedirectStandardError $errFile
    
    # Wait for specified duration or until process exits
    $timeout = $Duration * 1000  # Convert to milliseconds
    $finished = $process.WaitForExit($timeout)
    
    if (-not $finished) {
        Write-Host ""
        Write-Host "Test duration completed ($Duration seconds)" -ForegroundColor Green
        Write-Host "Stopping gPTP process..." -ForegroundColor Yellow
        $process.Kill()
        $process.WaitForExit(5000)  # Wait up to 5 seconds for clean exit
    }
    
    Write-Host ""
    Write-Host "=== Test Results ===" -ForegroundColor Green
    
    # Check log file for automotive-specific behavior
    if (Test-Path $logFile) {
        $logContent = Get-Content $logFile
        
        Write-Host "Checking automotive profile behaviors:" -ForegroundColor Cyan
        
        # Check for automotive profile activation
        $automotiveProfile = $logContent | Select-String "AUTOMOTIVE PROFILE"
        if ($automotiveProfile) {
            Write-Host "✓ Automotive profile activated" -ForegroundColor Green
        } else {
            Write-Host "✗ Automotive profile not detected" -ForegroundColor Red
        }
        
        # Check for BMCA disabled
        $bmcaDisabled = $logContent | Select-String "BMCA.*disabled|No BMCA"
        if ($bmcaDisabled) {
            Write-Host "✓ BMCA disabled (automotive compliant)" -ForegroundColor Green
        } else {
            Write-Host "? BMCA status unclear" -ForegroundColor Yellow
        }
        
        # Check for announce behavior
        $announceDisabled = $logContent | Select-String "announce.*disabled|No announce"
        if ($announceDisabled) {
            Write-Host "✓ Announce messages disabled" -ForegroundColor Green
        } else {
            Write-Host "? Announce message status unclear" -ForegroundColor Yellow
        }
        
        # Check for asCapable behavior
        $asCapable = $logContent | Select-String "asCapable.*true|Link.*up.*asCapable"
        if ($asCapable) {
            Write-Host "✓ asCapable behavior detected" -ForegroundColor Green
        } else {
            Write-Host "? asCapable behavior unclear" -ForegroundColor Yellow
        }
        
        # Check for interval information
        $syncInterval = $logContent | Select-String "sync.*interval|Sync.*125ms|125.*ms"
        if ($syncInterval) {
            Write-Host "✓ Sync interval information found" -ForegroundColor Green
        } else {
            Write-Host "? Sync interval information unclear" -ForegroundColor Yellow
        }
        
        Write-Host ""
        Write-Host "Last 20 lines of log:" -ForegroundColor Cyan
        $logContent | Select-Object -Last 20 | ForEach-Object { Write-Host "  $_" -ForegroundColor Gray }
    } else {
        Write-Host "WARNING: Log file not found: $logFile" -ForegroundColor Yellow
    }
    
    # Check for errors
    if (Test-Path $errFile) {
        $errContent = Get-Content $errFile
        if ($errContent.Count -gt 0) {
            Write-Host ""
            Write-Host "Errors detected:" -ForegroundColor Red
            $errContent | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
        }
    }
    
    Write-Host ""
    Write-Host "Test completed. Check log files for detailed information:" -ForegroundColor Green
    Write-Host "  Log: $logFile" -ForegroundColor Gray
    Write-Host "  Errors: $errFile" -ForegroundColor Gray
    
} catch {
    Write-Host ""
    Write-Host "ERROR during test execution: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}
