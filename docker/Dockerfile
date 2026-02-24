# =============================================================================
# fiberFoam Full Dockerfile: OpenFOAM + C++ core + GUI
# Multi-stage build for minimal final image size
# =============================================================================

# ---------------------------------------------------------------------------
# Stage 1: Build C++ core library and applications
# ---------------------------------------------------------------------------
FROM ubuntu:22.04 AS builder-cpp

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        cmake \
        g++ \
        make \
        pkg-config \
        libeigen3-dev \
        libfftw3-dev \
        libyaml-cpp-dev \
        nlohmann-json3-dev \
        libgtest-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY CMakeLists.txt ./
COPY src/ ./src/

RUN mkdir -p out && cd out \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_ONNX=OFF \
        -DBUILD_TESTS=OFF \
        -DBUILD_GUI=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
    && cmake --build . -j"$(nproc)" \
    && cmake --install .

# ---------------------------------------------------------------------------
# Stage 2: Build React frontend
# ---------------------------------------------------------------------------
FROM node:20-slim AS builder-frontend

WORKDIR /app

COPY gui/frontend/package.json gui/frontend/package-lock.json* ./
RUN npm install --frozen-lockfile || npm install

COPY gui/frontend/ ./
RUN npm run build

# ---------------------------------------------------------------------------
# Stage 3: Runtime image with OpenFOAM + C++ binaries + GUI
# ---------------------------------------------------------------------------
FROM openfoam/openfoam2312-default AS runtime

USER root

# Install Python and runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3-pip \
        python3-venv \
        libfftw3-3 \
        libyaml-cpp0.7 \
    && rm -rf /var/lib/apt/lists/*

# Copy C++ binaries and libraries from builder
COPY --from=builder-cpp /usr/local/bin/ /usr/local/bin/
COPY --from=builder-cpp /usr/local/lib/ /usr/local/lib/
RUN ldconfig

# Copy frontend build artifacts
COPY --from=builder-frontend /app/dist /app/frontend/dist

# Set up Python backend
WORKDIR /app/backend
COPY gui/backend/ ./

RUN pip3 install --no-cache-dir -r requirements.txt 2>/dev/null \
    || echo "No requirements.txt found; skipping pip install"

# Create data directories
RUN mkdir -p /data/uploads /app/models

ENV FIBERFOAM_UPLOAD_DIR=/data/uploads
ENV FIBERFOAM_MODELS_DIR=/app/models

EXPOSE 8000

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
