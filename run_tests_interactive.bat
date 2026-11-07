@echo off
set "PATH=C:\Program Files\CodeBlocks\MinGW\bin;%PATH%"

echo ========================================================
echo   KVDB Interactive Step-by-Step Test Runner
echo ========================================================
echo This script runs each test suite individually.
echo After each test completes, press [ENTER] to proceed.
echo ========================================================
echo.

echo --------------------------------------------------------
echo [1/6] Unit Tests
echo Running: .\build\unit_tests.exe
echo --------------------------------------------------------
.\build\unit_tests.exe
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Test 1 completed successfully.
echo Press [ENTER] to run Test 2 [Storage Engine Scale Tests]...
pause >nul
echo.

echo --------------------------------------------------------
echo [2/6] Storage Engine Scale Tests (1M keys)
echo Running: .\build\storage_tests.exe
echo --------------------------------------------------------
.\build\storage_tests.exe
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Test 2 completed successfully.
echo Press [ENTER] to run Test 3 [Workload Generator Tests]...
pause >nul
echo.

echo --------------------------------------------------------
echo [3/6] Workload Generator Tests
echo Running: .\build\workload_generator_tests.exe
echo --------------------------------------------------------
.\build\workload_generator_tests.exe
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Test 3 completed successfully.
echo Press [ENTER] to run Test 4 [Torture Tool Phase 5 Concurrency]...
pause >nul
echo.

echo --------------------------------------------------------
echo [4/6] Torture Tool Phase 5 (Concurrency)
echo Running: .\build\torture_tool_phase5.exe --reset --iterations 200 --threads 4
echo --------------------------------------------------------
.\build\torture_tool_phase5.exe --reset --iterations 200 --threads 4
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Test 4 completed successfully.
echo Press [ENTER] to run Test 5 [Log Scanner Tests]...
pause >nul
echo.

echo --------------------------------------------------------
echo [5/6] Log Scanner Tests
echo Running: .\build\log_scanner_test.exe
echo --------------------------------------------------------
.\build\log_scanner_test.exe
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Test 5 completed successfully.
echo Press [ENTER] to run Test 6 [Torture Tool Phase 6 Crash/Restart]...
pause >nul
echo.

echo --------------------------------------------------------
echo [6/6] Torture Tool Phase 6 (Crash/Restart)
echo Running: .\build\torture_tool_phase6.exe --reset --iterations 10
echo --------------------------------------------------------
.\build\torture_tool_phase6.exe --reset --iterations 10
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Test 6 completed successfully.
echo.
echo ========================================================
echo ALL 6 TEST SUITES PASSED SUCCESSFULLY!
echo ========================================================
goto :eof

:test_error
echo.
echo ========================================================
echo [FAILED] A test failed with exit code %ERRORLEVEL%!
echo ========================================================
exit /b %ERRORLEVEL%

