@echo off
setlocal
cd /d "%~dp0"

set "PY=%USERPROFILE%\.platformio\penv\Scripts\python.exe"
if not exist "%PY%" (
  echo.
  echo [MG DIRECT UPLOAD] ERROR
  echo PlatformIO Python not found:
  echo   %PY%
  echo.
  echo Open PlatformIO once or repair its Python environment before using this uploader.
  echo.
  pause
  exit /b 2
)

echo ============================================================
echo  MG SMART TESTER - UPLOAD EXISTING FIRMWARE
echo  No PlatformIO Build / CMake / Component Manager is started.
echo ============================================================
echo.

"%PY%" "%~dp0tools\upload_existing_firmware.py" %*
set "RC=%ERRORLEVEL%"

echo.
if "%RC%"=="0" (
  echo Direct upload completed successfully.
) else (
  echo Direct upload failed. Exit code: %RC%
)
echo.
pause
exit /b %RC%
