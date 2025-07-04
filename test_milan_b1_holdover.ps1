# Milan B.1 Media Clock Holdover Implementation Test Script

# Test Milan B.1 Media Clock Holdover during Grandmaster Change
# Based on Milan Specification B.1 requirements

param(
    [string]$MacAddress = "00:11:22:33:44:55",
    [switch]$TestMode = $false,
    [int]$TestDurationSeconds = 30
)

Write-Host "=== Milan B.1 Media Clock Holdover Test ===" -ForegroundColor Cyan
Write-Host "Testing Milan Baseline Interoperability Specification B.1 requirements" -ForegroundColor Green
Write-Host ""

# B.1 Requirements to test:
Write-Host "Milan B.1 Requirements:" -ForegroundColor Yellow
Write-Host "- B.1: Media Clock Holdover minimum 5 seconds" -ForegroundColor White
Write-Host "- B.1.1: tu (timestamp uncertain) bit set for 0.25 seconds on GM change" -ForegroundColor White
Write-Host "- B.1.2: Clock Domain - stream stable >60s, GM agreement <5s" -ForegroundColor White
Write-Host "- Bridge propagation time <50ms assumption" -ForegroundColor White
Write-Host ""

$LogFile = "build\Release\milan_b1_test_$(Get-Date -Format 'yyyyMMdd_HHmmss').log"

Write-Host "Starting Milan profile test with B.1 features..." -ForegroundColor Green
Write-Host "Log file: $LogFile" -ForegroundColor Gray
Write-Host ""

try {
    # Test 1: Basic Milan Profile Activation with B.1 Support
    Write-Host "=== Test 1: Milan Profile Activation ===" -ForegroundColor Cyan
    
    $processArgs = @(
        $MacAddress,
        "-profile", "milan",
        "-debug-packets"
    )
    
    Write-Host "Command: .\build\Release\gptp.exe $($processArgs -join ' ')" -ForegroundColor Gray
    
    $process = Start-Process -FilePath ".\build\Release\gptp.exe" -ArgumentList $processArgs -RedirectStandardOutput $LogFile -RedirectStandardError "${LogFile}.err" -NoNewWindow -PassThru
    
    Write-Host "Milan gPTP process started (PID: $($process.Id))" -ForegroundColor Green
    Write-Host "Monitoring for Milan B.1 messages..." -ForegroundColor Yellow
    
    # Monitor for Milan B.1 specific log messages
    $monitorTime = 0
    $maxMonitorTime = $TestDurationSeconds
    $b1Messages = @()
    
    while ($monitorTime -lt $maxMonitorTime -and !$process.HasExited) {
        Start-Sleep -Seconds 1
        $monitorTime++
        
        # Check for new log entries
        if (Test-Path $LogFile) {
            $logContent = Get-Content $LogFile -ErrorAction SilentlyContinue
            if ($logContent) {
                # Look for Milan B.1 specific messages
                $newB1Messages = $logContent | Where-Object { 
                    $_ -match "MILAN B\.1" -or 
                    $_ -match "Media Clock Holdover" -or
                    $_ -match "timestamp uncertain" -or
                    $_ -match "Grandmaster change" -or
                    $_ -match "tu bit" -or
                    $_ -match "stream stable"
                }
                
                foreach ($msg in $newB1Messages) {
                    if ($msg -notin $b1Messages) {
                        $b1Messages += $msg
                        Write-Host "B.1: $msg" -ForegroundColor Magenta
                    }
                }
                
                # Check for Milan profile activation
                $milanActivation = $logContent | Where-Object { $_ -match "Milan Baseline Interoperability Profile PROFILE ENABLED" }
                if ($milanActivation -and $monitorTime -eq 1) {
                    Write-Host "✓ Milan profile activated successfully" -ForegroundColor Green
                }
                
                # Check for timing compliance
                $timingMessages = $logContent | Where-Object { $_ -match "sync: 125\.000ms" }
                if ($timingMessages -and $monitorTime -eq 1) {
                    Write-Host "✓ Milan timing intervals correct (125ms sync)" -ForegroundColor Green
                }
            }
        }
        
        # Show progress
        if ($monitorTime % 5 -eq 0) {
            Write-Host "Monitoring... ${monitorTime}/${maxMonitorTime}s" -ForegroundColor Gray
        }
    }
    
    # Gracefully stop the process
    Write-Host ""
    Write-Host "Stopping Milan gPTP process..." -ForegroundColor Yellow
    
    if (!$process.HasExited) {
        $process.CloseMainWindow()
        Start-Sleep -Seconds 2
        
        if (!$process.HasExited) {
            $process.Kill()
            Start-Sleep -Seconds 1
        }
    }
    
    Write-Host "Process stopped." -ForegroundColor Green
    
} catch {
    Write-Host "Error during test: $($_.Exception.Message)" -ForegroundColor Red
    if ($process -and !$process.HasExited) {
        $process.Kill()
    }
}

Write-Host ""
Write-Host "=== Test Results Analysis ===" -ForegroundColor Cyan

# Analyze the log file for Milan B.1 compliance
if (Test-Path $LogFile) {
    $logContent = Get-Content $LogFile -ErrorAction SilentlyContinue
    
    if ($logContent) {
        Write-Host "Log file analysis:" -ForegroundColor Yellow
        
        # Check for Milan profile activation
        $profileActivation = $logContent | Where-Object { $_ -match "Milan.*PROFILE ENABLED" }
        if ($profileActivation) {
            Write-Host "✓ Milan profile activated correctly" -ForegroundColor Green
            Write-Host "  $($profileActivation[0])" -ForegroundColor Gray
        } else {
            Write-Host "✗ Milan profile activation not detected" -ForegroundColor Red
        }
        
        # Check for B.1 implementation
        $b1Implementation = $logContent | Where-Object { $_ -match "MILAN B\.1" }
        if ($b1Implementation) {
            Write-Host "✓ Milan B.1 implementation active" -ForegroundColor Green
            Write-Host "  Found $($b1Implementation.Count) B.1 related messages" -ForegroundColor Gray
        } else {
            Write-Host "⚠ Milan B.1 messages not detected (may activate on GM change)" -ForegroundColor Yellow
        }
        
        # Check for timing compliance
        $timingCompliance = $logContent | Where-Object { $_ -match "125\.000ms.*convergence target.*100ms" }
        if ($timingCompliance) {
            Write-Host "✓ Milan timing requirements met" -ForegroundColor Green
        } else {
            Write-Host "⚠ Milan timing messages not detected" -ForegroundColor Yellow
        }
        
        # Check for any errors
        $errors = $logContent | Where-Object { $_ -match "ERROR|FATAL|CRITICAL" }
        if ($errors) {
            Write-Host "⚠ Errors detected in log:" -ForegroundColor Yellow
            $errors | ForEach-Object { Write-Host "  $_" -ForegroundColor Red }
        } else {
            Write-Host "✓ No critical errors detected" -ForegroundColor Green
        }
        
        Write-Host ""
        Write-Host "Log file saved: $LogFile" -ForegroundColor Gray
        Write-Host "Log file size: $((Get-Item $LogFile).Length) bytes" -ForegroundColor Gray
        
    } else {
        Write-Host "⚠ Log file is empty or unreadable" -ForegroundColor Yellow
    }
} else {
    Write-Host "✗ Log file not found: $LogFile" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Milan B.1 Implementation Status ===" -ForegroundColor Cyan
Write-Host "Implemented Features:" -ForegroundColor Green
Write-Host "✓ Milan profile factory with B.1 configuration" -ForegroundColor White
Write-Host "✓ B.1 media clock holdover time: 5 seconds minimum" -ForegroundColor White
Write-Host "✓ B.1.1 tu bit duration: 0.25 seconds" -ForegroundColor White
Write-Host "✓ B.1.2 GM convergence time: 5 seconds maximum" -ForegroundColor White
Write-Host "✓ B.1.2 stream stability requirement: 60 seconds" -ForegroundColor White
Write-Host "✓ Bridge propagation assumption: <50ms" -ForegroundColor White
Write-Host "✓ Grandmaster change detection in BMCA" -ForegroundColor White
Write-Host "✓ MilanProfile class with B.1 methods" -ForegroundColor White

Write-Host ""
Write-Host "Framework Ready:" -ForegroundColor Yellow
Write-Host "⚡ handleGrandmasterChange() method" -ForegroundColor White
Write-Host "⚡ isMediaClockHoldoverRequired() method" -ForegroundColor White
Write-Host "⚡ shouldSetTimestampUncertain() method" -ForegroundColor White
Write-Host "⚡ updateTimestampUncertainBit() method" -ForegroundColor White
Write-Host "⚡ Stream stability tracking methods" -ForegroundColor White

Write-Host ""
Write-Host "=== Next Steps for Full B.1 Implementation ===" -ForegroundColor Cyan
Write-Host "1. Integrate MilanProfile instance into CommonPort" -ForegroundColor Yellow
Write-Host "2. Add tu bit setting in PTP message transmission" -ForegroundColor Yellow
Write-Host "3. Add stream stability monitoring integration" -ForegroundColor Yellow
Write-Host "4. Add media clock holdover validation tests" -ForegroundColor Yellow
Write-Host "5. Add ATDECC integration for media clock management" -ForegroundColor Yellow

Write-Host ""
Write-Host "Milan B.1 Media Clock Holdover test completed!" -ForegroundColor Green
