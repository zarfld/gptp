# Intel Network Adapter WMI Query Script
# This script queries the Intel WMI namespace to discover PTP/timestamping capabilities

Write-Host "=== Intel Network Adapter WMI Query Script ===" -ForegroundColor Green
Write-Host ""

# Function to safely execute WMI queries
function Safe-WMIQuery {
    param(
        [string]$Namespace,
        [string]$Class,
        [string]$Description
    )
    
    try {
        Write-Host "Querying $Description..." -ForegroundColor Yellow
        $result = Get-WmiObject -Namespace $Namespace -Class $Class -ErrorAction Stop
        return $result
    }
    catch {
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        return $null
    }
}

# Function to display object properties nicely
function Display-Properties {
    param(
        [object]$Object,
        [string]$Title,
        [string[]]$FilterProperties = @()
    )
    
    if ($Object -eq $null) {
        Write-Host "  No data available" -ForegroundColor Gray
        return
    }
    
    Write-Host ""
    Write-Host "--- $Title ---" -ForegroundColor Cyan
    
    if ($Object -is [System.Array]) {
        Write-Host "Found $($Object.Count) items:" -ForegroundColor Green
        for ($i = 0; $i -lt $Object.Count; $i++) {
            Write-Host "  [$i] $($Object[$i].GetType().Name)" -ForegroundColor Gray
        }
        
        if ($Object.Count -gt 0) {
            Write-Host ""
            Write-Host "Properties of first item:" -ForegroundColor Green
            $Object = $Object[0]
        }
    }
    
    $properties = $Object | Get-Member -MemberType Property | Where-Object { 
        $FilterProperties.Count -eq 0 -or $_.Name -in $FilterProperties 
    } | Sort-Object Name
    
    foreach ($prop in $properties) {
        $value = $Object.($prop.Name)
        if ($value -ne $null) {
            Write-Host "  $($prop.Name): $value" -ForegroundColor White
        }
    }
}

# 1. Check if Intel WMI namespace exists
Write-Host "1. Checking for Intel WMI namespace..." -ForegroundColor Yellow
try {
    $namespaces = Get-WmiObject -Namespace "root" -Class "__Namespace" | Where-Object { $_.Name -like "*Intel*" }
    if ($namespaces) {
        Write-Host "Found Intel-related namespaces:" -ForegroundColor Green
        foreach ($ns in $namespaces) {
            Write-Host "  - root\$($ns.Name)" -ForegroundColor White
        }
    } else {
        Write-Host "  No Intel WMI namespaces found" -ForegroundColor Red
    }
}
catch {
    Write-Host "  Error checking namespaces: $($_.Exception.Message)" -ForegroundColor Red
}

# 2. Try different Intel namespace variations
$possibleNamespaces = @("root\IntelNCS2", "root\IntelNCS", "root\Intel", "root\IntelManagement")
$workingNamespace = $null

foreach ($namespace in $possibleNamespaces) {
    Write-Host ""
    Write-Host "2. Trying namespace: $namespace" -ForegroundColor Yellow
    try {
        $classes = Get-WmiObject -Namespace $namespace -List | Where-Object { $_.Name -like "*Adapter*" -or $_.Name -like "*Ethernet*" -or $_.Name -like "*Net*" }
        if ($classes) {
            Write-Host "  Found classes in $namespace" -ForegroundColor Green
            $workingNamespace = $namespace
            foreach ($class in $classes) {
                Write-Host "    - $($class.Name)" -ForegroundColor White
            }
            break
        }
    }
    catch {
        Write-Host "  Namespace $namespace not accessible: $($_.Exception.Message)" -ForegroundColor Gray
    }
}

if ($workingNamespace) {
    Write-Host ""
    Write-Host "=== Using namespace: $workingNamespace ===" -ForegroundColor Green
    
    # 3. Query Intel Physical Ethernet Adapters
    $adapters = Safe-WMIQuery -Namespace $workingNamespace -Class "IANet_PhysicalEthernetAdapter" -Description "Intel Physical Ethernet Adapters"
    Display-Properties -Object $adapters -Title "Intel Ethernet Adapters" -FilterProperties @("Name", "DeviceID", "MACAddress", "LinkStatus", "LinkSpeed", "DriverVersion", "FWVersion", "PTPSupported", "PTPEnabled", "TimestampingSupported", "TimestampingEnabled", "Capabilities", "SupportedFeatures", "EnabledFeatures")
    
    # 4. Query adapter settings related to PTP/Timestamping
    Write-Host ""
    Write-Host "4. Querying adapter settings for PTP/Timestamping..." -ForegroundColor Yellow
    
    $settingClasses = @("IANet_AdapterSettingInt", "IANet_AdapterSettingEnum", "IANet_AdapterSettingString")
    foreach ($settingClass in $settingClasses) {
        $settings = Safe-WMIQuery -Namespace $workingNamespace -Class $settingClass -Description "$settingClass settings"
        if ($settings) {
            $ptpSettings = $settings | Where-Object { $_.SettingName -like "*PTP*" -or $_.SettingName -like "*Time*" -or $_.SettingName -like "*Clock*" -or $_.SettingName -like "*Stamp*" }
            if ($ptpSettings) {
                Write-Host ""
                Write-Host "PTP/Timestamping related settings in $settingClass :" -ForegroundColor Cyan
                foreach ($setting in $ptpSettings) {
                    Write-Host "  $($setting.SettingName): $($setting.Value) (Writable: $($setting.Writable))" -ForegroundColor White
                }
            }
        }
    }
    
    # 5. Query diagnostic capabilities
    $diagTests = Safe-WMIQuery -Namespace $workingNamespace -Class "IANet_DiagTest" -Description "Diagnostic Tests"
    if ($diagTests) {
        $timingTests = $diagTests | Where-Object { $_.TestName -like "*Time*" -or $_.TestName -like "*Clock*" -or $_.TestName -like "*PTP*" }
        if ($timingTests) {
            Write-Host ""
            Write-Host "Timing/PTP related diagnostic tests:" -ForegroundColor Cyan
            foreach ($test in $timingTests) {
                Write-Host "  $($test.TestName): $($test.Description)" -ForegroundColor White
            }
        }
    }
    
} else {
    Write-Host ""
    Write-Host "No working Intel WMI namespace found. Trying standard WMI..." -ForegroundColor Yellow
    
    # Fallback to standard WMI
    Write-Host "5. Querying standard network adapters..." -ForegroundColor Yellow
    try {
        $standardAdapters = Get-WmiObject -Class "Win32_NetworkAdapter" | Where-Object { $_.Manufacturer -like "*Intel*" -and $_.NetEnabled -eq $true }
        Display-Properties -Object $standardAdapters -Title "Standard Intel Network Adapters" -FilterProperties @("Name", "DeviceID", "MACAddress", "Speed", "Manufacturer", "Description")
        
        # Check for Intel-specific driver properties
        if ($standardAdapters) {
            Write-Host ""
            Write-Host "6. Checking Intel adapter configurations..." -ForegroundColor Yellow
            foreach ($adapter in $standardAdapters) {
                Write-Host "  Adapter: $($adapter.Name)" -ForegroundColor Cyan
                try {
                    $config = Get-WmiObject -Class "Win32_NetworkAdapterConfiguration" | Where-Object { $_.Index -eq $adapter.DeviceID }
                    if ($config) {
                        Write-Host "    IP Enabled: $($config.IPEnabled)" -ForegroundColor White
                        Write-Host "    DHCP Enabled: $($config.DHCPEnabled)" -ForegroundColor White
                    }
                }
                catch {
                    Write-Host "    Error getting configuration: $($_.Exception.Message)" -ForegroundColor Red
                }
            }
        }
    }
    catch {
        Write-Host "  Error querying standard adapters: $($_.Exception.Message)" -ForegroundColor Red
    }
}

# 7. Check Intel PROSet installation
Write-Host ""
Write-Host "7. Checking for Intel PROSet installation..." -ForegroundColor Yellow
try {
    $intelSoftware = Get-WmiObject -Class "Win32_Product" | Where-Object { $_.Name -like "*Intel*" -and ($_.Name -like "*PROSet*" -or $_.Name -like "*Network*" -or $_.Name -like "*Ethernet*") }
    if ($intelSoftware) {
        Write-Host "Found Intel network software:" -ForegroundColor Green
        foreach ($software in $intelSoftware) {
            Write-Host "  $($software.Name) - Version: $($software.Version)" -ForegroundColor White
        }
    } else {
        Write-Host "  No Intel network software found" -ForegroundColor Gray
    }
}
catch {
    Write-Host "  Error checking installed software: $($_.Exception.Message)" -ForegroundColor Red
}

# 8. Check for Intel services
Write-Host ""
Write-Host "8. Checking for Intel network services..." -ForegroundColor Yellow
try {
    $intelServices = Get-Service | Where-Object { $_.DisplayName -like "*Intel*" -and $_.DisplayName -like "*Network*" }
    if ($intelServices) {
        Write-Host "Found Intel network services:" -ForegroundColor Green
        foreach ($service in $intelServices) {
            Write-Host "  $($service.DisplayName): $($service.Status)" -ForegroundColor White
        }
    } else {
        Write-Host "  No Intel network services found" -ForegroundColor Gray
    }
}
catch {
    Write-Host "  Error checking services: $($_.Exception.Message)" -ForegroundColor Red
}

Write-Host ""
Write-Host "=== Query Complete ===" -ForegroundColor Green
Write-Host "If no Intel WMI interface was found, this suggests:" -ForegroundColor Yellow
Write-Host "1. Intel PROSet drivers/tools may not be installed" -ForegroundColor White
Write-Host "2. The adapter may not support advanced WMI management" -ForegroundColor White
Write-Host "3. WMI interface may be disabled or not available for this adapter model" -ForegroundColor White
