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
    pending = "pending"
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
# Preprocessing
# ---------------------------------------------------------------------------

class OtherValueMapping(str, Enum):
    solid = "solid"
    pore = "pore"


class RemapRequest(BaseModel):
    filename: str
    poreValue: int = Field(0, description="Which raw value represents pore (fluid) space")
    otherMapping: OtherValueMapping = OtherValueMapping.solid


class AnalyzeResponse(BaseModel):
    filename: str
    shape: list[int]
    uniqueValues: list[int]
    valueCounts: dict[str, int]


class OrientationResponse(BaseModel):
    xyAngle: float       # normalised [0, 90] in XY plane
    xyRawAngle: float    # raw angle in XY plane
    xyRotation: float    # rotation around Z to align fibers with X
    xzAngle: float       # normalised [0, 90] in XZ plane
    xzRawAngle: float    # raw angle in XZ plane
    xzRotation: float    # rotation around Y to align fibers with X


class RotateRequest(BaseModel):
    filename: str
    axis: FlowDirectionEnum
    angle: float


class PreprocessResponse(BaseModel):
    filename: str
    resolution: int
    fluidFraction: float
    shape: list[int]
    uniqueValues: list[int] = []


class AutoAlignRequest(BaseModel):
    filename: str


class AutoAlignResponse(BaseModel):
    xyAngle: float            # estimated XY angle before alignment
    xzAngle: float            # estimated XZ angle before alignment
    xyRotationApplied: float  # rotation applied around Z
    xzRotationApplied: float  # rotation applied around Y
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


class QuickPredictionRequest(BaseModel):
    inputPath: str
    flowDirections: list[FlowDirectionEnum] = [FlowDirectionEnum.x]
    voxelSize: float = 0.5e-6
    voxelRes: int = 320
    modelRes: int = 80
    inletBuffer: int = 0
    outletBuffer: int = 0
    viscosity: float = 7.934782609e-05
    density: float = 920.0
    deltaP: float = 1.0


class QuickPredictionData(BaseModel):
    direction: str
    permeability: float
    fiberVolumeContent: float
    meanVelocity: float
    flowLength: float


class QuickPredictionResponse(BaseModel):
    results: list[QuickPredictionData] = []


# ---------------------------------------------------------------------------
# Simulation
# ---------------------------------------------------------------------------

class SimulationRequest(BaseModel):
    caseDir: str
    solver: str = "simpleFoamMod"
    maxIter: int = 10000
    writeInterval: int = 10000


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


# ---------------------------------------------------------------------------
# Pipeline
# ---------------------------------------------------------------------------

class PipelineModeEnum(str, Enum):
    mesh_only = "mesh_only"
    predict_only = "predict_only"
    mesh_predict = "mesh_predict"
    full = "full"


class PipelineRequest(BaseModel):
    mode: PipelineModeEnum
    inputPath: str
    flowDirections: list[FlowDirectionEnum] = [FlowDirectionEnum.x]
    voxelSize: float = 0.5e-6
    voxelRes: int = 320
    modelRes: int = 80
    inletBuffer: int = 0
    outletBuffer: int = 0
    connectivity: bool = True
    solver: str = "simpleFoamMod"
    maxIter: int = 10000
    writeInterval: int = 10000
    convWindow: int = 10
    convSlope: float = 0.01
    convErrorBound: float = 0.01
    outputDir: Optional[str] = None
    viscosity: float = 7.934782609e-05
    density: float = 920.0
    deltaP: float = 1.0


class PipelineStepStatus(BaseModel):
    name: str
    status: JobStatusEnum
    jobId: Optional[str] = None
    log: list[str] = []
    progress: Optional[float] = None
    residuals: list[dict] = []


class PipelineStatus(BaseModel):
    pipelineId: str
    mode: PipelineModeEnum
    steps: list[PipelineStepStatus] = []
    currentStep: Optional[str] = None
    status: JobStatusEnum
    results: dict = {}
    caseDirs: list[str] = []
    createdAt: str = ""


class BatchRequest(BaseModel):
    mode: PipelineModeEnum
    flowDirections: list[FlowDirectionEnum] = [FlowDirectionEnum.x]
    voxelSize: float = 0.5e-6
    voxelRes: int = 320
    modelRes: int = 80
    inletBuffer: int = 0
    outletBuffer: int = 0
    connectivity: bool = True
    solver: str = "simpleFoamMod"
    maxIter: int = 10000
    writeInterval: int = 10000
    convWindow: int = 10
    convSlope: float = 0.01
    convErrorBound: float = 0.01
    inputFiles: list[str] = []  # empty = all files in batch dir
    outputDir: Optional[str] = None
    viscosity: float = 7.934782609e-05
    density: float = 920.0
    deltaP: float = 1.0
    # Preprocessing options (applied identically to each file before pipeline)
    remapPoreValue: Optional[int] = None  # None = skip remap
    remapOtherMapping: str = "solid"  # "solid" or "pore"
    autoAlign: bool = False


class BatchStatus(BaseModel):
    batchId: str
    status: JobStatusEnum
    totalFiles: int = 0
    completedFiles: int = 0
    pipelines: list[PipelineStatus] = []
