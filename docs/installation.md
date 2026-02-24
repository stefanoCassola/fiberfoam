# Installation

FiberFoam can be installed from source using CMake or deployed with Docker.

---

## Dependencies

### Required

| Dependency | Minimum Version | Purpose |
|---|---|---|
| C++ compiler | C++17 (GCC 9+, Clang 10+) | Core library and applications |
| CMake | 3.20 | Build system |
| Eigen3 | 3.3 | Linear algebra |
| FFTW3 | 3.3 | Fourier transforms for analysis |
| yaml-cpp | 0.6 | YAML configuration parsing |
| nlohmann-json | 3.9 | JSON I/O for GUI communication |
| OpenFOAM | v2312 | CFD solver runtime (`simpleFoamMod`) |

### Optional

| Dependency | Purpose |
|---|---|
| ONNX Runtime | ML velocity prediction (`fiberFoamPredict`, `fiberFoamConvertModel`) |
| GTest | Unit and integration tests |
| Python 3.10+ | Web GUI backend (FastAPI) |
| Node.js 20+ | Web GUI frontend (React + Vite) |

---

## Build from Source

### 1. Install system dependencies

On Ubuntu 22.04 or later:

```bash
sudo apt-get update
sudo apt-get install -y \
    cmake g++ make pkg-config \
    libeigen3-dev libfftw3-dev \
    libyaml-cpp-dev nlohmann-json3-dev \
    libgtest-dev
```

### 2. Install OpenFOAM v2312

Follow the official OpenFOAM installation guide at [openfoam.com](https://www.openfoam.com/download). After installation, source the environment:

```bash
source /usr/lib/openfoam/openfoam2312/etc/bashrc
```

### 3. (Optional) Install ONNX Runtime

To enable ML prediction, install ONNX Runtime:

```bash
# From the official release (example for x86_64 Linux)
wget https://github.com/microsoft/onnxruntime/releases/download/v1.17.0/onnxruntime-linux-x64-1.17.0.tgz
tar xzf onnxruntime-linux-x64-1.17.0.tgz
sudo cp -r onnxruntime-linux-x64-1.17.0/include/* /usr/local/include/
sudo cp -r onnxruntime-linux-x64-1.17.0/lib/* /usr/local/lib/
sudo ldconfig
```

### 4. Build FiberFoam

```bash
git clone https://github.com/stefanoCassola/fiberfoam.git
cd fiberfoam
mkdir build && cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_ONNX=ON \
    -DBUILD_TESTS=ON

cmake --build . -j$(nproc)
```

### 5. Install

```bash
sudo cmake --install .
```

This installs the following executables to `/usr/local/bin/`:

- `fiberFoamMesh`
- `fiberFoamPostProcess`
- `fiberFoamRun`
- `fiberFoamInfo`
- `fiberFoamPredict` (requires ONNX)
- `fiberFoamConvertModel` (requires ONNX)

### 6. Build the modified OpenFOAM solver

The `simpleFoamMod` solver must be compiled within the OpenFOAM environment:

```bash
source /usr/lib/openfoam/openfoam2312/etc/bashrc
cd src/solver/simpleFoamMod
wmake
```

This produces the `simpleFoamMod` executable in your OpenFOAM user applications directory.

---

## CMake Options

| Option | Default | Description |
|---|---|---|
| `BUILD_TESTS` | `ON` | Build unit and integration tests |
| `BUILD_GUI` | `OFF` | Build web GUI backend |
| `ENABLE_ONNX` | `ON` | Enable ONNX Runtime for ML inference |

To disable ONNX (builds only the core mesh and post-processing tools):

```bash
cmake .. -DENABLE_ONNX=OFF
```

---

## Docker

The project includes a multi-stage Dockerfile that packages everything into a single image:

```bash
cd fiberfoam
docker build -t fiberfoam:latest -f docker/Dockerfile .
```

### Running the Docker image

```bash
docker run -d \
    -p 8000:8000 \
    -v /path/to/your/data:/data/uploads \
    -v /path/to/models:/app/models \
    fiberfoam:latest
```

The Docker image includes:

- **Stage 1**: Builds the C++ library and applications (Ubuntu 22.04 base)
- **Stage 2**: Builds the React frontend (Node.js 20 base)
- **Stage 3**: Runtime image based on `openfoam/openfoam2312-default` with Python, C++ binaries, and the frontend

The web GUI is available at `http://localhost:8000` after starting the container.

---

## Verifying the Installation

```bash
# Check that core tools are available
fiberFoamInfo --version

# Check that the solver is available (requires OpenFOAM environment)
which simpleFoamMod

# Run the test suite (if built with BUILD_TESTS=ON)
cd build
ctest --output-on-failure
```

---

## Troubleshooting

**ONNX Runtime not found**: If CMake cannot find ONNX Runtime, ensure the library is installed in a standard location (`/usr/local/lib`) and run `sudo ldconfig`. You can also pass the path explicitly:

```bash
cmake .. -DCMAKE_PREFIX_PATH=/path/to/onnxruntime
```

**OpenFOAM environment not sourced**: The `simpleFoamMod` solver and the `fiberFoamRun` tool require the OpenFOAM environment. Make sure to source the OpenFOAM bashrc before running:

```bash
source /usr/lib/openfoam/openfoam2312/etc/bashrc
```

**Eigen3 not found**: On some systems, Eigen3 is installed in a non-standard location. Point CMake to it:

```bash
cmake .. -DEigen3_DIR=/usr/lib/cmake/eigen3
```
