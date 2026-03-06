#!/usr/bin/env bash
# FiberFoam Docker launcher for Linux/macOS
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Defaults (override via environment)
export FIBERFOAM_PORT="${FIBERFOAM_PORT:-3000}"
export FIBERFOAM_INPUT_DIR="${FIBERFOAM_INPUT_DIR:-./input}"
export FIBERFOAM_OUTPUT_DIR="${FIBERFOAM_OUTPUT_DIR:-./output}"

# Pass host UID/GID so the container writes files as the current user
export HOST_UID="${HOST_UID:-$(id -u)}"
export HOST_GID="${HOST_GID:-$(id -g)}"

# Check Docker is installed
if ! command -v docker &>/dev/null; then
    echo "ERROR: Docker is not installed or not in PATH."
    echo "Install Docker from https://docs.docker.com/get-docker/"
    exit 1
fi

# Check Docker daemon is running
if ! docker info &>/dev/null; then
    echo "ERROR: Docker daemon is not running. Please start Docker and try again."
    exit 1
fi

# Check docker compose is available
if docker compose version &>/dev/null; then
    COMPOSE="docker compose"
elif command -v docker-compose &>/dev/null; then
    COMPOSE="docker-compose"
else
    echo "ERROR: Docker Compose is not available."
    echo "Install it from https://docs.docker.com/compose/install/"
    exit 1
fi

# Create input directory if using default
if [ "$FIBERFOAM_INPUT_DIR" = "./input" ] && [ ! -d "./input" ]; then
    mkdir -p ./input
fi

# Create output directory if it does not exist
mkdir -p "$FIBERFOAM_OUTPUT_DIR"

# Build if image does not exist
IMAGE_NAME="docker-fiberfoam"
if [ -z "$(docker images -q "$IMAGE_NAME" 2>/dev/null)" ]; then
    echo "Building FiberFoam Docker image (this may take several minutes)..."
    $COMPOSE build
fi

# Start services
echo "Starting FiberFoam..."
$COMPOSE up -d

echo ""
echo "FiberFoam is running at: http://localhost:${FIBERFOAM_PORT}"
echo ""
echo "To view logs:  $COMPOSE -f $SCRIPT_DIR/docker-compose.yml logs -f"
echo "To stop:       $COMPOSE -f $SCRIPT_DIR/docker-compose.yml down"
