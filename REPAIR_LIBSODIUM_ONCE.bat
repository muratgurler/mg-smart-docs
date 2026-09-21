@echo off
setlocal
cd /d "%~dp0"

set "PY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if not exist "%PY%" (
  echo.
  echo [MG COMPONENT REPAIR] ERROR
  echo PlatformIO Python not found:
  echo   %PY%
  echo.
  pause
  exit /b 2
)

echo ============================================================
echo  MG SMART TESTER - ONE-TIME LIBSODIUM REPAIR
echo  Only managed_components\espressif__libsodium is quarantined.
echo  .pio and all other components stay untouched.
echo ============================================================
echo.

"%PY%" "%~dp0tools\repair_libsodium_component.py" %*
set "RC=%ERRORLEVEL%"

echo.
if "%RC%"=="0" (
  echo Repair helper completed.
) else (
  echo Repair helper failed. Exit code: %RC%
)
echo.
pause
exit /b %RC%
