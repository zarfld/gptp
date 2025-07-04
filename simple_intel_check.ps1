# Simple Intel PROSet check
Write-Host "=== Intel PROSet Analysis ===" -ForegroundColor Cyan

# Get I210 adapter
$i210Adapter = Get-WmiObject -Class "Win32_NetworkAdapter" | Where-Object { 
    $_.Description -like "*I210*" -or $_.Name -like "*I210*" 
}

if ($i210Adapter) {
    Write-Host "`nI210 Adapter Found:" -ForegroundColor Green
    Write-Host "  Name: $($i210Adapter.Name)"
    Write-Host "  Description: $($i210Adapter.Description)"
    Write-Host "  MAC: $($i210Adapter.MACAddress)"
    Write-Host "  Driver Service: $($i210Adapter.ServiceName)"
    Write-Host "  PNP Device ID: $($i210Adapter.PNPDeviceID)"
    
    # Check driver service details
    if ($i210Adapter.ServiceName) {
        $driverService = Get-Service -Name $i210Adapter.ServiceName -ErrorAction SilentlyContinue
        if ($driverService) {
            Write-Host "  Driver Status: $($driverService.Status)"
        }
        
        # Check if it's Intel driver
        $sysDriver = Get-WmiObject -Class "Win32_SystemDriver" | Where-Object { $_.Name -eq $i210Adapter.ServiceName }
        if ($sysDriver) {
            Write-Host "  Driver Path: $($sysDriver.PathName)"
            if ($sysDriver.PathName -like "*intel*" -or $sysDriver.PathName -like "*e1r*") {
                Write-Host "  Using Intel driver: YES" -ForegroundColor Green
            } else {
                Write-Host "  Using Intel driver: NO" -ForegroundColor Red
            }
        }
    }
} else {
    Write-Host "I210 adapter not found" -ForegroundColor Red
}

# Check Intel WMI namespace
Write-Host "`n=== Intel WMI Status ===" -ForegroundColor Cyan
try {
    $intelAdapters = Get-WmiObject -Namespace "root\IntelNCS2" -Class "IANet_PhysicalEthernetAdapter" -ErrorAction Stop
    if ($intelAdapters) {
        Write-Host "Intel WMI adapters found: $($intelAdapters.Count)" -ForegroundColor Green
    } else {
        Write-Host "Intel WMI namespace accessible but no adapters found" -ForegroundColor Yellow
    }
} catch {
    Write-Host "Intel WMI namespace not accessible: $($_.Exception.Message)" -ForegroundColor Red
}

# Check Intel software
Write-Host "`n=== Intel Software ===" -ForegroundColor Cyan
$intelSoftware = Get-WmiObject -Class "Win32_Product" | Where-Object { 
    $_.Name -like "*Intel*Network*" -or $_.Name -like "*PROSet*" 
}
if ($intelSoftware) {
    foreach ($software in $intelSoftware) {
        Write-Host "  $($software.Name) - $($software.Version)" -ForegroundColor Green
    }
} else {
    Write-Host "  No Intel network software found via WMI" -ForegroundColor Yellow
}

Write-Host "`n=== Summary ===" -ForegroundColor Cyan
Write-Host "The I210 adapter is present but may not be using Intel's advanced drivers."
Write-Host "This explains why Intel OID requests are failing in gPTP."
Write-Host "Consider using software timestamping or Windows standard APIs instead."
