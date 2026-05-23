@echo off
setlocal enabledelayedexpansion
title Ghostpad - Deploy Payload to PS5

echo.
echo ╔══════════════════════════════════════════════════════╗
echo ║        GHOSTPAD - Deploy Payload to PS5              ║
echo ╚══════════════════════════════════════════════════════╝
echo.
echo This sends ghostpad.elf to your PS5's ELF loader (port 9021).
echo The PS5 must be jailbroken with an ELF loader running.
echo.

:: ────────────────────────────────────────────────────────────
:: Read or prompt for PS5 IP
:: ────────────────────────────────────────────────────────────
set "CONFIG_FILE=%~dp0gui\ghostpad_config.json"
set "PS5_IP="
set "PS5_ELF_PORT=9021"

:: Try to read IP from saved config using Python
python --version >nul 2>&1
if %errorLevel% EQU 0 (
    for /f "delims=" %%i in ('python -c "import json,sys; d=json.load(open(r'%CONFIG_FILE%')); print(d.get('ps5_ip',''))" 2^>nul') do set "PS5_IP=%%i"
)

if "!PS5_IP!" == "" (
    set /p PS5_IP="Enter PS5 IP address: "
)

if "!PS5_IP!" == "" (
    echo [ERROR] No IP address provided.
    pause
    exit /b 1
)

echo.
echo [*] Target PS5: !PS5_IP!:!PS5_ELF_PORT!
echo [*] Payload:    %~dp0payload\ghostpad.elf
echo.

:: ────────────────────────────────────────────────────────────
:: Check payload exists
:: ────────────────────────────────────────────────────────────
if not exist "%~dp0payload\ghostpad.elf" (
    echo [ERROR] ghostpad.elf not found!
    echo         Run setup_wsl.bat first to build the payload.
    pause
    exit /b 1
)

:: ────────────────────────────────────────────────────────────
:: Deploy via WSL + prospero-deploy
:: ────────────────────────────────────────────────────────────
set "WIN_ELF_PATH=%~dp0payload\ghostpad.elf"
for /f "delims=" %%i in ('wsl -d Ubuntu-22.04 wslpath -u "%WIN_ELF_PATH:\=/%"') do set "WSL_ELF_PATH=%%i"

echo [*] Sending payload via WSL...
echo.

wsl -d Ubuntu-22.04 -- bash -c "
    /opt/ps5-payload-sdk/bin/prospero-deploy -h !PS5_IP! -p !PS5_ELF_PORT! '!WSL_ELF_PATH!' 2>&1
    EXIT_CODE=\$?
    if [ \$EXIT_CODE -eq 0 ]; then
        echo ''
        echo '[OK] Payload sent successfully!'
        echo '     Ghostpad is now running on your PS5.'
        echo '     Launch launch_gui.bat and connect to !PS5_IP!:6967'
    else
        echo ''
        echo \"[ERROR] Deploy failed (exit code: \$EXIT_CODE)\"
        echo '        Make sure the ELF loader is running on your PS5.'
    fi
" 2>&1

echo.
pause
