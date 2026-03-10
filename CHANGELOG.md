# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2026-03-10

### Added

#### Core
- C++ core library (libfiberfoam) with geometry, mesh, analysis, ML, I/O, and post-processing modules
- CLI applications: fiberFoamMesh, fiberFoamPredict, fiberFoamPostProcess, fiberFoamRun, fiberFoamInfo, fiberFoamConvertModel
- simpleFoamMod solver — extended OpenFOAM SIMPLE algorithm with permeability convergence criterion and automatic stopping
- Fiber-free buffer region support (configurable inlet/outlet layers)
- Dual-method permeability calculation (volume-averaged velocity and flow-rate/Darcy)
- Full 3x3 permeability tensor display when all three flow directions are simulated
- FFT-based fiber orientation estimation and automatic correction
- Connectivity filtering to remove disconnected fluid pockets

#### ML Prediction
- 3D U-Net models (~1.4M parameters, ONNX format) for velocity field prediction at 80-voxel resolution
- FNO (Fourier Neural Operator) models (~28.3M parameters, TensorFlow SavedModel) as an alternative architecture
- Model selection UI in the pipeline configuration

#### Web GUI
- React + TypeScript + Tailwind CSS frontend with Vite build
- FastAPI backend with async job orchestration
- Single pipeline workflow: Upload, Preprocess, Configure, Review, Run, Results
- Batch processing: multi-file selection with shared settings, sequential execution
- Interactive 3D voxel geometry viewer (Three.js)
- Live convergence charts (per-direction) with regression line overlay
- Real-time RAM usage monitoring
- Job history page with re-run and result viewing
- Server-side folder browser for output directory selection
- CSV export for single and batch results
- ParaView integration: launch visualization directly from the GUI
- Landing page with guided Docker setup, OS detection, and copy-paste commands
- Automatic Docker image update checking with version display

#### Infrastructure
- Multi-stage Docker build (C++ builder, OpenFOAM solver, frontend, runtime)
- Docker image auto-published to ghcr.io on push to main
- CI pipeline: C++ build, unit tests, integration tests, formatting check
- GitHub Release workflow for tagged versions with binary packaging
- Git LFS for ONNX and TensorFlow model files
- Dependabot for automated dependency updates

### Documentation
- README with three deployment options (Online/Vercel, Docker, Source)
- CLI reference with examples for all executables
- GUI guide covering single pipeline, batch processing, and settings
- Theory documentation for permeability calculation and fiber-free regions
- Architecture overview and contributing guide
- Interactive API documentation at `/docs` (Swagger) and `/redoc`
