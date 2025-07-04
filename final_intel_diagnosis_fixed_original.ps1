Write-Host "=== Intel I210 Hardware Timestamping Diagnosis ===" -ForegroundColor Cyan
Write-Host ""

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
    exit 1
}

# 2. Check driver information
Write-Host ""
Write-Host "2. Driver Information:" -ForegroundColor Yellow
try {
    $pnpDevice = Get-WmiObject -Class Win32_PnPEntity | Where-Object { $_.DeviceID -eq $i210Adapter.PNPDeviceID }
    if ($pnpDevice) {
        Write-Host "   Device Status: $($pnpDevice.Status)" -ForegroundColor White
        
        # Extract driver info from registry  
        $regPath = "HKLM:\SYSTEM\CurrentControlSet\Enum\$($i210Adapter.PNPDeviceID)"
        
        try {
            $driverInfo = Get-ItemProperty -Path $regPath -ErrorAction SilentlyContinue
            if ($driverInfo) {
                Write-Host "   Driver: $($driverInfo.Driver)" -ForegroundColor White
            }
        } catch {
            Write-Host "   Could not read driver info from registry" -ForegroundColor DarkGray
        }
    }
} catch {
    Write-Host "   Error getting driver info: $($_.Exception.Message)" -ForegroundColor Red
}

# 3. Check for Intel Network Connections software
Write-Host ""
Write-Host "3. Intel Network Management Software:" -ForegroundColor Yellow
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

# 4. Check Intel WMI Interface
Write-Host ""
Write-Host "4. Intel WMI Interface Status:" -ForegroundColor Yellow
try {
    $intelNamespace = "root\IntelNCS2"
    $intelClasses = Get-WmiObject -Namespace $intelNamespace -List | Where-Object { $_.Name -like "IANet*Adapter*" }
    
    if ($intelClasses) {
        Write-Host "   Intel WMI namespace accessible" -ForegroundColor Green
        
        $hasInstances = $false
        foreach ($class in $intelClasses) {
            try {
                $instances = Get-WmiObject -Namespace $intelNamespace -Class $class.Name -ErrorAction SilentlyContinue
                if ($instances) {
                    Write-Host "     $($class.Name): $($instances.Count) instances" -ForegroundColor Green
                    $hasInstances = $true
                } else {
                    Write-Host "     $($class.Name): No instances" -ForegroundColor DarkGray
                }
            } catch {
                Write-Host "     $($class.Name): Error accessing" -ForegroundColor Red
            }
        }
        
        if (-not $hasInstances) {
            Write-Host "   Intel WMI classes exist but contain no adapter instances" -ForegroundColor Yellow
        }
    } else {
        Write-Host "   No Intel adapter WMI classes found" -ForegroundColor Red
    }
} catch {
    Write-Host "   Intel WMI namespace not accessible" -ForegroundColor Red
}

# 5. Check for hardware timestamping capability indicators
Write-Host ""
Write-Host "5. Hardware Timestamping Capability Check:" -ForegroundColor Yellow

# Check if the adapter supports advanced features through standard WMI
$adapterConfig = Get-WmiObject -Class Win32_NetworkAdapterConfiguration | Where-Object { $_.Index -eq $i210Adapter.DeviceID }
if ($adapterConfig) {
    Write-Host "   Standard WMI config available" -ForegroundColor White
} else {
    Write-Host "   No standard WMI config found" -ForegroundColor Red
}

# 6. Assessment and Recommendations
Write-Host ""
Write-Host "6. Hardware Timestamping Assessment:" -ForegroundColor Yellow
Write-Host "   Intel I210 Hardware Assessment:" -ForegroundColor White
Write-Host "     • I210 chipset: Supports IEEE 1588 hardware timestamping" -ForegroundColor Green
Write-Host "     • Windows driver: May not expose timestamping APIs" -ForegroundColor Yellow
Write-Host "     • PTP support: Requires Intel PROSet or specialized driver" -ForegroundColor Yellow

Write-Host ""
Write-Host "7. Recommendations:" -ForegroundColor Yellow

if (-not $intelSoftware) {
    Write-Host "   Install Intel Network Connections software/PROSet" -ForegroundColor Cyan
    Write-Host "      - Download from Intel Support website" -ForegroundColor White
    Write-Host "      - Look for 'Intel Ethernet Adapter Complete Driver Pack'" -ForegroundColor White
}

$hasIntelWmiInstances = $false
try {
    $testInstances = Get-WmiObject -Namespace "root\IntelNCS2" -Class "IANet_PhysicalEthernetAdapter" -ErrorAction SilentlyContinue
    $hasIntelWmiInstances = ($null -ne $testInstances)
} catch { 
    $hasIntelWmiInstances = $false
}

if (-not $hasIntelWmiInstances) {
    Write-Host "   Current status: Hardware timestamping likely unavailable" -ForegroundColor Cyan
    Write-Host "      - I210 detected but not managed by Intel WMI interface" -ForegroundColor White
    Write-Host "      - Software timestamping fallback recommended" -ForegroundColor White
    Write-Host "      - Consider different adapter if hardware timestamping required" -ForegroundColor White
}

Write-Host ""
Write-Host "=== Diagnosis Complete ===" -ForegroundColor Cyan
