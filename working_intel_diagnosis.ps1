Write-Host "=== Intel I210 Hardware Timestamping Diagnosis ===" -ForegroundColor Cyan

# 1. Check for the specific I210 adapter
Write-Host "1. Intel I210 Adapter Detection:" -ForegroundColor Yellow
$i210Adapter = Get-WmiObject -Class Win32_NetworkAdapter | Where-Object { $_.Description -like "*I210*" }
if ($i210Adapter) {
    Write-Host "   Found Intel I210: $($i210Adapter.Description)" -ForegroundColor Green
    Write-Host "     MAC Address: $($i210Adapter.MACAddress)" -ForegroundColor White
    Write-Host "     PNP Device ID: $($i210Adapter.PNPDeviceID)" -ForegroundColor White
    Write-Host "     Net Enabled: $($i210Adapter.NetEnabled)" -ForegroundColor White
} else {
    Write-Host "   No Intel I210 adapter found" -ForegroundColor Red
}

# 2. Check for Intel Network Connections software
Write-Host ""
Write-Host "2. Intel Network Management Software:" -ForegroundColor Yellow
$intelSoftware = Get-ItemProperty "HKLM:\Software\Microsoft\Windows\CurrentVersion\Uninstall\*" | Where-Object { $_.DisplayName -like "*Intel*Network*" -or $_.DisplayName -like "*Intel*PROSet*" }

if ($intelSoftware) {
    Write-Host "   Found Intel Network software:" -ForegroundColor Green
    foreach ($software in $intelSoftware) {
        Write-Host "     - $($software.DisplayName)" -ForegroundColor White
        if ($software.DisplayVersion) {
            Write-Host "       Version: $($software.DisplayVersion)" -ForegroundColor Gray
        }
    }
} else {
    Write-Host "   No Intel Network management software found" -ForegroundColor Red
}

# 3. Check Intel WMI Interface
Write-Host ""
Write-Host "3. Intel WMI Interface Status:" -ForegroundColor Yellow
try {
    $testInstances = Get-WmiObject -Namespace "root\IntelNCS2" -Class "IANet_PhysicalEthernetAdapter" -ErrorAction SilentlyContinue
    if ($testInstances) {
        Write-Host "   Intel WMI adapter instances found: $($testInstances.Count)" -ForegroundColor Green
    } else {
        Write-Host "   No Intel WMI adapter instances found" -ForegroundColor Yellow
    }
} catch {
    Write-Host "   Intel WMI namespace not accessible" -ForegroundColor Red
}

# 4. Final Assessment
Write-Host ""
Write-Host "4. Hardware Timestamping Assessment:" -ForegroundColor Yellow
Write-Host "   Intel I210 chipset: Supports IEEE 1588 hardware timestamping" -ForegroundColor Green
Write-Host "   Windows support: Requires Intel PROSet or specialized drivers" -ForegroundColor Yellow

if (-not $intelSoftware) {
    Write-Host ""
    Write-Host "5. Recommendation:" -ForegroundColor Yellow
    Write-Host "   Install Intel Network Connections software/PROSet for hardware timestamping support" -ForegroundColor Cyan
} else {
    Write-Host ""
    Write-Host "5. Status:" -ForegroundColor Yellow
    Write-Host "   Intel software installed but I210 may not be fully supported for hardware timestamping" -ForegroundColor Yellow
}

Write-Host ""
Write-Host "=== Diagnosis Complete ===" -ForegroundColor Cyan
