@echo off
REM Intel Driver Analysis Script for gPTP
REM Usage: analyze_intel_drivers.bat "C:\path\to\intel\drivers"

if "%1"=="" (
    echo Usage: %0 "C:\path\to\intel\drivers"
    echo.
    echo Example: %0 "C:\Intel\Drivers\Ethernet"
    pause
    exit /b 1
)

set DRIVER_PATH=%1
set OUTPUT_DIR=%~dp0analysis_results
set TIMESTAMP=%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%
set TIMESTAMP=%TIMESTAMP: =0%

echo Intel Driver Analysis for gPTP
echo ==============================
echo Driver path: %DRIVER_PATH%
echo Output directory: %OUTPUT_DIR%
echo.

REM Create output directory
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

REM Run PowerShell analysis script
echo Running driver analysis...
powershell.exe -ExecutionPolicy Bypass -File "%~dp0Extract-IntelDriverInfo.ps1" -DriverPath "%DRIVER_PATH%" -OutputFile "%OUTPUT_DIR%\intel_analysis_%TIMESTAMP%.json"

if %ERRORLEVEL% EQU 0 (
    echo.
    echo Analysis completed successfully!
    echo Results saved to: %OUTPUT_DIR%\intel_analysis_%TIMESTAMP%.json
    echo C++ mapping saved to: %OUTPUT_DIR%\intel_analysis_%TIMESTAMP%_mapping.cpp
    echo.
    echo You can now:
    echo 1. Review the JSON results for extracted information
    echo 2. Copy the C++ mappings to windows_hal_vendor_intel.cpp
    echo 3. Update the driver info framework with the new data
) else (
    echo.
    echo Analysis failed! Check the error messages above.
)

echo.
pause
