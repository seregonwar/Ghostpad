@echo off
setlocal enabledelayedexpansion
title Ghostpad - WSL Setup

echo.
echo +------------------------------------------------------+
echo ^|          GHOSTPAD - WSL Environment Setup            ^|
echo ^|   PS5 Remote Controller Payload Build System         ^|
echo +------------------------------------------------------+
echo.

:: ────────────────────────────────────────────────────────────
:: Step 0: Request Administrator privileges if needed
:: ────────────────────────────────────────────────────────────
net session >nul 2>&1
if %errorLevel% NEQ 0 (
    echo [!] Requesting Administrator privileges...
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b 0
)
echo [OK] Running with Administrator privileges.
echo.

:: ────────────────────────────────────────────────────────────
:: Step 1: Check WSL 2
:: ────────────────────────────────────────────────────────────
echo [1/7] Checking WSL availability...
wsl --status >nul 2>&1
if %errorLevel% NEQ 0 (
    echo [!] WSL not found. Installing WSL 2...
    wsl --install --no-distribution
    if %errorLevel% NEQ 0 (
        echo [ERROR] WSL installation failed.
        echo         Enable Virtualization in BIOS and ensure Windows 10 build 19041+.
        pause & exit /b 1
    )
    echo [!] WSL installed. Please RESTART and run this script again.
    pause & exit /b 0
)
echo [OK] WSL is available.

:: ────────────────────────────────────────────────────────────
:: Step 2: Check for Ubuntu 22.04
:: ────────────────────────────────────────────────────────────
echo.
echo [2/7] Checking for Ubuntu 22.04...
wsl -d Ubuntu-22.04 -- echo ok >nul 2>&1
if %errorLevel% NEQ 0 (
    echo [!] Ubuntu 22.04 not found. Installing...
    wsl --install -d Ubuntu-22.04
    if %errorLevel% NEQ 0 (
        echo [ERROR] Ubuntu 22.04 installation failed.
        pause & exit /b 1
    )
    echo [!] Ubuntu installed. Complete username/password setup, then run this script again.
    pause & exit /b 0
)
echo [OK] Ubuntu 22.04 is available.

:: ────────────────────────────────────────────────────────────
:: Get the WSL path for our scripts directory
:: ────────────────────────────────────────────────────────────
set "WIN_SCRIPTS=%~dp0scripts"
set "WIN_PAYLOAD=%~dp0payload"

:: Convert Windows paths to WSL paths via wslpath
for /f "delims=" %%i in ('wsl -d Ubuntu-22.04 -- wslpath -u "%WIN_SCRIPTS:\=/%"') do set "WSL_SCRIPTS=%%i"
for /f "delims=" %%i in ('wsl -d Ubuntu-22.04 -- wslpath -u "%WIN_PAYLOAD:\=/%"') do set "WSL_PAYLOAD=%%i"

echo [*] Scripts dir (WSL): !WSL_SCRIPTS!
echo [*] Payload dir (WSL): !WSL_PAYLOAD!

:: ────────────────────────────────────────────────────────────
:: Step 3: Install build dependencies (runs install_deps.sh)
:: ────────────────────────────────────────────────────────────
echo.
echo [3/7] Installing build dependencies (clang-18, lld-18, etc.)...
echo       clang-18 requires the LLVM apt repo - this is handled automatically.
echo       First run may take 3-5 minutes...
echo.
wsl -d Ubuntu-22.04 -- bash "!WSL_SCRIPTS!/install_deps.sh"
if %errorLevel% NEQ 0 (
    echo.
    echo [ERROR] Dependency installation failed.
    echo.
    echo If you see "sudo: command not found" or permission errors:
    echo   1. Open Windows Settings ^> System ^> For Developers
    echo   2. Enable "Sudo" under the Developer Mode section
    echo   OR open WSL manually and run:
    echo      bash scripts/install_deps.sh
    pause & exit /b 1
)
echo.
echo [OK] Dependencies installed.

:: ────────────────────────────────────────────────────────────
:: Step 4: Download PS5 Payload SDK (runs install_sdk.sh)
:: ────────────────────────────────────────────────────────────
echo.
echo [4/7] Downloading and installing PS5 Payload SDK...
wsl -d Ubuntu-22.04 -- bash "!WSL_SCRIPTS!/install_sdk.sh"
if %errorLevel% NEQ 0 (
    echo [ERROR] SDK installation failed. Check output above.
    pause & exit /b 1
)
echo [OK] PS5 Payload SDK ready.

:: ────────────────────────────────────────────────────────────
:: Step 5: Verify toolchain
:: ────────────────────────────────────────────────────────────
echo.
echo [5/7] Verifying SDK toolchain...
wsl -d Ubuntu-22.04 -- bash -c "test -f /opt/ps5-payload-sdk/bin/prospero-clang && echo '[OK] prospero-clang found' || echo '[ERROR] prospero-clang missing'"
wsl -d Ubuntu-22.04 -- bash -c "test -f /opt/ps5-payload-sdk/toolchain/prospero.mk && echo '[OK] prospero.mk found' || echo '[ERROR] prospero.mk missing'"

:: ────────────────────────────────────────────────────────────
:: Step 6: Build the payload (runs build_payload.sh)
:: ────────────────────────────────────────────────────────────
echo.
echo [6/7] Building Ghostpad payload...
wsl -d Ubuntu-22.04 -- bash "!WSL_SCRIPTS!/build_payload.sh" "!WSL_PAYLOAD!"
if %errorLevel% NEQ 0 (
    echo [WARN] Build may have failed. Check output above.
    echo        You can rebuild manually in WSL:
    echo          export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
    echo          make -C payload clean all
) else (
    echo [OK] ghostpad.elf built successfully.
)

:: ────────────────────────────────────────────────────────────
:: Step 7: Check Python for GUI
:: ────────────────────────────────────────────────────────────
echo.
echo [7/7] Checking Python for GUI...
python --version >nul 2>&1
if %errorLevel% EQU 0 (
    for /f "tokens=*" %%v in ('python --version 2^>^&1') do echo [OK] %%v found.
    python -c "import tkinter" >nul 2>&1
    if %errorLevel% EQU 0 (
        echo [OK] tkinter available - GUI is ready.
    ) else (
        echo [WARN] tkinter not found. Reinstall Python and check "tcl/tk and IDLE".
    )
) else (
    echo [WARN] Python not found. Install Python 3.8+ from https://python.org
    echo        Check "Add Python to PATH" during installation.
)

:: ────────────────────────────────────────────────────────────
:: Done
:: ────────────────────────────────────────────────────────────
echo.
echo ========================================================
echo   Setup Complete!
echo.
echo   NEXT STEPS:
echo   1. Deploy payload to PS5 (must have ELF loader running):
echo        Double-click  deploy_payload.bat
echo      OR in WSL:
echo        export PS5_PAYLOAD_SDK=/opt/ps5-payload-sdk
echo        make -C payload test PS5_HOST=^<your-ps5-ip^>
echo.
echo   2. Launch the controller GUI:
echo        Double-click  launch_gui.bat
echo.
echo   Payload listens on port 6967.
echo   ELF loader (elfldr) is on port 9021.
echo ========================================================
echo.
pause
