# Contributing

Thank you for your interest in contributing to FiberFoam. This guide covers the development setup, code style, testing requirements, and pull request process.

---

## Development Setup

### 1. Clone the repository

```bash
git clone https://github.com/stefanoCassola/fiberfoam.git
cd fiberfoam
```

### 2. Install dependencies

See the [Installation](../installation.md) page for the full list. For development, you also need:

- **GTest** for unit tests
- **clang-format** (version 14+) for code formatting
- **Python 3.10+** and **Node.js 20+** if working on the GUI

### 3. Build in Debug mode

```bash
mkdir build && cd build
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTS=ON \
    -DENABLE_ONNX=ON \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build . -j$(nproc)
```

The `CMAKE_EXPORT_COMPILE_COMMANDS=ON` flag generates `compile_commands.json` for IDE integration (clangd, VS Code, CLion).

### 4. Run the test suite

```bash
cd build
ctest --output-on-failure
```

---

## Code Style

### C++

- **Standard**: C++17
- **Naming conventions**:
    - Classes and structs: `PascalCase` (e.g., `HexMeshBuilder`, `FlowDirection`)
    - Functions and methods: `camelCase` (e.g., `generateCellMap`, `computeROIBounds`)
    - Member variables: `camelCase_` with trailing underscore (e.g., `geometry_`, `opts_`)
    - Constants and enum values: `PascalCase` (e.g., `FlowDirection::X`, `CellRegion::Fibrous`)
    - Namespaces: `lowercase` (e.g., `fiberfoam`)
- **Formatting**: Use `clang-format` with the project's `.clang-format` configuration. Format before committing:

```bash
clang-format -i src/libfiberfoam/**/*.cpp src/libfiberfoam/**/*.h
```

- **Headers**: Use `#pragma once` for include guards
- **Includes**: Group in order: (1) corresponding header, (2) project headers, (3) third-party headers, (4) standard library headers. Separate groups with blank lines.

### Python (GUI backend)

- Follow PEP 8
- Use type hints for function signatures
- Use `black` for formatting and `ruff` for linting

### TypeScript (GUI frontend)

- Follow the project ESLint configuration
- Use functional React components with hooks

---

## Project Structure Conventions

- All domain logic goes in `src/libfiberfoam/`. Applications should be thin wrappers.
- Each module (geometry, mesh, ml, etc.) is a subdirectory under `libfiberfoam/` with its own header and source files.
- Public interfaces are defined in header files (`.h`). Implementation details stay in source files (`.cpp`).
- New source files must be added to the `FIBERFOAM_SOURCES` list in `src/libfiberfoam/CMakeLists.txt`.
- New applications follow the `add_fiberfoam_app()` pattern in `src/apps/CMakeLists.txt`.

---

## Testing

### Unit tests

Unit tests use Google Test and live in the `tests/` directory. Each module should have corresponding tests:

```
tests/
    test_voxelarray.cpp
    test_hexmeshbuilder.cpp
    test_connectivity.cpp
    test_permeability.cpp
    ...
```

### Writing tests

- Test files are named `test_<module>.cpp`
- Use descriptive test names: `TEST(HexMeshBuilder, SingleVoxelProducesEightPoints)`
- Test edge cases: empty geometry, single voxel, fully solid, fully fluid
- For numerical tests, use `EXPECT_NEAR` with appropriate tolerances

### Running specific tests

```bash
cd build
# Run all tests
ctest --output-on-failure

# Run a specific test binary
./tests/test_hexmeshbuilder

# Run tests matching a pattern
ctest -R permeability
```

---

## Pull Request Process

### 1. Create a feature branch

```bash
git checkout -b feature/my-improvement
```

### 2. Make your changes

- Keep commits focused and atomic
- Write clear commit messages describing what and why
- Add or update tests for any new functionality
- Update documentation if the public interface changes

### 3. Verify before submitting

```bash
# Build
cmake --build build -j$(nproc)

# Run tests
cd build && ctest --output-on-failure

# Format code
clang-format -i <changed files>
```

### 4. Submit a pull request

- Push your branch to your fork and open a PR against the `main` branch
- Provide a clear description of the changes and their motivation
- Reference any related issues
- Include before/after benchmarks if the change affects performance

### 5. Code review

- Address reviewer feedback promptly
- Keep the PR focused; split unrelated changes into separate PRs
- Ensure all CI checks pass before requesting a merge

---

## Reporting Issues

When reporting a bug, please include:

- FiberFoam version (`fiberFoamInfo --version`)
- Operating system and compiler version
- OpenFOAM version
- Steps to reproduce the issue
- Expected vs. actual behavior
- Relevant log output

---

## Areas for Contribution

Some areas where contributions are particularly welcome:

- **Additional ML model architectures**: New model formats or training pipelines
- **Parallel mesh generation**: OpenMP or MPI parallelization of `HexMeshBuilder`
- **Additional boundary conditions**: Non-uniform pressure, velocity inlet profiles
- **Visualization**: VTK export, ParaView integration
- **Documentation**: Tutorials, examples, theory references
