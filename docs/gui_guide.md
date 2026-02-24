# GUI Guide

FiberFoam includes a web-based graphical user interface built with a FastAPI backend and a React (Vite + TypeScript + Tailwind CSS) frontend. The GUI provides an interactive workflow for uploading geometry, configuring simulations, running the pipeline, and viewing results.

---

## Accessing the GUI

### Docker (recommended)

```bash
docker run -d \
    -p 8000:8000 \
    -v /path/to/data:/data/uploads \
    -v /path/to/models:/app/models \
    fiberfoam:latest
```

Open your browser at `http://localhost:8000`.

### Local development

Start the backend:

```bash
cd gui/backend
pip install -r requirements.txt
uvicorn main:app --host 0.0.0.0 --port 8000 --reload
```

Start the frontend (in a separate terminal):

```bash
cd gui/frontend
npm install
npm run dev
```

The frontend development server runs at `http://localhost:5173` and proxies API requests to the backend at `http://localhost:8000`.

---

## API Endpoints

The backend exposes the following REST API groups:

| Prefix | Tag | Description |
|---|---|---|
| `/api/geometry` | geometry | Upload and inspect geometry files |
| `/api/prediction` | prediction | Run ML velocity prediction |
| `/api/mesh` | mesh | Generate OpenFOAM meshes |
| `/api/simulation` | simulation | Launch and monitor solver runs |
| `/api/postprocess` | postprocess | Extract permeability results |
| `/api/health` | -- | Health check endpoint |

Interactive API documentation is available at `http://localhost:8000/docs` (Swagger UI) and `http://localhost:8000/redoc` (ReDoc).

---

## Workflow Walkthrough

### 1. Upload Geometry

Navigate to the **Geometry** page. Upload a binary voxel geometry file (`.dat`). The GUI displays:

- Voxel resolution and domain dimensions
- Solid fraction and estimated fiber volume content
- A 3D preview of the microstructure (rendered as a voxel visualization)

Configure the voxel resolution and voxel size if they differ from the defaults (320 and 0.5e-6 m).

### 2. Configure Simulation

On the **Configuration** page, set the simulation parameters:

- **Flow directions**: Select one or more of X, Y, Z
- **Fluid properties**: Kinematic viscosity, density, dynamic viscosity, inlet/outlet pressures
- **Buffer zones**: Number of inlet and outlet buffer layers
- **Mesh options**: Connectivity check, periodic boundaries
- **Solver settings**: Maximum iterations, write interval, convergence criteria
- **Post-processing**: Permeability method (volume-averaged, flow-rate, or both), fibrous-region-only filtering

The configuration can be exported as a YAML file for command-line use.

### 3. Generate Mesh

Click **Generate Mesh** to create OpenFOAM case directories for each selected flow direction. The GUI shows progress and reports:

- Number of cells, points, and faces generated
- Number of cells removed by connectivity filtering (if enabled)
- Buffer region cell counts (inlet and outlet)
- Boundary patch summary

### 4. Predict Velocity (Optional)

If ONNX models are available (configured via the `FIBERFOAM_MODELS_DIR` environment variable or the models directory setting), the **Prediction** page allows you to generate initial velocity fields:

- Select the model resolution (e.g., 80)
- Choose which direction(s) to predict
- View a slice visualization of the predicted velocity field

### 5. Run Simulation

The **Simulation** page launches the `simpleFoamMod` solver for each flow direction. During the run, the GUI displays:

- Real-time solver log output
- Residual convergence plots (U and p)
- Permeability convergence curve (updated each iteration)
- Current iteration count and estimated time remaining

The solver runs within the OpenFOAM environment inside the Docker container (or on the host if running locally with OpenFOAM installed).

### 6. View Results

After the simulation completes, the **Results** page shows:

- Permeability tensor components for each flow direction
- Fiber volume content
- Comparison between volume-averaged and flow-rate methods
- Convergence history plots
- Option to download `permeabilityInfo.csv` files

---

## Environment Variables

The following environment variables configure the GUI backend:

| Variable | Default | Description |
|---|---|---|
| `FIBERFOAM_UPLOAD_DIR` | `/data/uploads` | Directory for uploaded geometry files |
| `FIBERFOAM_MODELS_DIR` | `/app/models` | Directory containing ONNX model files |

---

## Tips

- The GUI is fully functional inside the Docker container, which includes OpenFOAM and all required dependencies.
- For large geometries, the mesh generation step may take several minutes. The progress indicator updates in real time.
- All operations performed through the GUI can also be executed from the command line using the same parameters. Use the "Export Config" button to generate a YAML configuration file.
