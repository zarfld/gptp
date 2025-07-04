# Comprehensive Intel WMI and adapter analysis script
# Attempts to query Intel WMI interfaces and correlate with actual hardware

Write-Host "=== Comprehensive Intel Adapter & WMI Analysis ===" -ForegroundColor Cyan

# Function to safely query WMI with error handling
function Query-WMI {
    param(
        [string]$Namespace,
        [string]$Class,
        [string]$Filter = $null
    )
    
    try {
        if ($Filter) {
            return Get-WmiObject -Namespace $Namespace -Class $Class -Filter $Filter -ErrorAction Stop
        } else {
            return Get-WmiObject -Namespace $Namespace -Class $Class -ErrorAction Stop
        }
    }
    catch {
        Write-Host "    Error querying $Namespace\$Class : $($_.Exception.Message)" -ForegroundColor Red
        return $null
    }
}

# 1. First, let's see all network adapters on the system
Write-Host "`n=== All Network Adapters (Standard WMI) ===" -ForegroundColor Cyan
$standardAdapters = Query-WMI -Namespace "root\cimv2" -Class "Win32_NetworkAdapter"
if ($standardAdapters) {
    $intelAdapters = $standardAdapters | Where-Object { $_.Manufacturer -like "*Intel*" -or $_.Description -like "*Intel*" }
    foreach ($adapter in $intelAdapters) {
        Write-Host "`nAdapter: $($adapter.Name)" -ForegroundColor Yellow
        Write-Host "  Description: $($adapter.Description)"
        Write-Host "  Manufacturer: $($adapter.Manufacturer)"
        Write-Host "  PNP Device ID: $($adapter.PNPDeviceID)"
        Write-Host "  MAC Address: $($adapter.MACAddress)"
        Write-Host "  Net Connection ID: $($adapter.NetConnectionID)"
        Write-Host "  Adapter Type: $($adapter.AdapterType)"
        Write-Host "  Device ID: $($adapter.DeviceID)"
    }
}

# 2. Query Intel WMI interface with more detail
Write-Host "`n=== Intel WMI Interface Detailed Analysis ===" -ForegroundColor Cyan

# Try to instantiate Intel adapter objects directly
$intelNamespace = "root\IntelNCS2"

# Get all adapter-related classes
$adapterClasses = @(
    "IANet_PhysicalEthernetAdapter",
    "IANet_EthernetAdapter", 
    "IANet_LogicalEthernetAdapter",
    "CIM_EthernetAdapter",
    "CIM_NetworkAdapter"
)

foreach ($className in $adapterClasses) {
    Write-Host "`nQuerying $className..." -ForegroundColor Yellow
    $adapters = Query-WMI -Namespace $intelNamespace -Class $className
    
    if ($adapters) {
        foreach ($adapter in $adapters) {
            Write-Host "  Found adapter instance:" -ForegroundColor Green
            
            # Get all properties
            $properties = $adapter.PSObject.Properties | Where-Object { $_.MemberType -eq "Property" }
            foreach ($prop in $properties) {
                if ($prop.Value -ne $null -and $prop.Value -ne "") {
                    Write-Host "    $($prop.Name): $($prop.Value)"
                }
            }
        }
    } else {
        Write-Host "  No instances found" -ForegroundColor Gray
    }
}

# 3. Try to enumerate adapter settings and look for PTP/Timestamping
Write-Host "`n=== Intel Adapter Settings Analysis ===" -ForegroundColor Cyan

$settingClasses = @(
    "IANet_AdapterSettingInt",
    "IANet_AdapterSettingEnum", 
    "IANet_AdapterSettingString",
    "IANet_AdapterSetting"
)

foreach ($settingClass in $settingClasses) {
    Write-Host "`nQuerying $settingClass..." -ForegroundColor Yellow
    $settings = Query-WMI -Namespace $intelNamespace -Class $settingClass
    
    if ($settings) {
        # Look for PTP/Timestamping related settings
        $relevantSettings = $settings | Where-Object { 
            ($_.SettingName -like "*PTP*") -or 
            ($_.SettingName -like "*Timestamp*") -or 
            ($_.SettingName -like "*1588*") -or
            ($_.DisplayName -like "*PTP*") -or
            ($_.DisplayName -like "*Timestamp*") -or
            ($_.DisplayName -like "*1588*") -or
            ($_.Description -like "*PTP*") -or
            ($_.Description -like "*Timestamp*") -or
            ($_.Description -like "*1588*")
        }
        
        if ($relevantSettings) {
            Write-Host "  Found PTP/Timestamping settings:" -ForegroundColor Green
            foreach ($setting in $relevantSettings) {
                Write-Host "    Setting Name: $($setting.SettingName)"
                if ($setting.DisplayName) { Write-Host "      Display Name: $($setting.DisplayName)" }
                if ($setting.Description) { Write-Host "      Description: $($setting.Description)" }
                if ($setting.Value -ne $null) { Write-Host "      Current Value: $($setting.Value)" }
                if ($setting.Writable -ne $null) { Write-Host "      Writable: $($setting.Writable)" }
                Write-Host ""
            }
        }
        
        # Also show all settings if there are not too many
        if ($settings.Count -le 20) {
            Write-Host "  All settings in $settingClass :" -ForegroundColor Cyan
            foreach ($setting in $settings) {
                Write-Host "    $($setting.SettingName) = $($setting.Value)"
            }
        } else {
            Write-Host "  Found $($settings.Count) settings total" -ForegroundColor Gray
        }
    }
}

# 4. Try to get diagnostic capabilities
Write-Host "`n=== Intel Diagnostic Capabilities ===" -ForegroundColor Cyan
$diagTests = Query-WMI -Namespace $intelNamespace -Class "IANet_DiagTest"
if ($diagTests) {
    foreach ($test in $diagTests) {
        Write-Host "  Test: $($test.TestName)"
        Write-Host "    Description: $($test.Description)"
        Write-Host "    Test Type: $($test.TestType)"
    }
}

# 5. Check for Intel network services
Write-Host "`n=== Intel Network Services ===" -ForegroundColor Cyan
$netServices = Query-WMI -Namespace $intelNamespace -Class "IANet_NetService"
if ($netServices) {
    foreach ($service in $netServices) {
        Write-Host "  Service: $($service.ServiceName)"
        Write-Host "    Status: $($service.Status)"
        Write-Host "    Type: $($service.ServiceType)"
    }
}

# 6. Cross-reference with our I210 findings
Write-Host "`n=== Cross-Reference with I210 Analysis ===" -ForegroundColor Cyan

# Try to find settings or adapters that match our I210's MAC address
$i210MacAddress = "70-85-C2-97-53-F6"  # From our previous analysis
Write-Host "Looking for I210 adapter with MAC: $i210MacAddress"

# Check if any Intel WMI objects reference this MAC
foreach ($className in $adapterClasses) {
    $adapters = Query-WMI -Namespace $intelNamespace -Class $className
    if ($adapters) {
        $matchingAdapter = $adapters | Where-Object { $_.MACAddress -eq $i210MacAddress }
        if ($matchingAdapter) {
            Write-Host "Found matching adapter in $className!" -ForegroundColor Green
            $matchingAdapter.PSObject.Properties | Where-Object { $_.Value -ne $null } | ForEach-Object {
                Write-Host "  $($_.Name): $($_.Value)"
            }
        }
    }
}

# 7. Try to detect if Intel PROSet is properly configured for this adapter
Write-Host "`n=== Intel PROSet Configuration Check ===" -ForegroundColor Cyan

# Check registry for Intel management capabilities
$intelRegPaths = @(
    "HKLM:\SOFTWARE\Intel\NCS2",
    "HKLM:\SOFTWARE\Intel\PROSet",
    "HKLM:\SOFTWARE\Intel\NetworkConnections"
)

foreach ($regPath in $intelRegPaths) {
    if (Test-Path $regPath) {
        Write-Host "Found Intel registry path: $regPath" -ForegroundColor Green
        try {
            $regItems = Get-ChildItem $regPath -Recurse -ErrorAction SilentlyContinue
            Write-Host "  Contains $($regItems.Count) items"
        }
        catch {
            Write-Host "  Could not enumerate items: $($_.Exception.Message)"
        }
    }
}

Write-Host "`n=== Analysis Complete ===" -ForegroundColor Cyan
Write-Host "Summary:" -ForegroundColor Yellow
Write-Host "- Intel WMI namespace exists but may not have adapter instances"
Write-Host "- This could indicate the I210 is not managed by Intel PROSet"
Write-Host "- The adapter may be using generic Windows drivers instead"
Write-Host "- Consider installing Intel PROSet for advanced management capabilities"
