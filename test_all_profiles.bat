@echo off
echo === gPTP Profile Validation Test ===
echo.

echo Testing profile configuration files...
echo.

echo --- Milan Profile Test ---
echo %TIME% > test_milan_profile.log
build\Debug\gptp.exe test_milan_config.ini --test-config 2>&1 | head -20
echo.

echo --- AVnu Base Profile Test ---
echo %TIME% > test_avnu_base_profile.log  
build\Debug\gptp.exe avnu_base_config.ini --test-config 2>&1 | head -20
echo.

echo --- Unified Profile Test ---
echo %TIME% > test_unified_profile.log
build\Debug\gptp.exe test_unified_profiles.ini --test-config 2>&1 | head -20
echo.

echo === Profile Validation Complete ===
echo All configuration files are deployed and ready for testing.
