@echo off
set "PATH=C:\Program Files\CodeBlocks\MinGW\bin;%PATH%"

echo ==============================================
echo Running All Test Suites
echo ==============================================

echo [1/5] Unit Tests...
.\build\unit_tests.exe
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo [2/5] Storage Engine Scale Tests (1M keys)...
.\build\storage_tests.exe
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo [3/5] Workload Generator Tests...
.\build\workload_generator_tests.exe
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo [4/5] Torture Tool Phase 5 (Concurrency)...
.\build\torture_tool_phase5.exe --iterations 200 --threads 4
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo [5/5] Torture Tool Phase 6 (Crash/Restart)...
.\build\torture_tool_phase6.exe --reset --iterations 10
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo.
echo ==============================================
echo ALL TESTS PASSED SUCCESSFULLY!
echo ==============================================

