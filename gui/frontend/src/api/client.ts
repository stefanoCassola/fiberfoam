import axios, { AxiosResponse } from 'axios'

// ---------------------------------------------------------------------------
// Axios instance -- all requests are relative to /api
// ---------------------------------------------------------------------------
const api = axios.create({
  baseURL: '/api',
  headers: { 'Content-Type': 'application/json' },
  timeout: 120_000, // 2 minutes default; long-running jobs use their own
})

// ---------------------------------------------------------------------------
// Shared Types
// ---------------------------------------------------------------------------
export interface GeometryStats {
  filename: string
  dimensions: [number, number, number]
  voxelCount: number
  porosity: number
  voxelSize: number
  fileSize: number
}

export interface BufferZoneConfig {
  inlet: number
  outlet: number
}

export interface PredictionConfig {
  modelName: string
  direction: 'x' | 'y' | 'z'
}

export interface PredictionResult {
  modelName: string
  direction: string
  permeability: number
  velocityFieldPath: string
}

export interface MeshConfig {
  refinementLevel: number
  bufferZone: BufferZoneConfig
  snapControls: boolean
}

export interface MeshStats {
  cellCount: number
  pointCount: number
  faceCount: number
  maxNonOrthogonality: number
  maxSkewness: number
  quality: string
}

export interface SolverConfig {
  solver: string
  direction: 'x' | 'y' | 'z'
  nIterations: number
  tolerance: number
  relaxationFactor: number
  writeInterval: number
}

export interface SimulationStatus {
  status: 'idle' | 'running' | 'completed' | 'error'
  progress: number
  currentIteration: number
  totalIterations: number
  elapsedTime: number
  residuals: ResidualPoint[]
}

export interface ResidualPoint {
  iteration: number
  Ux: number
  Uy: number
  Uz: number
  p: number
}

export interface PermeabilityResult {
  direction: string
  darcyMethod: number
  pressureDropMethod: number
  unit: string
}

export interface PostProcessResult {
  permeability: PermeabilityResult[]
  convergencePlot: ConvergencePoint[]
}

export interface ConvergencePoint {
  iteration: number
  residual: number
}

export interface JobStatus {
  jobId: string
  status: 'queued' | 'running' | 'completed' | 'error'
  progress: number
  message: string
}

// ---------------------------------------------------------------------------
// Geometry endpoints
// ---------------------------------------------------------------------------
export async function uploadGeometry(file: File): Promise<GeometryStats> {
  const form = new FormData()
  form.append('file', file)
  const res: AxiosResponse<GeometryStats> = await api.post(
    '/geometry/upload',
    form,
    { headers: { 'Content-Type': 'multipart/form-data' }, timeout: 300_000 },
  )
  return res.data
}

export async function getGeometryStats(): Promise<GeometryStats> {
  const res = await api.get<GeometryStats>('/geometry/stats')
  return res.data
}

export async function getGeometryVoxels(): Promise<{
  voxels: number[][][]
  dimensions: [number, number, number]
}> {
  const res = await api.get('/geometry/voxels')
  return res.data
}

export async function setBufferZone(
  config: BufferZoneConfig,
): Promise<{ status: string }> {
  const res = await api.post('/geometry/buffer-zone', config)
  return res.data
}

// ---------------------------------------------------------------------------
// Prediction endpoints (ML model)
// ---------------------------------------------------------------------------
export async function listModels(): Promise<string[]> {
  const res = await api.get<string[]>('/prediction/models')
  return res.data
}

export async function runPrediction(
  config: PredictionConfig,
): Promise<PredictionResult> {
  const res = await api.post<PredictionResult>('/prediction/run', config, {
    timeout: 600_000,
  })
  return res.data
}

export async function getVelocitySlice(
  axis: string,
  index: number,
): Promise<{ data: number[][] }> {
  const res = await api.get('/prediction/velocity-slice', {
    params: { axis, index },
  })
  return res.data
}

// ---------------------------------------------------------------------------
// Mesh endpoints
// ---------------------------------------------------------------------------
export async function generateMesh(
  config: MeshConfig,
): Promise<JobStatus> {
  const res = await api.post<JobStatus>('/mesh/generate', config, {
    timeout: 600_000,
  })
  return res.data
}

export async function getMeshStats(): Promise<MeshStats> {
  const res = await api.get<MeshStats>('/mesh/stats')
  return res.data
}

export async function getMeshPreview(): Promise<{
  vertices: number[][]
  faces: number[][]
}> {
  const res = await api.get('/mesh/preview')
  return res.data
}

// ---------------------------------------------------------------------------
// Simulation endpoints
// ---------------------------------------------------------------------------
export async function startSimulation(
  config: SolverConfig,
): Promise<JobStatus> {
  const res = await api.post<JobStatus>('/simulation/start', config, {
    timeout: 600_000,
  })
  return res.data
}

export async function stopSimulation(): Promise<{ status: string }> {
  const res = await api.post('/simulation/stop')
  return res.data
}

export async function getSimulationStatus(): Promise<SimulationStatus> {
  const res = await api.get<SimulationStatus>('/simulation/status')
  return res.data
}

// ---------------------------------------------------------------------------
// Post-processing endpoints
// ---------------------------------------------------------------------------
export async function runPostProcessing(
  direction: string,
): Promise<PostProcessResult> {
  const res = await api.post<PostProcessResult>('/postprocess/run', {
    direction,
  })
  return res.data
}

export async function getPermeabilityResults(): Promise<
  PermeabilityResult[]
> {
  const res = await api.get<PermeabilityResult[]>('/postprocess/permeability')
  return res.data
}

export async function exportCsv(): Promise<Blob> {
  const res = await api.get('/postprocess/export-csv', {
    responseType: 'blob',
  })
  return res.data
}

// ---------------------------------------------------------------------------
// Job status polling helper
// ---------------------------------------------------------------------------
export async function getJobStatus(jobId: string): Promise<JobStatus> {
  const res = await api.get<JobStatus>(`/jobs/${jobId}`)
  return res.data
}

export default api
