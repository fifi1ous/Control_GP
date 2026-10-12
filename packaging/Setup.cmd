@echo off
rem Downloads the Python packages and Poppler needed by Control_GP.
rem Control_GP.exe offers this automatically on the first start; run this
rem file manually only if that failed or to repair the installation.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0tools\setup_deps.ps1" %*
echo.
pause
