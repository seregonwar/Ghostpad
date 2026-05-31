@echo off
setlocal EnableDelayedExpansion

cd /d "%~dp0"
title Ghostpad - Build Portable

echo.
echo ========================================
echo   Ghostpad - Build Portable EXE
echo ========================================
echo.

where node >nul 2>&1
if errorlevel 1 (
  echo ERROR: Node.js is not installed or not in PATH.
  echo Install from https://nodejs.org/
  pause
  exit /b 1
)

if not exist "node_modules" (
  echo Installing root dependencies...
  call npm install
  if errorlevel 1 goto :failed
)

if not exist "react\node_modules" (
  echo Installing React dependencies...
  cd react
  call npm install
  if errorlevel 1 ( cd .. & goto :failed )
  cd ..
)

echo Building React frontend...
cd react
set NODE_OPTIONS=--openssl-legacy-provider
call npm run build
if errorlevel 1 ( cd .. & goto :failed )
cd ..
set NODE_OPTIONS=

echo.
echo Compiling portable executable...
call npx electron-builder --win portable
if errorlevel 1 goto :failed

echo.
echo ========================================
echo   Done! Portable EXE is in dist\
echo ========================================
echo.
pause
exit /b 0

:failed
echo.
echo Build failed. See errors above.
pause
exit /b 1
