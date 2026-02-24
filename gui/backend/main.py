from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from routes import geometry, prediction, mesh, simulation, postprocess
import os

app = FastAPI(
    title="FiberFoam",
    version="0.1.0",
    description="Web GUI for fiber foam flow simulation",
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.include_router(geometry.router, prefix="/api/geometry", tags=["geometry"])
app.include_router(prediction.router, prefix="/api/prediction", tags=["prediction"])
app.include_router(mesh.router, prefix="/api/mesh", tags=["mesh"])
app.include_router(simulation.router, prefix="/api/simulation", tags=["simulation"])
app.include_router(postprocess.router, prefix="/api/postprocess", tags=["postprocess"])

# Serve static frontend files in production
frontend_dir = os.path.join(os.path.dirname(__file__), "..", "frontend", "dist")
if os.path.isdir(frontend_dir):
    app.mount("/", StaticFiles(directory=frontend_dir, html=True), name="frontend")


@app.get("/api/health")
async def health():
    return {"status": "ok", "version": "0.1.0"}
