# Intel PTP Hardware Timestamp Configuration Script
# Based on Intel Community Support instructions
# https://community.intel.com/t5/Ethernet-Products/Hardware-Timestamp-on-windows/m-p/1496441#M33563

param(
    [Parameter(Mandatory=$false)]
    [string]$AdapterName = "AVB",
    [Parameter(Mandatory=$false)]
    [string]$SoftwareTimestampMode = "RxAll & TxAll"
)

Write-Host "Intel PTP Hardware Timestamp Configuration Script" -ForegroundColor Green
Write-Host "=================================================" -ForegroundColor Green

# Check if Intel PROSet PowerShell module is available (Windows 10 only)
$intelModule = Get-Module -ListAvailable -Name "IntelNetAdapterModule" -ErrorAction SilentlyContinue

if ($intelModule) {
    Write-Host "Intel PROSet PowerShell module detected - using Set-IntelNetAdapterSetting" -ForegroundColor Yellow
    
    try {
        # Enable PTP Hardware Timestamp
        Write-Host "Enabling PTP Hardware Timestamp for adapter: $AdapterName"
        Set-IntelNetAdapterSetting -Name $AdapterName -DisplayName "PTP Hardware Timestamp" -DisplayValue "Enabled"
        
        # Enable Software Timestamp
        Write-Host "Setting Software Timestamp to: $SoftwareTimestampMode"
        Set-IntelNetAdapterSetting -Name $AdapterName -DisplayName "Software Timestamp" -DisplayValue $SoftwareTimestampMode
        
        Write-Host "Intel PROSet configuration completed successfully!" -ForegroundColor Green
    }
    catch {
        Write-Error "Failed to configure using Intel PROSet: $($_.Exception.Message)"
        $useFallback = $true
    }
}
else {
    Write-Host "Intel PROSet not available - using registry/netsh fallback methods" -ForegroundColor Yellow
    $useFallback = $true
}

# Fallback method for Windows 11 or when Intel PROSet is not available
if ($useFallback) {
    Write-Host "Using alternative configuration methods..." -ForegroundColor Cyan
    
    # Method 1: Use Set-NetAdapterAdvancedProperty (built-in Windows command)
    try {
        Write-Host "Attempting to use Set-NetAdapterAdvancedProperty..."
        
        # Enable PTP Hardware Timestamp
        Set-NetAdapterAdvancedProperty -Name $AdapterName -RegistryKeyword "*PtpHardwareTimestamp" -RegistryValue 1 -ErrorAction Stop
        Write-Host "✓ PTP Hardware Timestamp enabled via Set-NetAdapterAdvancedProperty"
        
        # Set Software Timestamp (map friendly names to registry values)
        $timestampValue = switch ($SoftwareTimestampMode) {
            "Disabled" { 0 }
            "RxAll" { 1 }
            "TxAll" { 2 }
            "RxAll & TxAll" { 3 }
            "TaggedTx" { 4 }
            "RxAll & TaggedTx" { 5 }
            default { 3 } # Default to RxAll & TxAll
        }
        
        Set-NetAdapterAdvancedProperty -Name $AdapterName -RegistryKeyword "*SoftwareTimestamp" -RegistryValue $timestampValue -ErrorAction Stop
        Write-Host "✓ Software Timestamp set to $SoftwareTimestampMode via Set-NetAdapterAdvancedProperty"
        
        Write-Host "Alternative configuration method completed successfully!" -ForegroundColor Green
    }
    catch {
        Write-Warning "Set-NetAdapterAdvancedProperty failed: $($_.Exception.Message)"
        
        # Method 2: Direct registry modification (requires admin privileges)
        Write-Host "Attempting direct registry configuration..." -ForegroundColor Cyan
        
        try {
            # Find the network adapter in registry
            $netClass = "HKLM:\SYSTEM\CurrentControlSet\Control\Class\{4d36e972-e325-11ce-bfc1-08002be10318}"
            $adapterKeys = Get-ChildItem $netClass | Where-Object { $_.PSChildName -match "^\d{4}$" }
            
            foreach ($key in $adapterKeys) {
                $driverDesc = Get-ItemProperty -Path $key.PSPath -Name "DriverDesc" -ErrorAction SilentlyContinue
                if ($driverDesc -and $driverDesc.DriverDesc -like "*I210*") {
                    Write-Host "Found Intel I210 adapter in registry: $($key.PSPath)"
                    
                    # Set PTP Hardware Timestamp
                    Set-ItemProperty -Path $key.PSPath -Name "*PtpHardwareTimestamp" -Value "1" -Type String
                    Write-Host "✓ PTP Hardware Timestamp enabled via registry"
                    
                    # Set Software Timestamp  
                    Set-ItemProperty -Path $key.PSPath -Name "*SoftwareTimestamp" -Value $timestampValue.ToString() -Type String
                    Write-Host "✓ Software Timestamp configured via registry"
                    
                    break
                }
            }
            
            Write-Host "Registry configuration completed - restart may be required!" -ForegroundColor Green
        }
        catch {
            Write-Error "Registry configuration failed: $($_.Exception.Message)"
            Write-Host "Manual configuration may be required via Device Manager" -ForegroundColor Red
        }
    }
}

# Verify current settings
Write-Host "`nVerifying current configuration..." -ForegroundColor Cyan
try {
    $properties = Get-NetAdapterAdvancedProperty -Name $AdapterName | Where-Object { 
        $_.RegistryKeyword -eq "*PtpHardwareTimestamp" -or $_.RegistryKeyword -eq "*SoftwareTimestamp" 
    }
    
    foreach ($prop in $properties) {
        $value = if ($prop.RegistryKeyword -eq "*PtpHardwareTimestamp") {
            if ($prop.RegistryValue -eq "1") { "Enabled" } else { "Disabled" }
        } else {
            switch ($prop.RegistryValue) {
                "0" { "Disabled" }
                "1" { "RxAll" }
                "2" { "TxAll" }
                "3" { "RxAll & TxAll" }
                "4" { "TaggedTx" }
                "5" { "RxAll & TaggedTx" }
                default { "Unknown ($($prop.RegistryValue))" }
            }
        }
        Write-Host "$($prop.RegistryKeyword): $value" -ForegroundColor White
    }
}
catch {
    Write-Warning "Could not verify settings: $($_.Exception.Message)"
}

Write-Host "`nConfiguration script completed!" -ForegroundColor Green
Write-Host "Note: A system restart or adapter reset may be required for changes to take effect." -ForegroundColor Yellow
