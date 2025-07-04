# Intel PROSet Integration Analysis
# Check if we can enable Intel PROSet management for the I210

Write-Host "=== Intel PROSet Integration Analysis ===" -ForegroundColor Cyan

# Our I210 details from previous analysis
$i210MacAddress = "68:05:CA:8B:76:4E"  # Actual MAC from WMI query
$i210PnpId = "PCI\VEN_8086&DEV_1533&SUBSYS_0003103C&REV_03\4&3ED1CE&0&0008"

Write-Host "`nTarget I210 Details:" -ForegroundColor Yellow
Write-Host "  MAC Address: $i210MacAddress"
Write-Host "  PNP Device ID: $i210PnpId"

# 1. Check current driver details for the I210
Write-Host "`n=== Current I210 Driver Analysis ===" -ForegroundColor Cyan

$i210Adapter = Get-WmiObject -Class "Win32_NetworkAdapter" | Where-Object { $_.MACAddress -eq $i210MacAddress }
if ($i210Adapter) {
    Write-Host "I210 Adapter Found:" -ForegroundColor Green
    Write-Host "  Name: $($i210Adapter.Name)"
    Write-Host "  Description: $($i210Adapter.Description)"
    Write-Host "  Driver: $($i210Adapter.ServiceName)"
    Write-Host "  Net Connection ID: $($i210Adapter.NetConnectionID)"
    
    # Get driver details
    $driverService = Get-WmiObject -Class "Win32_SystemDriver" | Where-Object { $_.Name -eq $i210Adapter.ServiceName }
    if ($driverService) {
        Write-Host "  Driver Path: $($driverService.PathName)"
        Write-Host "  Driver State: $($driverService.State)"
        Write-Host "  Driver Start Mode: $($driverService.StartMode)"
    }
    
    # Check device manager info
    $pnpDevice = Get-WmiObject -Class "Win32_PnPEntity" | Where-Object { $_.DeviceID -eq $i210Adapter.PNPDeviceID }
    if ($pnpDevice) {
        Write-Host "  Device Status: $($pnpDevice.Status)"
        Write-Host "  Hardware IDs: $($pnpDevice.HardwareID -join ', ')"
        Write-Host "  Compatible IDs: $($pnpDevice.CompatibleID -join ', ')"
    }
    
    # Check network adapter configuration
    $networkConfig = Get-WmiObject -Class "Win32_NetworkAdapterConfiguration" | Where-Object { $_.Index -eq $i210Adapter.Index }
    if ($networkConfig) {
        Write-Host "  IP Enabled: $($networkConfig.IPEnabled)"
        Write-Host "  DHCP Enabled: $($networkConfig.DHCPEnabled)"
        Write-Host "  DNS Domain: $($networkConfig.DNSDomain)"
    }
}

# 2. Check Intel PROSet installation and configuration
Write-Host "`n=== Intel PROSet Installation Check ===" -ForegroundColor Cyan

# Check installed software
$intelSoftware = Get-WmiObject -Class "Win32_Product" | Where-Object { $_.Name -like "*Intel*" -and $_.Name -like "*Network*" }
if ($intelSoftware) {
    foreach ($software in $intelSoftware) {
        Write-Host "Intel Software: $($software.Name) - Version: $($software.Version)"
    }
}

# Check Intel services
$intelServices = Get-Service | Where-Object { $_.ServiceName -like "*Intel*" -or $_.DisplayName -like "*Intel*" }
if ($intelServices) {
    Write-Host "`nIntel Services:" -ForegroundColor Yellow
    foreach ($service in $intelServices) {
        Write-Host "  $($service.DisplayName) ($($service.ServiceName)): $($service.Status)"
    }
}

# Check for Intel registry keys
$intelRegPaths = @(
    "HKLM:\SOFTWARE\Intel\NCS2",
    "HKLM:\SOFTWARE\Intel\PROSet",
    "HKLM:\SOFTWARE\Intel\NetworkConnections",
    "HKLM:\SOFTWARE\Intel\Intel(R) PROSet",
    "HKLM:\SYSTEM\CurrentControlSet\Services\e1rexpress"
)

foreach ($regPath in $intelRegPaths) {
    if (Test-Path $regPath) {
        Write-Host "`nFound registry path: $regPath" -ForegroundColor Green
        try {
            $regItems = Get-ItemProperty $regPath -ErrorAction SilentlyContinue
            if ($regItems) {
                $regItems.PSObject.Properties | Where-Object { $_.Name -notlike "PS*" } | ForEach-Object {
                    Write-Host "  $($_.Name): $($_.Value)"
                }
            }
        }
        catch {
            Write-Host "  Could not read registry: $($_.Exception.Message)"
        }
    }
}

# 3. Check if adapter can be managed by Intel PROSet
Write-Host "`n=== Intel PROSet Compatibility Check ===" -ForegroundColor Cyan

# Check device vendor/device IDs
$deviceVendorId = "8086"  # Intel
$deviceId = "1533"        # I210
$subsystemId = "0003103C" # HP subsystem

Write-Host "Device identifiers:" -ForegroundColor Yellow
Write-Host "  Vendor ID: $deviceVendorId (Intel)"
Write-Host "  Device ID: $deviceId (I210)"
Write-Host "  Subsystem ID: $subsystemId (HP)"

# Check if this device ID is supported by Intel PROSet
$intelSupportedDevices = @(
    "1533",  # I210
    "1539",  # I211
    "15A0",  # I218
    "15A1",  # I218
    "15A2",  # I218
    "15A3",  # I218
    "1502",  # I82579LM
    "1503",  # I82579V
    "10D3",  # 82574L
    "10F5",  # 82567LM
    "294C"   # 82566DC-2
)

if ($intelSupportedDevices -contains $deviceId) {
    Write-Host "✓ I210 (Device ID: $deviceId) is supported by Intel PROSet" -ForegroundColor Green
} else {
    Write-Host "✗ I210 (Device ID: $deviceId) may not be supported by Intel PROSet" -ForegroundColor Red
}

# 4. Check current driver vs Intel PROSet driver
Write-Host "`n=== Driver Comparison ===" -ForegroundColor Cyan

# Get current driver file info
$currentDriverService = $i210Adapter.ServiceName
Write-Host "Current driver service: $currentDriverService"

# Check if Intel PROSet driver is available
$intelDriverPath = "C:\Windows\System32\drivers\e1r68x64.sys"
if (Test-Path $intelDriverPath) {
    Write-Host "Intel PROSet driver found: $intelDriverPath" -ForegroundColor Green
    $driverInfo = Get-ItemProperty $intelDriverPath
    Write-Host "  Driver version: $($driverInfo.VersionInfo.FileVersion)"
} else {
    Write-Host "Intel PROSet driver not found at expected location" -ForegroundColor Red
}

# 5. Recommendations
Write-Host "`n=== Recommendations ===" -ForegroundColor Cyan
Write-Host "Based on the analysis:" -ForegroundColor Yellow

if ($intelSoftware) {
    Write-Host "1. Intel PROSet is installed but may not be managing the I210" -ForegroundColor Yellow
    Write-Host "   - Try uninstalling and reinstalling Intel PROSet" -ForegroundColor White
    Write-Host "   - Or update to latest Intel PROSet version" -ForegroundColor White
} else {
    Write-Host "1. Intel PROSet is not properly installed" -ForegroundColor Red
    Write-Host "   - Download and install Intel PROSet/Network Connections software" -ForegroundColor White
}

Write-Host "2. The I210 may be using generic Windows drivers instead of Intel drivers" -ForegroundColor Yellow
Write-Host "   - Check Device Manager for driver details" -ForegroundColor White
Write-Host "   - Try updating driver through Device Manager" -ForegroundColor White

Write-Host "3. For hardware timestamping support:" -ForegroundColor Yellow
Write-Host "   - Intel PROSet management is required for advanced OID support" -ForegroundColor White
Write-Host "   - Generic Windows drivers typically do not support Intel OIDs" -ForegroundColor White

Write-Host "4. Alternative approach:" -ForegroundColor Yellow
Write-Host "   - Use Windows standard timestamping APIs instead of Intel OIDs" -ForegroundColor White
Write-Host "   - Modify gPTP to use software timestamping with higher precision" -ForegroundColor White

Write-Host "`n=== Next Steps ===" -ForegroundColor Cyan
Write-Host "1. Check Device Manager for driver details"
Write-Host "2. Try installing/updating Intel PROSet"
Write-Host "3. If hardware timestamping is not available, enhance software timestamping"
Write-Host "4. Consider using standard Windows networking APIs for timestamping"
