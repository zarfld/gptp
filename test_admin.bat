@echo off
REM Test gPTP as Administrator
echo === GPTP ADMINISTRATOR MODE TEST ===
echo Current user: %USERNAME%
echo Current time: %TIME%

cd /d "C:\Users\dzarf\source\repos\gptp-1\build\Debug"

echo.
echo === Configuration ===
type gptp_cfg.ini | more

echo.
echo === Starting gPTP as Administrator ===
echo Watch for Intel PTP hardware clock status...
echo.

REM Run gPTP and redirect output to log file
gptp.exe 68-05-CA-8B-76-4E > admin_test_output.log 2>&1

echo.
echo === Administrator test complete ===
echo Check admin_test_output.log for results
pause
