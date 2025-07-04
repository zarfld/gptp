# Simple network adapter query
Write-Host "Querying network adapters..." -ForegroundColor Green

# Get all network adapters
$adapters = Get-WmiObject -Class Win32_NetworkAdapter
Write-Host "Total adapters found: $($adapters.Count)" -ForegroundColor Yellow

# Filter Intel adapters
$intelAdapters = $adapters | Where-Object { $_.Manufacturer -like "*Intel*" }
Write-Host "Intel adapters found: $($intelAdapters.Count)" -ForegroundColor Yellow

# Display Intel adapters
foreach ($adapter in $intelAdapters) {
    Write-Host ""
    Write-Host "--- $($adapter.Name) ---" -ForegroundColor Cyan
    Write-Host "  DeviceID: $($adapter.DeviceID)"
    Write-Host "  MACAddress: $($adapter.MACAddress)"
    Write-Host "  Manufacturer: $($adapter.Manufacturer)"
    Write-Host "  NetEnabled: $($adapter.NetEnabled)"
    Write-Host "  Speed: $($adapter.Speed)"
    Write-Host "  AdapterType: $($adapter.AdapterType)"
    Write-Host "  PNPDeviceID: $($adapter.PNPDeviceID)"
}

# Try to get the I210 adapter specifically
Write-Host ""
Write-Host "Looking for I210 adapter..." -ForegroundColor Green
$i210Adapters = $adapters | Where-Object { $_.Description -like "*I210*" -or $_.Name -like "*I210*" }
if ($i210Adapters) {
    foreach ($adapter in $i210Adapters) {
        Write-Host "Found I210: $($adapter.Name)" -ForegroundColor Green
        Write-Host "  Description: $($adapter.Description)"
        Write-Host "  PNPDeviceID: $($adapter.PNPDeviceID)"
    }
} else {
    Write-Host "No I210 adapters found" -ForegroundColor Red
}

# Check device manager for I210
Write-Host ""
Write-Host "Checking PnP devices for I210..." -ForegroundColor Green
$pnpDevices = Get-WmiObject -Class Win32_PnPEntity | Where-Object { $_.Name -like "*I210*" -or $_.DeviceID -like "*I210*" }
if ($pnpDevices) {
    foreach ($device in $pnpDevices) {
        Write-Host "PnP Device: $($device.Name)" -ForegroundColor Green
        Write-Host "  DeviceID: $($device.DeviceID)"
        Write-Host "  Status: $($device.Status)"
    }
}

# === Intel WMI/PROSet Advanced Query ===
Write-Host ""; Write-Host "Checking for Intel WMI/PROSet classes..." -ForegroundColor Cyan
$intelNamespaces = @("root\IntelNCS2", "root\IntelNCS", "root\Intel", "root\IntelManagement")
foreach ($ns in $intelNamespaces) {
    try {
        $classes = Get-WmiObject -Namespace $ns -List -ErrorAction Stop
        if ($classes) {
            Write-Host "Found classes in ${ns}:" -ForegroundColor Yellow
            foreach ($class in $classes) {
                Write-Host "  $($class.Name)" -ForegroundColor White
            }
            # For each class, show properties and methods (first instance only)
            foreach ($class in $classes | Select-Object -First 3) {
                try {
                    Write-Host "`nClass: $($class.Name)" -ForegroundColor Cyan
                    $instances = Get-WmiObject -Namespace $ns -Class $class.Name -ErrorAction SilentlyContinue
                    if ($instances) {
                        $instance = $instances | Select-Object -First 1
                        Write-Host "  Properties:" -ForegroundColor Magenta
                        $instance | Get-Member -MemberType Property | ForEach-Object { Write-Host "    $($_.Name)" }
                        Write-Host "  Methods:" -ForegroundColor Magenta
                        $instance | Get-Member -MemberType Method | ForEach-Object { Write-Host "    $($_.Name)" }
                    } else {
                        Write-Host "  No instances found." -ForegroundColor DarkGray
                    }
                } catch {
                    Write-Host "  Error accessing class $($class.Name): $($_.Exception.Message)" -ForegroundColor Red
                }
            }
            break
        }
    } catch {
        Write-Host "Namespace ${ns} not accessible: $($_.Exception.Message)" -ForegroundColor DarkGray
    }
}

# === Specific Intel Adapter Query ===
Write-Host ""; Write-Host "Querying Intel-specific adapter classes..." -ForegroundColor Cyan
$intelAdapterNamespaces = @("root\IntelNCS2", "root\IntelNCS")
foreach ($ns in $intelAdapterNamespaces) {
    try {
        Write-Host "Checking namespace: ${ns}" -ForegroundColor Yellow
        
        # Look for Intel Physical Ethernet Adapters with timeout
        Write-Host "  Querying IANet_PhysicalEthernetAdapter..." -ForegroundColor Gray
        $physicalAdapters = Get-WmiObject -Namespace $ns -Class "IANet_PhysicalEthernetAdapter" -ErrorAction SilentlyContinue
        if ($physicalAdapters) {
            Write-Host "  Found $($physicalAdapters.Count) IANet_PhysicalEthernetAdapter instance(s):" -ForegroundColor Green
            foreach ($adapter in $physicalAdapters | Select-Object -First 3) {
                Write-Host "    Name: $($adapter.Name)" -ForegroundColor White
                Write-Host "    DeviceID: $($adapter.DeviceID)" -ForegroundColor White
                Write-Host "    Description: $($adapter.Description)" -ForegroundColor White
                
                # Check for timestamp-related properties
                $timestampProps = $adapter | Get-Member -MemberType Property | Where-Object { $_.Name -like "*Timestamp*" -or $_.Name -like "*Time*" -or $_.Name -like "*PTP*" }
                if ($timestampProps) {
                    Write-Host "    Timestamp-related properties:" -ForegroundColor Cyan
                    $timestampProps | ForEach-Object {
                        $value = $adapter.$($_.Name)
                        Write-Host "      $($_.Name): $value" -ForegroundColor Yellow
                    }
                }
                Write-Host ""
            }
        } else {
            Write-Host "  No IANet_PhysicalEthernetAdapter instances found." -ForegroundColor DarkGray
        }
        
        # Look for Intel Adapter Settings with timeout
        Write-Host "  Querying IANet_AdapterSetting..." -ForegroundColor Gray
        $adapterSettings = Get-WmiObject -Namespace $ns -Class "IANet_AdapterSetting" -ErrorAction SilentlyContinue
        if ($adapterSettings) {
            $timestampSettings = $adapterSettings | Where-Object { $_.KeywordName -like "*Timestamp*" -or $_.KeywordName -like "*PTP*" -or $_.KeywordName -like "*Time*" }
            if ($timestampSettings) {
                Write-Host "  Found timestamp-related IANet_AdapterSetting instances:" -ForegroundColor Green
                foreach ($setting in $timestampSettings | Select-Object -First 5) {
                    Write-Host "    Setting: $($setting.KeywordName)" -ForegroundColor White
                    Write-Host "      DisplayName: $($setting.DisplayName)" -ForegroundColor White
                    Write-Host "      CurrentValue: $($setting.CurrentValue)" -ForegroundColor White
                    Write-Host "      DefaultValue: $($setting.DefaultValue)" -ForegroundColor White
                    Write-Host ""
                }
            } else {
                Write-Host "  No timestamp-related settings found in IANet_AdapterSetting." -ForegroundColor DarkGray
            }
        } else {
            Write-Host "  No IANet_AdapterSetting instances found." -ForegroundColor DarkGray
        }
        
        Write-Host "  Completed namespace ${ns}" -ForegroundColor Yellow
        break
    } catch {
        Write-Host "  Error accessing ${ns}: $($_.Exception.Message)" -ForegroundColor Red
    }
}
