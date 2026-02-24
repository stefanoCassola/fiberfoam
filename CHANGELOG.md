# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- C++ core library (libfiberfoam) with geometry, mesh, analysis, ML, I/O, and post-processing modules
- CLI applications: fiberFoamMesh, fiberFoamPredict, fiberFoamPostProcess, fiberFoamRun, fiberFoamInfo
- simpleFoamMod solver with permeability convergence criterion
- Fiber-free buffer region support
- Dual-method permeability calculation (volume-averaged + flow rate)
- FFT-based fiber orientation estimation
- ONNX Runtime ML inference
- Web GUI (React + FastAPI)
- Docker support
- Unit and integration tests
- Documentation (MkDocs)
