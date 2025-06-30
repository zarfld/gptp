@echo off
:: Intel PTP Hardware Timestamp Configuration Batch Script
:: Based on Intel community support instructions
:: https://community.intel.com/t5/Ethernet-Products/Hardware-Timestamp-on-windows/m-p/1496441#M33563

echo Intel PTP Hardware Timestamp Configuration
echo ==========================================

:: Check for administrator privileges
net session >nul 2>&1
if %errorLevel% == 0 (
    echo Running with administrator privileges - Good!
) else (
    echo WARNING: Not running as administrator. Some configurations may fail.
    echo Please run this script as administrator for best results.
    pause
)

:: Set default adapter name (change this to match your adapter)
set ADAPTER_NAME=AVB

:: Allow user to override adapter name
if not "%1"=="" set ADAPTER_NAME=%1

echo.
echo Configuring Intel PTP settings for adapter: %ADAPTER_NAME%
echo.

:: Method 1: Try Set-NetAdapterAdvancedProperty (built-in Windows command)
echo Attempting configuration using Set-NetAdapterAdvancedProperty...
powershell.exe -Command "try { Set-NetAdapterAdvancedProperty -Name '%ADAPTER_NAME%' -RegistryKeyword '*PtpHardwareTimestamp' -RegistryValue 1; Write-Host 'PTP Hardware Timestamp enabled'; Set-NetAdapterAdvancedProperty -Name '%ADAPTER_NAME%' -RegistryKeyword '*SoftwareTimestamp' -RegistryValue 3; Write-Host 'Software Timestamp set to RxAll & TxAll'; Write-Host 'Configuration successful!' -ForegroundColor Green } catch { Write-Error 'Configuration failed'; exit 1 }"

if %errorLevel% == 0 (
    echo.
    echo SUCCESS: Intel PTP configuration completed!
    goto verify
) else (
    echo.
    echo WARNING: Set-NetAdapterAdvancedProperty method failed
    echo Trying alternative configuration method...
)

:: Method 2: Try using the PowerShell script
if exist "scripts\configure_intel_ptp.ps1" (
    echo Running PowerShell configuration script...
    powershell.exe -ExecutionPolicy Bypass -File "scripts\configure_intel_ptp.ps1" -AdapterName "%ADAPTER_NAME%"
    
    if %errorLevel% == 0 (
        echo.
        echo SUCCESS: PowerShell script configuration completed!
        goto verify
    )
)

:: Method 3: Manual instructions
echo.
echo MANUAL CONFIGURATION REQUIRED
echo =============================
echo The automatic configuration failed. Please configure manually:
echo.
echo 1. Open Device Manager
echo 2. Find your Intel network adapter under "Network adapters"
echo 3. Right-click and select "Properties"
echo 4. Go to the "Advanced" tab
echo 5. Find "PTP Hardware Timestamp" and set it to "Enabled"
echo 6. Find "Software Timestamp" and set it to "RxAll & TxAll"
echo 7. Click OK and restart the adapter or system
echo.
echo Alternatively, run these PowerShell commands as administrator:
echo Set-NetAdapterAdvancedProperty -Name "%ADAPTER_NAME%" -RegistryKeyword "*PtpHardwareTimestamp" -RegistryValue 1
echo Set-NetAdapterAdvancedProperty -Name "%ADAPTER_NAME%" -RegistryKeyword "*SoftwareTimestamp" -RegistryValue 3
echo.
goto end

:verify
echo.
echo Verifying current configuration...
powershell.exe -Command "try { $props = Get-NetAdapterAdvancedProperty -Name '%ADAPTER_NAME%' | Where-Object { $_.RegistryKeyword -eq '*PtpHardwareTimestamp' -or $_.RegistryKeyword -eq '*SoftwareTimestamp' }; foreach ($prop in $props) { $value = if ($prop.RegistryKeyword -eq '*PtpHardwareTimestamp') { if ($prop.RegistryValue -eq '1') { 'Enabled' } else { 'Disabled' } } else { @('Disabled', 'RxAll', 'TxAll', 'RxAll & TxAll', 'TaggedTx', 'RxAll & TaggedTx')[[int]$prop.RegistryValue] }; Write-Host \"$($prop.RegistryKeyword): $value\" } } catch { Write-Warning 'Could not verify settings' }"

echo.
echo Configuration completed! You may need to restart the network adapter or system.
echo To restart the adapter, run: 
echo   Restart-NetAdapter -Name "%ADAPTER_NAME%"

:end
echo.
pause
