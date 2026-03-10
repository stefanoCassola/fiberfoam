# GUI Guide

FiberFoam includes a web-based GUI built with FastAPI (backend) and React/TypeScript/Tailwind CSS (frontend). It provides an interactive workflow for uploading geometry, preprocessing, running simulations, and viewing permeability results.

---

## Accessing the GUI

### Online (Vercel)

Visit **[fiberfoam.vercel.app](https://fiberfoam.vercel.app)**.

The web app detects whether a local backend is running. If not, it provides a guided setup flow:

1. Detects your operating system (Windows, macOS, Linux)
2. Links to the Docker installer for your platform
3. Provides the `docker run` command to start the backend
4. Auto-retries the connection every 5 seconds and proceeds once detected

All computation runs on your local machine — the Vercel site is only the frontend interface.

### Docker (recommended for offline use)

```bash
cd docker
./launch.sh
```

Open your browser at `http://localhost:3000`.

The launch script handles Docker Compose, creates required directories, and passes your user UID/GID so that output files are owned by your host user.

### Environment Variables

| Variable | Default | Description |
|---|---|---|
| `FIBERFOAM_PORT` | `3000` | Host port for the web GUI |
| `FIBERFOAM_INPUT_DIR` | `./input` | Host directory with geometry files (read-only mount) |
| `FIBERFOAM_OUTPUT_DIR` | `./output` | Host directory for simulation results |

### Local Development

Start the backend:

```bash
cd gui/backend
pip install -r requirements.txt
uvicorn main:app --host 0.0.0.0 --port 8000 --reload
```

Start the frontend (separate terminal):

```bash
cd gui/frontend
npm install
npm run dev
```

The frontend dev server runs at `http://localhost:5173` and proxies API requests to the backend.

---

## Single Pipeline Workflow

The **Pipeline** page guides you through the full simulation workflow in sequential steps.

### Step 1: Upload

- **Upload** a binary voxel geometry file (`.dat`, `.npy`, `.raw`)
- **3D Viewer** — toggle an interactive visualization of the geometry with orbit controls, auto-rotate, and zoom

### Step 2: Preprocess

- **Analyze** the file to see voxel dimensions, unique values, and value counts
- **Remap** values if needed (select which value represents pore space, choose how to treat other values)
- **Orientation** — estimate fiber alignment via FFT; manually rotate or auto-align to canonical axes

### Step 3: Mode Selection

- **Pipeline Mode**: Mesh Only, Prediction Only, Mesh + Predict, or Full Simulation

### Step 4: Configure

- **Flow Directions**: X, Y, Z (one or more)
- **Voxel Size** (physical size per voxel in meters)
- **Voxel Resolution** (number of voxels along one edge of the cubic domain)
- **ML Model** selection (when prediction is enabled)
- **Buffer Zones**: inlet and outlet buffer layers (voxels)
- **Connectivity Check**: remove disconnected fluid regions before meshing
- **Solver Settings**: max iterations, write interval, convergence criteria
- **Output Folder**: choose where to save results using the built-in folder picker

### Step 5: Review

- Summary of all selected settings before execution

### Step 6: Progress

- **Live progress** per step with status indicators
- **Convergence charts** showing residuals (Ux, Uy, Uz, p) and permeability convergence for each flow direction separately. When simulating multiple directions, each gets its own chart with a direction label
- **Regression line** over the convergence window, showing the permeability stabilization trend
- **RAM monitor** showing live memory usage
- **Stop & Write** button to gracefully stop the solver and write the current state
- **Cancel** button to abort the entire pipeline

### Step 7: Results

- Permeability values per flow direction (volume-averaged and flow-rate methods)
- **Full 3x3 permeability tensor** (volume-averaged) when all three flow directions (X, Y, Z) are simulated, including off-diagonal components. Diagonal elements are highlighted
- Fiber volume content, flow length, cross-section area
- **Download CSV** with all results
- **Save to Folder** to copy case directories to a chosen location

---

## Batch Processing

The **Batch** page processes multiple geometry files with identical settings.

### File Selection

1. Click **Browse** to open the server-side file browser
2. Navigate directories to find your geometry files
3. All `.dat`, `.npy`, and `.raw` files are listed with checkboxes
4. Tick/untick individual files or use **Select All / Deselect All**

### Preprocessing (Optional)

- **Remap Values**: Apply value remapping to all selected files (choose pore value and mapping for other values)
- **Auto-Align**: Automatically detect and correct fiber orientation for all files

Preprocessing is applied identically to every selected file before its pipeline execution.

### Configuration

Same options as the single pipeline: mode, flow directions, voxel size, resolution, ML model, buffer zones, connectivity check, and output folder.

### Execution

- Click **Run Batch** — selected files are processed sequentially
- Progress table shows each file's current step, status, and progress bar
- **RAM monitor** shows live memory usage during processing
- After completion, a summary shows total/completed/failed counts
- **Export All as CSV** downloads results for all completed runs

### Session Persistence

Batch processing continues in the background if you navigate away from the Batch page. When you return, the page automatically restores the running batch and resumes progress display.

---

## Settings & Connection

The **Settings** button in the sidebar opens a dialog to configure the backend connection:

- View and change the backend URL (default: `http://localhost:3000/api`)
- **Test** the connection to verify reachability
- **Reset** to auto-detected defaults

The sidebar also shows a **connection status indicator** (green = connected, red = disconnected).

---

## Update Checking

When connected to the backend, the landing page automatically checks for newer versions of the Docker image. If an update is available, a yellow banner shows the commands to pull the latest image and restart the container. The current version is displayed in the status section.

Version information is embedded in the Docker image at build time and returned by the `/api/health` endpoint.

---

## ML Model Selection

FiberFoam ships with two neural network architectures for velocity field prediction:

| Model | Parameters | Format | Strengths |
|-------|-----------|--------|-----------|
| **3D U-Net** | ~1.4M | ONNX | Fast inference, low memory |
| **FNO** (Fourier Neural Operator) | ~28.3M | TensorFlow SavedModel | Higher capacity |

Select the model in **Step 4: Configure** when prediction is enabled. Both models are available at 80-voxel resolution.

**Note:** Inaccurate predictions can lead to longer convergence times than starting from scratch. Monitor the convergence chart — if residuals increase or stall, consider re-running without prediction.

---

## Feedback

The **Feedback** button in the sidebar opens a form to submit:

- **Bug reports**, **feature requests**, **questions**, or **general feedback**
- Optional contact information for follow-up
- No account required — submissions are stored locally (Docker) or create GitHub issues automatically (Vercel)

---

## API Endpoints

The backend exposes REST APIs with interactive documentation at `/docs` (Swagger) and `/redoc`.

| Prefix | Description |
|---|---|
| `/api/geometry` | Upload and inspect geometry files |
| `/api/preprocess` | Analyze, remap, rotate, auto-align geometry |
| `/api/prediction` | ML velocity prediction, model listing |
| `/api/pipeline` | Single pipeline and batch orchestration |
| `/api/results` | Download results, CSV export |
| `/api/filesystem` | Server-side folder browsing, save results |
| `/api/feedback` | Submit feedback |
| `/api/system/stats` | RAM usage monitoring |
| `/api/health` | Health check and version info |
| `/api/updates/check` | Check for newer Docker image versions |

---

## Tips

- The Docker container includes OpenFOAM v2312 and all dependencies — no host installation needed
- Output files are owned by your host user (UID/GID auto-detected from the output mount)
- For large geometries (resolution > 200), mesh generation and simulation may take significant time
- The 3D geometry viewer renders only exposed surface faces for efficient visualization of large voxel models
- The folder browser can access all host mount points (configurable via `FIBERFOAM_BROWSE_ROOT`)
- All GUI operations map to the same backend APIs available from the command line
