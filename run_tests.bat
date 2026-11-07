@echo off
setlocal
set "PATH=C:\Program Files\CodeBlocks\MinGW\bin;%PATH%"

set "AUTO_MODE="
if "%1"=="--auto" set "AUTO_MODE=1"
if "%1"=="--ci" set "AUTO_MODE=1"
if "%1"=="-y" set "AUTO_MODE=1"

echo ========================================================
echo   KVDB Master Test Suite (Step-by-Step Verification)
echo ========================================================
if "%AUTO_MODE%"=="" (
    echo [Interactive Mode] Press [ENTER] after each test to proceed.
) else (
    echo [Automated Mode] Running all tests sequentially.
)
echo ========================================================
echo.

echo --------------------------------------------------------
echo [1/8] Unit Tests
echo Running: .\build\unit_tests.exe
echo --------------------------------------------------------
.\build\unit_tests.exe
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Suite 1/8 passed.
if "%AUTO_MODE%"=="" (
    echo Press [ENTER] to run Suite 2 [Storage Engine Scale Tests]...
    pause >nul
)
echo.

echo --------------------------------------------------------
echo [2/8] Storage Engine Scale Tests (1M keys)
echo Running: .\build\storage_tests.exe
echo --------------------------------------------------------
.\build\storage_tests.exe
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Suite 2/8 passed.
if "%AUTO_MODE%"=="" (
    echo Press [ENTER] to run Suite 3 [Workload Generator Tests]...
    pause >nul
)
echo.

echo --------------------------------------------------------
echo [3/8] Workload Generator Tests
echo Running: .\build\workload_generator_tests.exe
echo --------------------------------------------------------
.\build\workload_generator_tests.exe
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Suite 3/8 passed.
if "%AUTO_MODE%"=="" (
    echo Press [ENTER] to run Suite 4 [Torture Tool Phase 5 Concurrency]...
    pause >nul
)
echo.

echo --------------------------------------------------------
echo [4/8] Torture Tool Phase 5 (Concurrency Stress)
echo Running: .\build\torture_tool_phase5.exe --reset --iterations 200 --threads 4
echo --------------------------------------------------------
.\build\torture_tool_phase5.exe --reset --iterations 200 --threads 4
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Suite 4/8 passed.
if "%AUTO_MODE%"=="" (
    echo Press [ENTER] to run Suite 5 [Log Scanner Tests]...
    pause >nul
)
echo.

echo --------------------------------------------------------
echo [5/8] Log Scanner Tests
echo Running: .\build\log_scanner_test.exe
echo --------------------------------------------------------
.\build\log_scanner_test.exe
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Suite 5/8 passed.
if "%AUTO_MODE%"=="" (
    echo Press [ENTER] to run Suite 6 [Torture Tool Phase 6 Crash/Restart]...
    pause >nul
)
echo.

echo --------------------------------------------------------
echo [6/8] Torture Tool Phase 6 (Crash/Restart Verification)
echo Running: .\build\torture_tool_phase6.exe --reset --iterations 10
echo --------------------------------------------------------
.\build\torture_tool_phase6.exe --reset --iterations 10
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Suite 6/8 passed.
if "%AUTO_MODE%"=="" (
    echo Press [ENTER] to run Suite 7 [Crash Recovery Hardening Tests]...
    pause >nul
)
echo.

echo --------------------------------------------------------
echo [7/8] Crash Recovery Hardening Tests
echo Running: .\build\recovery_tests.exe
echo --------------------------------------------------------
.\build\recovery_tests.exe
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Suite 7/8 passed.
if "%AUTO_MODE%"=="" (
    echo Press [ENTER] to run Suite 8 [Subprocess and Child Process Tests]...
    pause >nul
)
echo.

echo --------------------------------------------------------
echo [8/8] Subprocess and Child Process Hardening Tests
echo Running: .\build\child_process_tests.exe
echo --------------------------------------------------------
.\build\child_process_tests.exe
if %ERRORLEVEL% neq 0 goto :test_error
echo.
echo [PASS] Suite 8/8 passed.
echo.

echo ========================================================
echo ALL 8 TEST SUITES PASSED SUCCESSFULLY!
echo ========================================================
goto :eof

:test_error
echo.
echo ========================================================
echo [FAILED] A test suite failed with exit code %ERRORLEVEL%!
echo ========================================================
exit /b %ERRORLEVEL%
