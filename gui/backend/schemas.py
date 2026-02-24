from pydantic import BaseModel, Field
from typing import Optional
from enum import Enum


# ---------------------------------------------------------------------------
# Enums
# ---------------------------------------------------------------------------

class FlowDirectionEnum(str, Enum):
    x = "x"
    y = "y"
    z = "z"


class JobStatusEnum(str, Enum):
    running = "running"
    completed = "completed"
    failed = "failed"
    not_found = "not_found"


class PermeabilityMethod(str, Enum):
    volumeAveraged = "volumeAveraged"
    flowRate = "flowRate"
    both = "both"


# ---------------------------------------------------------------------------
# Geometry
# ---------------------------------------------------------------------------

class GeometryUploadResponse(BaseModel):
    filename: str
    resolution: int
    fluidFraction: float
    shape: list[int]


# ---------------------------------------------------------------------------
# Mesh
# ---------------------------------------------------------------------------

class MeshRequest(BaseModel):
    inputPath: str
    voxelSize: float = 0.5e-6
    voxelRes: int = 320
    flowDirection: FlowDirectionEnum = FlowDirectionEnum.x
    inletBuffer: int = 0
    outletBuffer: int = 0
    connectivity: bool = True


class MeshResponse(BaseModel):
    jobId: str
    caseDir: str
    nCells: Optional[int] = None
    nPoints: Optional[int] = None
    nFaces: Optional[int] = None
    nInternalFaces: Optional[int] = None


# ---------------------------------------------------------------------------
# Prediction (ML)
# ---------------------------------------------------------------------------

class PredictionRequest(BaseModel):
    inputPath: str
    voxelRes: int = 320
    modelRes: int = 80
    modelsDir: str = ""
    flowDirections: list[FlowDirectionEnum] = [FlowDirectionEnum.x]


class DirectionPrediction(BaseModel):
    direction: str
    outputFile: str


class PredictionResponse(BaseModel):
    jobId: str
    outputDir: str
    directions: list[DirectionPrediction] = []


# ---------------------------------------------------------------------------
# Simulation
# ---------------------------------------------------------------------------

class SimulationRequest(BaseModel):
    caseDir: str
    solver: str = "simpleFoamMod"
    maxIter: int = 1000000
    writeInterval: int = 50000


class SimulationResponse(BaseModel):
    jobId: str
    status: JobStatusEnum
    logPath: Optional[str] = None


# ---------------------------------------------------------------------------
# Post-processing
# ---------------------------------------------------------------------------

class PostProcessRequest(BaseModel):
    caseDir: str
    method: PermeabilityMethod = PermeabilityMethod.both
    fibrousRegionOnly: bool = True


class PermeabilityData(BaseModel):
    direction: str
    permVolAvgMain: Optional[float] = None
    permVolAvgSecondary: Optional[float] = None
    permVolAvgTertiary: Optional[float] = None
    permFlowRate: Optional[float] = None
    fiberVolumeContent: Optional[float] = None
    flowLength: Optional[float] = None
    crossSectionArea: Optional[float] = None


class PostProcessResponse(BaseModel):
    jobId: str
    caseDir: str
    results: list[PermeabilityData] = []


# ---------------------------------------------------------------------------
# Job status
# ---------------------------------------------------------------------------

class JobStatus(BaseModel):
    jobId: str
    status: JobStatusEnum
    progress: Optional[float] = None
    returncode: Optional[int] = None
    log: list[str] = []
