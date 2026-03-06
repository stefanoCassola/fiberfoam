# FiberFoam Docker launcher for Windows PowerShell
$ErrorActionPreference = "Stop"

Push-Location $PSScriptRoot
try {
    # Defaults (override via environment before running)
    if (-not $env:FIBERFOAM_PORT) { $env:FIBERFOAM_PORT = "3000" }
    if (-not $env:FIBERFOAM_INPUT_DIR) { $env:FIBERFOAM_INPUT_DIR = "./input" }

    # Check Docker is installed
    try {
        $null = & docker --version 2>&1
    } catch {
        Write-Error "Docker is not installed or not in PATH. Install from https://docs.docker.com/get-docker/"
        exit 1
    }

    # Check Docker daemon is running
    $dockerInfo = & docker info 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Docker daemon is not running. Please start Docker Desktop and try again."
        exit 1
    }

    # Check docker compose
    $composeVersion = & docker compose version 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Error "Docker Compose is not available. Install from https://docs.docker.com/compose/install/"
        exit 1
    }

    # Create input directory if using default
    if ($env:FIBERFOAM_INPUT_DIR -eq "./input" -and -not (Test-Path "./input")) {
        New-Item -ItemType Directory -Path "./input" | Out-Null
    }

    # Build if image does not exist
    $imageId = & docker images -q "docker-fiberfoam" 2>&1
    if ([string]::IsNullOrWhiteSpace($imageId)) {
        Write-Host "Building FiberFoam Docker image (this may take several minutes)..."
        & docker compose build
        if ($LASTEXITCODE -ne 0) { throw "Docker build failed" }
    }

    # Start services
    Write-Host "Starting FiberFoam..."
    & docker compose up -d
    if ($LASTEXITCODE -ne 0) { throw "Docker compose up failed" }

    Write-Host ""
    Write-Host "FiberFoam is running at: http://localhost:$($env:FIBERFOAM_PORT)"
    Write-Host ""
    Write-Host "To view logs:  docker compose -f $PSScriptRoot\docker-compose.yml logs -f"
    Write-Host "To stop:       docker compose -f $PSScriptRoot\docker-compose.yml down"
} finally {
    Pop-Location
}
