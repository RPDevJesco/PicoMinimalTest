@echo off
REM build.bat — Build (and optionally flash) pico2-ili9341 via Docker
REM
REM Usage:
REM   build.bat              — build only
REM   build.bat flash        — build + copy .uf2 to mounted Pico
REM   build.bat clean        — remove build directory
REM

setlocal enabledelayedexpansion

set IMAGE_NAME=pico2-ili9341-build
set BUILD_DIR=build
set TARGET=pico2_ili9341
set SCRIPT_DIR=%~dp0

REM Default Pico mount drive — override: set PICO_MOUNT=E:\ before running
if not defined PICO_MOUNT set PICO_MOUNT=D:\

set CMD=%1
if "%CMD%"=="" set CMD=build

if /i "%CMD%"=="clean" goto :clean
if /i "%CMD%"=="build" goto :build
if /i "%CMD%"=="flash" goto :build
goto :usage

:clean
echo ^>^>^> Cleaning build directory
if exist "%SCRIPT_DIR%%BUILD_DIR%" rmdir /s /q "%SCRIPT_DIR%%BUILD_DIR%"
echo ^>^>^> Done
goto :eof

:build
REM ---- Build Docker image (cached unless Dockerfile changes) ----
echo ^>^>^> Building Docker image: %IMAGE_NAME%
docker build -t %IMAGE_NAME% "%SCRIPT_DIR%."
if errorlevel 1 (
    echo ERROR: Docker image build failed
    exit /b 1
)

REM ---- Run build inside container ----
echo ^>^>^> Compiling inside container
docker run --rm -v "%SCRIPT_DIR%.":/project %IMAGE_NAME% ^
    bash -c "mkdir -p %BUILD_DIR% && cd %BUILD_DIR% && cmake -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s .. && make -j$(nproc)"
if errorlevel 1 (
    echo ERROR: Build failed
    exit /b 1
)

set UF2=%SCRIPT_DIR%%BUILD_DIR%\src\%TARGET%.uf2
echo.
echo ^>^>^> UF2 ready: %UF2%

REM ---- Flash if requested ----
if /i not "%CMD%"=="flash" goto :eof

if not exist "%PICO_MOUNT%" (
    echo ERROR: Pico not mounted at %PICO_MOUNT%
    echo        Hold BOOTSEL, plug in, then retry.
    echo        Or override: set PICO_MOUNT=E:\ before running.
    exit /b 1
)

copy "%UF2%" "%PICO_MOUNT%\"
if errorlevel 1 (
    echo ERROR: Copy to %PICO_MOUNT% failed
    exit /b 1
)
echo ^>^>^> Flashed to %PICO_MOUNT%
goto :eof

:usage
echo Usage: build.bat [build^|flash^|clean]
exit /b 1
