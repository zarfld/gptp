# Intel Driver Information Extraction Tool
# This PowerShell script extracts gPTP-relevant information from Intel driver files

param(
    [Parameter(Mandatory=$true)]
    [string]$DriverPath,
    [string]$OutputFile = "intel_driver_analysis.json"
)

Write-Host "Intel Driver Analysis Tool for gPTP" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan

# Initialize results structure
$results = @{
    timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    driver_path = $DriverPath
    device_mappings = @()
    registry_parameters = @()
    capabilities = @{}
    errors = @()
}

function Extract-INFDeviceIDs {
    param([string]$infPath)
    
    Write-Host "Analyzing INF file: $infPath" -ForegroundColor Yellow
    
    if (-not (Test-Path $infPath)) {
        $results.errors += "INF file not found: $infPath"
        return
    }
    
    $content = Get-Content $infPath
    
    # Find device ID mappings
    $inDeviceSection = $false
    foreach ($line in $content) {
        if ($line -match '^\[.*\.NTamd64.*\]') {
            $inDeviceSection = $true
            continue
        }
        
        if ($inDeviceSection -and $line -match '^\[') {
            $inDeviceSection = $false
        }
        
        if ($inDeviceSection -and $line -match 'PCI\\VEN_8086&DEV_([0-9A-F]{4})') {
            $deviceId = $matches[1]
            $deviceName = ""
            
            # Extract device description
            if ($line -match '%(.+?)%\s*=') {
                $deviceName = $matches[1]
            }
            
            $results.device_mappings += @{
                device_id = "0x$deviceId"
                device_name = $deviceName
                pci_string = ($line -split ',' | Where-Object { $_ -match 'PCI\\VEN' }).Trim()
            }
        }
    }
    
    # Find registry parameters related to timestamping
    $inRegSection = $false
    foreach ($line in $content) {
        if ($line -match '^\[.*\.reg\]') {
            $inRegSection = $true
            continue
        }
        
        if ($inRegSection -and $line -match '^\[') {
            $inRegSection = $false
        }
        
        if ($inRegSection -and $line -match 'HKR.*\\(.*?),.*ParamDesc.*%(.*?)%') {
            $paramName = $matches[1]
            $paramDesc = $matches[2]
            
            # Look for timestamp, PTP, or clock related parameters
            if ($paramName -match 'Timestamp|IEEE1588|PTP|Clock' -or 
                $paramDesc -match 'Timestamp|IEEE1588|PTP|Clock') {
                
                $results.registry_parameters += @{
                    parameter_name = $paramName
                    description = $paramDesc
                    registry_path = "Ndi\\params\\$paramName"
                }
            }
        }
    }
}

function Extract-DriverVersionInfo {
    param([string]$sysPath)
    
    Write-Host "Analyzing driver binary: $sysPath" -ForegroundColor Yellow
    
    if (-not (Test-Path $sysPath)) {
        $results.errors += "Driver binary not found: $sysPath"
        return
    }
    
    try {
        $versionInfo = (Get-Item $sysPath).VersionInfo
        $results.capabilities.driver_version = $versionInfo.ProductVersion
        $results.capabilities.file_version = $versionInfo.FileVersion
        $results.capabilities.company_name = $versionInfo.CompanyName
        $results.capabilities.file_description = $versionInfo.FileDescription
        
        # Check digital signature
        $signature = Get-AuthenticodeSignature $sysPath
        $results.capabilities.signature_status = $signature.Status.ToString()
        $results.capabilities.signature_issuer = $signature.SignerCertificate.Subject
        
    } catch {
        $results.errors += "Failed to extract version info from $sysPath : $($_.Exception.Message)"
    }
}

function Find-ClockRateHints {
    param([string]$driverPath)
    
    Write-Host "Searching for clock rate information..." -ForegroundColor Yellow
    
    # Look in INF files for clock rate hints
    $infFiles = Get-ChildItem -Path $driverPath -Filter "*.inf" -Recurse
    
    foreach ($inf in $infFiles) {
        $content = Get-Content $inf.FullName
        foreach ($line in $content) {
            if ($line -match 'Clock|Frequency|Rate.*?(\d{6,})') {
                $clockRate = $matches[1]
                $results.capabilities.clock_rate_hints += @{
                    file = $inf.Name
                    value = $clockRate
                    context = $line.Trim()
                }
            }
        }
    }
}

function Generate-CppMapping {
    Write-Host "Generating C++ device mapping..." -ForegroundColor Green
    
    $cppOutput = @"
// Generated Intel device mappings from driver analysis
// Generated: $($results.timestamp)

static DeviceClockRateMapping ExtractedDeviceClockRateMap[] = {
"@

    foreach ($device in $results.device_mappings) {
        $cppOutput += "`n    {`"$($device.device_name)`", 125000000}, // Device ID: $($device.device_id)"
    }
    
    $cppOutput += @"

    {NULL, 0}
};

// Registry parameters found in driver analysis
static const std::map<std::string, std::string> ExtractedRegistryParams = {
"@

    foreach ($param in $results.registry_parameters) {
        $cppOutput += "`n    {`"$($param.parameter_name)`", `"$($param.description)`"},"
    }
    
    $cppOutput += @"

};
"@

    # Save to file
    $cppFile = $OutputFile.Replace('.json', '_mapping.cpp')
    $cppOutput | Out-File -FilePath $cppFile -Encoding UTF8
    Write-Host "C++ mapping saved to: $cppFile" -ForegroundColor Green
}

# Main execution
try {
    if (-not (Test-Path $DriverPath)) {
        Write-Error "Driver path not found: $DriverPath"
        exit 1
    }
    
    Write-Host "Scanning driver path: $DriverPath" -ForegroundColor Green
    
    # Find and analyze INF files
    $infFiles = Get-ChildItem -Path $DriverPath -Filter "*.inf" -Recurse
    Write-Host "Found $($infFiles.Count) INF files" -ForegroundColor White
    
    foreach ($inf in $infFiles) {
        Extract-INFDeviceIDs -infPath $inf.FullName
    }
    
    # Find and analyze SYS files
    $sysFiles = Get-ChildItem -Path $DriverPath -Filter "*.sys" -Recurse
    Write-Host "Found $($sysFiles.Count) SYS files" -ForegroundColor White
    
    foreach ($sys in $sysFiles) {
        Extract-DriverVersionInfo -sysPath $sys.FullName
    }
    
    # Look for clock rate information
    Find-ClockRateHints -driverPath $DriverPath
    
    # Generate C++ mappings
    Generate-CppMapping
    
    # Save results to JSON
    $results | ConvertTo-Json -Depth 10 | Out-File -FilePath $OutputFile -Encoding UTF8
    
    # Display summary
    Write-Host "`nAnalysis Complete!" -ForegroundColor Green
    Write-Host "=================" -ForegroundColor Green
    Write-Host "Device mappings found: $($results.device_mappings.Count)" -ForegroundColor White
    Write-Host "Registry parameters found: $($results.registry_parameters.Count)" -ForegroundColor White
    Write-Host "Errors: $($results.errors.Count)" -ForegroundColor White
    Write-Host "Results saved to: $OutputFile" -ForegroundColor Green
    
    if ($results.errors.Count -gt 0) {
        Write-Host "`nErrors encountered:" -ForegroundColor Red
        foreach ($error in $results.errors) {
            Write-Host "  - $error" -ForegroundColor Red
        }
    }
    
} catch {
    Write-Error "Analysis failed: $($_.Exception.Message)"
    exit 1
}
