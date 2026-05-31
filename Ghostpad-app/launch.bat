@echo off
setlocal EnableDelayedExpansion

cd /d "%~dp0"
title Ghostpad Desktop

echo.
echo ========================================
echo   Ghostpad - PS5 Desktop Client
echo ========================================
echo.

where node >nul 2>&1
if errorlevel 1 (
  echo ERROR: Node.js is not installed or not in PATH.
  echo Install Node.js from https://nodejs.org/
  pause
  exit /b 1
)

if not exist "node_modules" (
  echo Installing Electron dependencies...
  call npm install
  if errorlevel 1 goto :failed
)

if not exist "react\node_modules" (
  echo Installing React dependencies...
  cd react
  call npm install
  if errorlevel 1 goto :failed
  cd ..
)

echo Building Ghostpad UI...
cd react
set NODE_OPTIONS=--openssl-legacy-provider
call npm run build
if errorlevel 1 goto :failed
cd ..

echo Starting Ghostpad on http://127.0.0.1:3847 ...
echo.
set NODE_OPTIONS=
call npm start
if errorlevel 1 goto :failed
exit /b 0

:failed
echo.
echo Launch failed. See errors above.
pause
exit /b 1
