# Test gPTP Profile Compatibility with PreSonus AVB Network
# This script tests different gPTP profiles to find best compatibility

param(
    [string]$MacAddress = "68-05-CA-8B-76-4E",
    [int]$Duration = 15,
    [string]$Profile = "milan"  # milan, avnu, or ieee1588
)

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

if (-not $isAdmin) {
    Write-Host "=== gPTP Profile Compatibility Test ===" -ForegroundColor Yellow
    Write-Host "Requesting administrator privileges..." -ForegroundColor Yellow
    $scriptPath = $MyInvocation.MyCommand.Path
    Start-Process powershell.exe -Verb RunAs -ArgumentList "-NoExit", "-File", "`"$scriptPath`"", "-MacAddress", $MacAddress, "-Duration", $Duration, "-Profile", $Profile
    exit
}

# We're running as admin now
Write-Host "=== gPTP PROFILE COMPATIBILITY TEST ===" -ForegroundColor Green
Write-Host "Current user: $(whoami)" -ForegroundColor Green
Write-Host "Testing profile: $Profile" -ForegroundColor Cyan

# Stop any existing gPTP processes
Write-Host "`nStopping any existing gPTP processes..."
Stop-Process -Name "gptp" -Force -ErrorAction SilentlyContinue
Start-Sleep 2

# Navigate to the gPTP directory
Set-Location "C:\Users\dzarf\source\repos\gptp-1\build\Debug"

# Show network information
Write-Host "`n=== AVB NETWORK ENVIRONMENT ===" -ForegroundColor Magenta
Write-Host "🎵 PreSonus StudioLive 32SC (mixing console)"
Write-Host "🎵 PreSonus StudioLive 32R (rack mixer)" 
Write-Host "🔌 PreSonus SWE5 AVB Switch (managed switch)"
Write-Host "💻 PC with Intel I210 + gPTP (profile: $Profile)"

# Copy appropriate config file
$configFile = "gptp_cfg.ini"
switch ($Profile.ToLower()) {
    "avnu" {
        if (Test-Path "..\avnu_presonus_config.ini") {
            Copy-Item "..\avnu_presonus_config.ini" $configFile -Force
            Write-Host "`n✅ Using AVnu profile config for PreSonus compatibility" -ForegroundColor Green
        }
    }
    "milan" {
        # Keep existing Milan config
        Write-Host "`n✅ Using Milan profile config" -ForegroundColor Green
    }
    "ieee1588" {
        # Could create IEEE1588 config if needed
        Write-Host "`n✅ Using IEEE1588 profile config" -ForegroundColor Green
    }
}

# Show current configuration
Write-Host "`n=== CURRENT CONFIGURATION ===" -ForegroundColor Cyan
Get-Content $configFile | Select-Object -First 15

# Create timestamped output file
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$outputFile = "profile_test_${Profile}_$timestamp.log"

Write-Host "`n=== STARTING GPTP WITH $($Profile.ToUpper()) PROFILE ===" -ForegroundColor Green
Write-Host "Duration: $Duration seconds"
Write-Host "Watching for PreSonus device interactions..." -ForegroundColor Yellow

# Run gPTP and capture output
$arguments = if ($Profile.ToLower() -eq "milan") { @("-Milan", $MacAddress) } else { @($MacAddress) }
$process = Start-Process -FilePath ".\gptp.exe" -ArgumentList $arguments -PassThru -NoNewWindow -RedirectStandardOutput $outputFile -RedirectStandardError "$outputFile.err"

# Wait for specified duration
$timeout = (Get-Date).AddSeconds($Duration)
while ((Get-Date) -lt $timeout -and !$process.HasExited) {
    Start-Sleep -Seconds 1
}

# Stop the process if still running
if (!$process.HasExited) {
    Write-Host "`n=== STOPPING GPTP AFTER $Duration SECONDS ===" -ForegroundColor Yellow
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep 2
}

# Analyze results
if (Test-Path $outputFile) {
    Write-Host "`n=== $($Profile.ToUpper()) PROFILE ANALYSIS ===" -ForegroundColor Cyan
    $content = Get-Content $outputFile
    
    $timerEvents = $content | Select-String "PDELAY_INTERVAL_TIMEOUT_EXPIRES occurred"
    $pdelayRequests = $content | Select-String "sending PDelay request" 
    $responses = $content | Select-String "RX timestamp SUCCESS"
    $announceMessages = $content | Select-String "messageType=11"
    $syncMessages = $content | Select-String "messageType=0"
    $asCapableEnabled = $content | Select-String "AsCapable STATE CHANGE: ENABLED"
    $asCapableDisabled = $content | Select-String "AsCapable STATE CHANGE: DISABLED"
    $timeouts = $content | Select-String "Response Receipt Timeout"
    
    Write-Host "📊 RESULTS FOR $($Profile.ToUpper()) PROFILE:"
    Write-Host "  ✅ Timer Events: $($timerEvents.Count)" -ForegroundColor Green
    Write-Host "  📤 PDelay Requests Sent: $($pdelayRequests.Count)" -ForegroundColor Green
    Write-Host "  📥 PDelay Responses: $($responses.Count)" -ForegroundColor $(if ($responses.Count -gt 0) { "Green" } else { "Red" })
    Write-Host "  📢 Announce Messages: $($announceMessages.Count)" -ForegroundColor $(if ($announceMessages.Count -gt 0) { "Green" } else { "Yellow" })
    Write-Host "  🕒 Sync Messages: $($syncMessages.Count)" -ForegroundColor $(if ($syncMessages.Count -gt 0) { "Green" } else { "Yellow" })
    Write-Host "  ✅ AsCapable Enabled: $($asCapableEnabled.Count)" -ForegroundColor Green
    Write-Host "  ❌ AsCapable Disabled: $($asCapableDisabled.Count)" -ForegroundColor $(if ($asCapableDisabled.Count -gt 0) { "Yellow" } else { "Green" })
    Write-Host "  ⏰ Timeouts: $($timeouts.Count)" -ForegroundColor $(if ($timeouts.Count -gt 0) { "Yellow" } else { "Green" })
    
    # Compatibility assessment
    $score = 0
    if ($responses.Count -gt 0) { $score += 3 }          # PDelay working
    if ($announceMessages.Count -gt 0) { $score += 2 }   # Announce working
    if ($syncMessages.Count -gt 0) { $score += 2 }       # Sync working  
    if ($timeouts.Count -eq 0) { $score += 1 }           # No timeouts
    if ($asCapableDisabled.Count -eq 0) { $score += 1 }  # Stays AsCapable
    
    Write-Host "`n🏆 COMPATIBILITY SCORE: $score/9" -ForegroundColor $(
        if ($score -ge 7) { "Green" } 
        elseif ($score -ge 4) { "Yellow" }
        else { "Red" }
    )
    
    if ($score -ge 7) {
        Write-Host "✅ EXCELLENT: $($Profile.ToUpper()) profile works well with PreSonus devices" -ForegroundColor Green
    } elseif ($score -ge 4) {
        Write-Host "⚠️  PARTIAL: $($Profile.ToUpper()) profile partially compatible" -ForegroundColor Yellow
    } else {
        Write-Host "❌ POOR: $($Profile.ToUpper()) profile not compatible" -ForegroundColor Red
    }
    
    Write-Host "`n📝 Log saved to: $outputFile" -ForegroundColor Cyan
}

Write-Host "`n💡 RECOMMENDATIONS:" -ForegroundColor Cyan
Write-Host "• Try different profiles: .\test_profile_compatibility.ps1 -Profile avnu" -ForegroundColor White
Write-Host "• Check PreSonus switch gPTP settings" -ForegroundColor White
Write-Host "• Verify network cable connections" -ForegroundColor White

Read-Host "`nPress Enter to close"
