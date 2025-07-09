#!/usr/bin/env powershell
# Intel I225 Hardware Detection and Validation Script
# Based on analysis and HAL improvements for OpenAvnu
# Author: GitHub Copilot (2025-01-19)

Write-Host "=== Intel I225 Hardware Detection Script ===" -ForegroundColor Cyan
Write-Host "This script detects I225 hardware and analyzes stepping versions" -ForegroundColor Green
Write-Host "WARNING: This script does NOT validate OpenAvnu functionality" -ForegroundColor Red

# Your specific hardware PCI ID
$TARGET_PCI_ID = "PCI\VEN_8086&DEV_15F3&SUBSYS_00008086&REV_03"

Write-Host "`n1. Detecting Intel I225 Network Adapters..." -ForegroundColor Yellow

# Get all Intel network adapters
$IntelAdapters = Get-WmiObject Win32_NetworkAdapter | Where-Object {
    $_.PNPDeviceID -like "*VEN_8086*" -and
    $_.PNPDeviceID -like "*DEV_15F*"
}

if ($IntelAdapters.Count -eq 0) {
    Write-Host "   No Intel I225 adapters found" -ForegroundColor Red
    Write-Host "   Expected device: $TARGET_PCI_ID" -ForegroundColor Red
    exit 1
}

foreach ($adapter in $IntelAdapters) {
    Write-Host "`n--- Intel I225 Adapter Found ---" -ForegroundColor Green
    Write-Host "   Name: $($adapter.Name)" -ForegroundColor White
    Write-Host "   Description: $($adapter.Description)" -ForegroundColor White
    Write-Host "   PCI ID: $($adapter.PNPDeviceID)" -ForegroundColor White
    Write-Host "   Status: $($adapter.NetConnectionStatus)" -ForegroundColor White
    
    # Parse PCI ID components
    if ($adapter.PNPDeviceID -match "VEN_([0-9A-Fa-f]{4})&DEV_([0-9A-Fa-f]{4})&SUBSYS_([0-9A-Fa-f]{8})&REV_([0-9A-Fa-f]{2})") {
        $VendorID = $matches[1]
        $DeviceID = $matches[2]
        $SubsystemID = $matches[3]
        $RevisionID = $matches[4]
        
        Write-Host "   Vendor ID: 0x$VendorID (Intel)" -ForegroundColor Cyan
        Write-Host "   Device ID: 0x$DeviceID" -ForegroundColor Cyan
        Write-Host "   Subsystem ID: 0x$SubsystemID" -ForegroundColor Cyan
        Write-Host "   Revision ID: 0x$RevisionID" -ForegroundColor Cyan
        
        # Determine specific I225 variant
        switch ($DeviceID.ToUpper()) {
            "15F2" { $Variant = "I225-LM (LAN on Motherboard)" }
            "15F3" { $Variant = "I225-V (Standalone)" }
            "0D9F" { $Variant = "I225-IT (Industrial)" }
            "3100" { $Variant = "I225-K (Embedded)" }
            "125B" { $Variant = "I226-LM (Next Gen LM)" }
            "125C" { $Variant = "I226-V (Next Gen V)" }
            "125D" { $Variant = "I226-IT (Next Gen IT)" }
            default { $Variant = "Unknown I225 variant" }
        }
        
        Write-Host "   I225 Variant: $Variant" -ForegroundColor Green
        
        # Determine stepping from revision ID
        $SteppingValue = [int]("0x" + $RevisionID)
        $SteppingLetter = switch ($SteppingValue) {
            0x00 { "A0" }
            0x01 { "A1" }
            0x02 { "A2" }
            0x03 { "A3" }
            default { "Unknown ($SteppingValue)" }
        }
        
        Write-Host "   Hardware Stepping: $SteppingLetter" -ForegroundColor Cyan
        
        # Check for IPG timing issues
        $HasIPGIssue = $SteppingValue -le 0x01
        
        if ($HasIPGIssue) {
            Write-Host "   IPG Issue: YES - Critical timing issue detected" -ForegroundColor Red
            Write-Host "   Mitigation Required: Speed limit to 1Gbps" -ForegroundColor Red
        } else {
            Write-Host "   IPG Issue: NO - Production stepping" -ForegroundColor Green
            Write-Host "   Full 2.5GbE Support: Available" -ForegroundColor Green
        }
        
        # Check if this matches your hardware
        if ($adapter.PNPDeviceID -eq $TARGET_PCI_ID) {
            Write-Host "`n   *** THIS IS YOUR HARDWARE ***" -ForegroundColor Magenta
            Write-Host "   Your I225-V (0x15F3) with stepping A3 (0x03)" -ForegroundColor Magenta
            Write-Host "   Status: Production stepping - no IPG mitigation needed" -ForegroundColor Green
            Write-Host "   Supports: Full 2.5GbE with hardware timestamping" -ForegroundColor Green
        }
    }
}

Write-Host "`n2. Checking Driver Information..." -ForegroundColor Yellow

# Get driver information
$DriverInfo = Get-WmiObject Win32_PnPEntity | Where-Object {
    $_.PNPDeviceID -like "*VEN_8086&DEV_15F*"
}

foreach ($driver in $DriverInfo) {
    Write-Host "`n--- Driver Information ---" -ForegroundColor Green
    Write-Host "   Driver Name: $($driver.Name)" -ForegroundColor White
    Write-Host "   Driver Version: $($driver.DriverVersion)" -ForegroundColor White
    Write-Host "   Driver Date: $($driver.DriverDate)" -ForegroundColor White
    Write-Host "   Hardware ID: $($driver.PNPDeviceID)" -ForegroundColor White
}

Write-Host "`n3. Registry Analysis..." -ForegroundColor Yellow

# Check for Intel driver registry entries
$RegistryPaths = @(
    "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4D36E972-E325-11CE-BFC1-08002BE10318}",
    "HKLM:\SOFTWARE\Intel\NCS2\CurrentVersion\Advanced"
)

foreach ($path in $RegistryPaths) {
    if (Test-Path $path) {
        Write-Host "   Registry Path: $path" -ForegroundColor Cyan
        
        # Look for Intel I225 specific entries
        $subkeys = Get-ChildItem $path -ErrorAction SilentlyContinue
        foreach ($subkey in $subkeys) {
            $props = Get-ItemProperty $subkey.PSPath -ErrorAction SilentlyContinue
            if ($props -and $props.DriverDesc -like "*I225*") {
                Write-Host "   Found I225 Driver Entry:" -ForegroundColor Green
                Write-Host "     Description: $($props.DriverDesc)" -ForegroundColor White
                Write-Host "     Version: $($props.DriverVersion)" -ForegroundColor White
                
                # Check for PTP-related parameters
                $PtpParams = @("*PtpHardwareTimestamp", "*IEEE1588", "*HardwareTimestamp")
                foreach ($param in $PtpParams) {
                    if ($null -ne $props.$param) {
                        Write-Host "     $param`: $($props.$param)" -ForegroundColor Cyan
                    }
                }
            }
        }
    }
}

Write-Host "`n4. Network Configuration Analysis..." -ForegroundColor Yellow

# Get network adapter configuration
$NetAdapters = Get-NetAdapter | Where-Object {
    $_.InterfaceDescription -like "*I225*"
}

foreach ($adapter in $NetAdapters) {
    Write-Host "`n--- Network Adapter: $($adapter.Name) ---" -ForegroundColor Green
    Write-Host "   Description: $($adapter.InterfaceDescription)" -ForegroundColor White
    Write-Host "   Status: $($adapter.Status)" -ForegroundColor White
    Write-Host "   Link Speed: $($adapter.LinkSpeed)" -ForegroundColor White
    Write-Host "   Media Type: $($adapter.MediaType)" -ForegroundColor White
    
    # Get advanced properties
    $AdvancedProps = Get-NetAdapterAdvancedProperty -Name $adapter.Name -ErrorAction SilentlyContinue
    
    if ($AdvancedProps) {
        Write-Host "   Advanced Properties:" -ForegroundColor Cyan
        foreach ($prop in $AdvancedProps) {
            if ($prop.DisplayName -like "*PTP*" -or 
                $prop.DisplayName -like "*1588*" -or 
                $prop.DisplayName -like "*Timestamp*" -or
                $prop.DisplayName -like "*Speed*") {
                Write-Host "     $($prop.DisplayName): $($prop.DisplayValue)" -ForegroundColor White
            }
        }
    }
}

Write-Host "`n5. HAL Integration Test..." -ForegroundColor Yellow

# Simulate HAL detection logic
$TestDeviceDesc = "Intel(R) Ethernet Controller I225-V"
$TestPciDeviceId = 0x15F3
$TestPciRevision = 0x03

Write-Host "   Testing HAL Logic:" -ForegroundColor Cyan
Write-Host "     Device Description: $TestDeviceDesc" -ForegroundColor White
Write-Host "     PCI Device ID: 0x$($TestPciDeviceId.ToString('X4'))" -ForegroundColor White
Write-Host "     PCI Revision: 0x$($TestPciRevision.ToString('X2'))" -ForegroundColor White

# Simulate device detection
$IsI225 = $TestDeviceDesc -like "*I225*"
$ClockRate = if ($IsI225) { 200000000 } else { 125000000 }
$HwTimestamp = $true
$SteppingId = $TestPciRevision -band 0x0F
$RequiresMitigation = $SteppingId -le 0x01

Write-Host "   HAL Results:" -ForegroundColor Green
Write-Host "     Is I225 Family: $IsI225" -ForegroundColor White
Write-Host "     Clock Rate: $ClockRate Hz (200 MHz)" -ForegroundColor White
Write-Host "     Hardware Timestamp: $HwTimestamp" -ForegroundColor White
Write-Host "     Stepping ID: $SteppingId (A3)" -ForegroundColor White
Write-Host "     Requires IPG Mitigation: $RequiresMitigation" -ForegroundColor White

Write-Host "`n6. IMPORTANT WARNINGS..." -ForegroundColor Yellow

Write-Host "   For your hardware ($TARGET_PCI_ID):" -ForegroundColor Green
Write-Host "   - I225-V controller with A3 stepping detected" -ForegroundColor Green
Write-Host "   - Based on Intel docs: No IPG timing issues expected" -ForegroundColor Green
Write-Host "   - Theoretical: Hardware timestamping capability (200 MHz clock)" -ForegroundColor Green
Write-Host "   - OpenAvnu functionality: UNTESTED AND UNVALIDATED" -ForegroundColor Red

Write-Host "`n   Critical Requirements:" -ForegroundColor Red
Write-Host "   1. Compile OpenAvnu with updated HAL code" -ForegroundColor White
Write-Host "   2. Test basic network functionality" -ForegroundColor White
Write-Host "   3. Validate gPTP synchronization on actual hardware" -ForegroundColor White
Write-Host "   4. Test AVB streaming performance" -ForegroundColor White
Write-Host "   5. Validate all theoretical assumptions through testing" -ForegroundColor White

Write-Host "`n=== HARDWARE DETECTED - FUNCTIONALITY TESTING REQUIRED ===" -ForegroundColor Red
