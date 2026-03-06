@echo off
REM FiberFoam Docker launcher for Windows CMD
setlocal enabledelayedexpansion

cd /d "%~dp0"

REM Defaults (override via environment before running)
if not defined FIBERFOAM_PORT set FIBERFOAM_PORT=3000
if not defined FIBERFOAM_INPUT_DIR set FIBERFOAM_INPUT_DIR=./input

REM Check Docker is installed
docker --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker is not installed or not in PATH.
    echo Install Docker from https://docs.docker.com/get-docker/
    exit /b 1
)

REM Check Docker daemon is running
docker info >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker daemon is not running. Please start Docker Desktop and try again.
    exit /b 1
)

REM Check docker compose
docker compose version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker Compose is not available.
    echo Install it from https://docs.docker.com/compose/install/
    exit /b 1
)

REM Create input directory if using default
if "%FIBERFOAM_INPUT_DIR%"=="./input" (
    if not exist "input" mkdir input
)

REM Build if image does not exist
docker images -q docker-fiberfoam >nul 2>&1
for /f %%i in ('docker images -q docker-fiberfoam 2^>nul') do set IMAGE_EXISTS=%%i
if not defined IMAGE_EXISTS (
    echo Building FiberFoam Docker image (this may take several minutes^)...
    docker compose build
)

REM Start services
echo Starting FiberFoam...
docker compose up -d

echo.
echo FiberFoam is running at: http://localhost:%FIBERFOAM_PORT%
echo.
echo To view logs:  docker compose -f "%~dp0docker-compose.yml" logs -f
echo To stop:       docker compose -f "%~dp0docker-compose.yml" down

endlocal
