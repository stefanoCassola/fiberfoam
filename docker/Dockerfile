# =============================================================================
# fiberFoam Full Dockerfile: OpenFOAM + C++ core + GUI
# Multi-stage build for minimal final image size
#
# Stages:
#   1. builder-cpp      – compile C++ core (libfiberfoam + apps) + download ONNX Runtime
#   2. builder-solver   – compile simpleFoamMod against OpenFOAM 2312
#   3. openfoam-runtime – extract only the OpenFOAM libs & binaries we need
#   4. builder-frontend – build React frontend
#   5. runtime          – slim ubuntu:22.04 with everything assembled
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
        curl \
        ca-certificates \
        libeigen3-dev \
        libfftw3-dev \
        libyaml-cpp-dev \
        nlohmann-json3-dev \
        libgtest-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /build

COPY CMakeLists.txt ./
COPY src/ ./src/
# Download ONNX Runtime (cached in Docker layer)
RUN mkdir -p external && \
    curl -sL https://github.com/microsoft/onnxruntime/releases/download/v1.17.1/onnxruntime-linux-x64-1.17.1.tgz \
    | tar xz -C external/

RUN mkdir -p out && cd out \
    && cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DENABLE_ONNX=ON \
        -DBUILD_TESTS=OFF \
        -DBUILD_GUI=OFF \
        -DCMAKE_INSTALL_PREFIX=/usr/local \
    && cmake --build . -j"$(nproc)" \
    && cmake --install .

# ---------------------------------------------------------------------------
# Stage 2: Build OpenFOAM solver (simpleFoamMod)
# ---------------------------------------------------------------------------
FROM opencfd/openfoam-default:2312 AS builder-solver

USER root

RUN apt-get update && apt-get install -y libeigen3-dev && rm -rf /var/lib/apt/lists/*

COPY src/solver/simpleFoamMod/ /tmp/simpleFoamMod/

ENV EIGEN_DIR=/usr/include/eigen3

RUN bash -c 'source /usr/lib/openfoam/openfoam2312/etc/bashrc && cd /tmp/simpleFoamMod && wmake'

# ---------------------------------------------------------------------------
# Stage 3: Extract minimal OpenFOAM runtime (libs + foamToVTK only)
# ---------------------------------------------------------------------------
FROM opencfd/openfoam-default:2312 AS openfoam-runtime

USER root

# Collect only the shared libraries and binaries actually needed at runtime.
# simpleFoamMod links:  OpenFOAM finiteVolume fvOptions meshTools sampling
#                       turbulenceModels incompressibleTurbulenceModels
#                       incompressibleTransportModels atmosphericModels
# foamToVTK pulls additional transitive deps (see ldd output).
# We use ldd to discover them all automatically, plus the dummy/ and
# sys-openmpi/ Pstream stubs.
RUN bash -c '\
    set -e; \
    source /usr/lib/openfoam/openfoam2312/etc/bashrc; \
    OF=/usr/lib/openfoam/openfoam2312; \
    OUT=/opt/openfoam; \
    mkdir -p $OUT/lib $OUT/bin $OUT/etc $OUT/lib/dummy $OUT/lib/sys-openmpi; \
    \
    # Collect all OpenFOAM .so deps from foamToVTK and simpleFoam (proxy for simpleFoamMod) \
    for bin in $(which foamToVTK) $(which simpleFoam); do \
        ldd $bin 2>/dev/null | grep openfoam | awk "{print \$3}" >> /tmp/of-libs.txt; \
    done; \
    sort -u /tmp/of-libs.txt | while read lib; do \
        cp -L "$lib" $OUT/lib/ 2>/dev/null || true; \
    done; \
    \
    # Copy dummy and sys-openmpi Pstream stubs \
    cp $OF/platforms/linux64GccDPInt32Opt/lib/dummy/* $OUT/lib/dummy/ 2>/dev/null || true; \
    cp $OF/platforms/linux64GccDPInt32Opt/lib/sys-openmpi/* $OUT/lib/sys-openmpi/ 2>/dev/null || true; \
    \
    # Copy foamToVTK binary \
    cp $(which foamToVTK) $OUT/bin/; \
    \
    # Copy minimal etc/ (bashrc + config) for environment sourcing \
    cp -r $OF/etc/* $OUT/etc/; \
    \
    echo "OpenFOAM runtime extraction complete"; \
    du -sh $OUT/; \
    '

# ---------------------------------------------------------------------------
# Stage 4: Build React frontend
# ---------------------------------------------------------------------------
FROM node:20-slim AS builder-frontend

WORKDIR /app

COPY gui/frontend/package.json gui/frontend/package-lock.json* ./
RUN npm install --frozen-lockfile || npm install

COPY gui/frontend/ ./
RUN npm run build

# ---------------------------------------------------------------------------
# Stage 5: Slim runtime image (ubuntu base instead of full OpenFOAM)
# ---------------------------------------------------------------------------
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive

# Install only runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
        python3-pip \
        python3-venv \
        libfftw3-3 \
        libyaml-cpp0.7 \
        libstdc++6 \
        libgomp1 \
        gosu \
        curl \
        # OpenFOAM system-level runtime deps (OpenMPI, hwloc, libevent, zlib)
        libopenmpi3 \
        libhwloc15 \
        libevent-2.1-7 \
        zlib1g \
    && rm -rf /var/lib/apt/lists/*

# Copy C++ binaries and libraries from builder
COPY --from=builder-cpp /usr/local/bin/ /usr/local/bin/
COPY --from=builder-cpp /usr/local/lib/ /usr/local/lib/

# Copy ONNX Runtime shared libraries
COPY --from=builder-cpp /build/external/onnxruntime-linux-x64-1.17.1/lib/libonnxruntime.so* /usr/local/lib/

# Copy simpleFoamMod solver binary from solver builder
COPY --from=builder-solver /root/OpenFOAM/user-v2312/platforms/linux64GccDPInt32Opt/bin/simpleFoamMod /usr/local/bin/simpleFoamMod

# Copy minimal OpenFOAM runtime (libs + foamToVTK + etc)
COPY --from=openfoam-runtime /opt/openfoam/lib/ /usr/lib/openfoam/openfoam2312/platforms/linux64GccDPInt32Opt/lib/
COPY --from=openfoam-runtime /opt/openfoam/bin/ /usr/lib/openfoam/openfoam2312/platforms/linux64GccDPInt32Opt/bin/
COPY --from=openfoam-runtime /opt/openfoam/etc/ /usr/lib/openfoam/openfoam2312/etc/

RUN ldconfig

# Copy frontend build artifacts
COPY --from=builder-frontend /app/dist /app/frontend/dist

# Copy ONNX models and scaling factors
COPY models/ /app/models/
COPY models/scaling_factors.json /app/models/scaling_factors.json

# Set up Python backend
WORKDIR /app/backend
COPY gui/backend/ ./

RUN pip3 install --no-cache-dir -r requirements.txt onnxruntime

# Create data directories
RUN mkdir -p /data/cases /data/input /data/jobs /data/uploads /app/models

ARG FIBERFOAM_VERSION=dev
ENV FIBERFOAM_VERSION=${FIBERFOAM_VERSION}

ENV FIBERFOAM_UPLOAD_DIR=/data/uploads
ENV FIBERFOAM_MODELS_DIR=/app/models
ENV FIBERFOAM_WORK_DIR=/data/cases
ENV FIBERFOAM_BATCH_DIR=/data/input
ENV FIBERFOAM_SCALING_FACTORS=/app/models/scaling_factors.json
ENV LD_LIBRARY_PATH=/usr/local/lib:/usr/lib/openfoam/openfoam2312/platforms/linux64GccDPInt32Opt/lib:/usr/lib/openfoam/openfoam2312/platforms/linux64GccDPInt32Opt/lib/dummy:${LD_LIBRARY_PATH}

EXPOSE 8000

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD curl -f http://localhost:8000/api/health || exit 1

COPY docker/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
