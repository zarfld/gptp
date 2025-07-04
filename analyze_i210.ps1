# Intel I210 Adapter Deep Analysis Script
Write-Host "=== Intel I210 Adapter Deep Analysis ===" -ForegroundColor Green

# Get the I210 adapter
$i210 = Get-WmiObject -Class Win32_NetworkAdapter | Where-Object { $_.Description -like "*I210*" }
if (-not $i210) {
    Write-Host "I210 adapter not found!" -ForegroundColor Red
    exit 1
}

Write-Host "Found I210 adapter: $($i210.Name)" -ForegroundColor Yellow
Write-Host "MAC Address: $($i210.MACAddress)" -ForegroundColor White
Write-Host "PNP Device ID: $($i210.PNPDeviceID)" -ForegroundColor White

# Get adapter configuration
Write-Host ""
Write-Host "=== Adapter Configuration ===" -ForegroundColor Green
$config = Get-WmiObject -Class Win32_NetworkAdapterConfiguration | Where-Object { $_.Index -eq $i210.DeviceID }
if ($config) {
    Write-Host "IP Enabled: $($config.IPEnabled)" -ForegroundColor White
    Write-Host "DHCP Enabled: $($config.DHCPEnabled)" -ForegroundColor White
    Write-Host "IP Address: $($config.IPAddress)" -ForegroundColor White
    Write-Host "MAC Address: $($config.MACAddress)" -ForegroundColor White
}

# Check registry for driver settings
Write-Host ""
Write-Host "=== Registry Driver Settings ===" -ForegroundColor Green
$pnpId = $i210.PNPDeviceID
$regPath = "HKLM:\SYSTEM\CurrentControlSet\Enum\$pnpId"

try {
    if (Test-Path $regPath) {
        Write-Host "Found registry path: $regPath" -ForegroundColor Green
        
        # Check device parameters
        $deviceParams = "$regPath\Device Parameters"
        if (Test-Path $deviceParams) {
            Write-Host ""
            Write-Host "Device Parameters:" -ForegroundColor Cyan
            $params = Get-ItemProperty -Path $deviceParams -ErrorAction SilentlyContinue
            $params.PSObject.Properties | Where-Object { $_.Name -notlike "PS*" } | ForEach-Object {
                Write-Host "  $($_.Name): $($_.Value)" -ForegroundColor White
            }
        }
        
        # Check driver key path
        $driverKey = Get-ItemProperty -Path $regPath -Name "Driver" -ErrorAction SilentlyContinue
        if ($driverKey) {
            $driverPath = "HKLM:\SYSTEM\CurrentControlSet\Control\Class\$($driverKey.Driver)"
            Write-Host ""
            Write-Host "Driver Registry Path: $driverPath" -ForegroundColor Cyan
            
            if (Test-Path $driverPath) {
                $driverProps = Get-ItemProperty -Path $driverPath -ErrorAction SilentlyContinue
                
                # Look for PTP/Timestamping related settings
                $ptpSettings = $driverProps.PSObject.Properties | Where-Object { 
                    $_.Name -like "*PTP*" -or $_.Name -like "*Time*" -or $_.Name -like "*Clock*" -or 
                    $_.Name -like "*Stamp*" -or $_.Name -like "*Hardware*" -or $_.Name -like "*Offload*" 
                }
                
                if ($ptpSettings) {
                    Write-Host ""
                    Write-Host "PTP/Timestamping related settings:" -ForegroundColor Yellow
                    foreach ($setting in $ptpSettings) {
                        Write-Host "  $($setting.Name): $($setting.Value)" -ForegroundColor White
                    }
                } else {
                    Write-Host "  No PTP/Timestamping settings found" -ForegroundColor Gray
                }
                
                # Show all driver settings for reference
                Write-Host ""
                Write-Host "All driver settings:" -ForegroundColor Yellow
                $allSettings = $driverProps.PSObject.Properties | Where-Object { $_.Name -notlike "PS*" } | Sort-Object Name
                foreach ($setting in $allSettings) {
                    Write-Host "  $($setting.Name): $($setting.Value)" -ForegroundColor White
                }
            }
        }
    } else {
        Write-Host "Registry path not found: $regPath" -ForegroundColor Red
    }
} catch {
    Write-Host "Error accessing registry: $($_.Exception.Message)" -ForegroundColor Red
}

# Check device manager properties using DevCon-style queries
Write-Host ""
Write-Host "=== Device Properties ===" -ForegroundColor Green
try {
    $device = Get-WmiObject -Class Win32_PnPEntity | Where-Object { $_.DeviceID -eq $pnpId }
    if ($device) {
        Write-Host "Device Name: $($device.Name)" -ForegroundColor White
        Write-Host "Device Status: $($device.Status)" -ForegroundColor White
        Write-Host "Hardware ID: $($device.HardwareID)" -ForegroundColor White
        Write-Host "Compatible IDs: $($device.CompatibleID)" -ForegroundColor White
        Write-Host "Service: $($device.Service)" -ForegroundColor White
    }
} catch {
    Write-Host "Error getting device properties: $($_.Exception.Message)" -ForegroundColor Red
}

# Check for Intel network service
Write-Host ""
Write-Host "=== Intel Network Services ===" -ForegroundColor Green
$intelServices = Get-Service | Where-Object { 
    $_.Name -like "*Intel*" -or $_.DisplayName -like "*Intel*" 
} | Where-Object {
    $_.Name -like "*Net*" -or $_.DisplayName -like "*Net*" -or 
    $_.Name -like "*Adapter*" -or $_.DisplayName -like "*Adapter*" -or
    $_.Name -like "*PROSet*" -or $_.DisplayName -like "*PROSet*"
}

if ($intelServices) {
    foreach ($service in $intelServices) {
        Write-Host "Service: $($service.DisplayName) ($($service.Name))" -ForegroundColor Green
        Write-Host "  Status: $($service.Status)" -ForegroundColor White
        Write-Host "  StartType: $($service.StartType)" -ForegroundColor White
    }
} else {
    Write-Host "No Intel network services found" -ForegroundColor Gray
}

# Check driver files
Write-Host ""
Write-Host "=== Driver Files ===" -ForegroundColor Green
try {
    $drivers = Get-WmiObject -Class Win32_SystemDriver | Where-Object { 
        $_.Name -like "*e1i*" -or $_.Name -like "*e1d*" -or $_.Name -like "*igb*" -or 
        $_.DisplayName -like "*Intel*" 
    }
    
    if ($drivers) {
        foreach ($driver in $drivers) {
            Write-Host "Driver: $($driver.DisplayName) ($($driver.Name))" -ForegroundColor Green
            Write-Host "  Path: $($driver.PathName)" -ForegroundColor White
            Write-Host "  State: $($driver.State)" -ForegroundColor White
            Write-Host "  Start Mode: $($driver.StartMode)" -ForegroundColor White
        }
    } else {
        Write-Host "No Intel network drivers found" -ForegroundColor Gray
    }
} catch {
    Write-Host "Error querying drivers: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Analysis Complete ===" -ForegroundColor Green
