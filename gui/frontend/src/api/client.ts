import axios, { AxiosResponse } from 'axios'

// ---------------------------------------------------------------------------
// Backend URL resolution
// ---------------------------------------------------------------------------
const STORAGE_KEY = 'fiberfoam_backend_url'

function resolveBaseURL(): string {
  // 1. Check localStorage override
  const stored = localStorage.getItem(STORAGE_KEY)
  if (stored) return stored

  // 2. If running inside Docker / same-origin (path-based API), use relative
  //    This is the case when frontend is served by the FastAPI backend
  if (window.location.port === '3000' || window.location.pathname.startsWith('/api')) {
    return '/api'
  }

  // 3. Hosted mode (e.g. Vercel) — default to user's local backend
  return 'http://localhost:3000/api'
}

export function getBackendUrl(): string {
  return api.defaults.baseURL ?? resolveBaseURL()
}

export function setBackendUrl(url: string) {
  const cleaned = url.replace(/\/+$/, '')
  localStorage.setItem(STORAGE_KEY, cleaned)
  api.defaults.baseURL = cleaned
}

export function resetBackendUrl() {
  localStorage.removeItem(STORAGE_KEY)
  api.defaults.baseURL = resolveBaseURL()
}

// ---------------------------------------------------------------------------
// Axios instance
// ---------------------------------------------------------------------------
const api = axios.create({
  baseURL: resolveBaseURL(),
  headers: { 'Content-Type': 'application/json' },
  timeout: 120_000, // 2 minutes default; long-running jobs use their own
})

// ---------------------------------------------------------------------------
// Shared Types
// ---------------------------------------------------------------------------

/** Matches backend GeometryUploadResponse */
export interface GeometryStats {
  filename: string
  resolution: number
  fluidFraction: number
  shape: number[]
}

export interface BufferZoneConfig {
  inlet: number
  outlet: number
}

/** Matches backend MeshRequest */
export interface MeshRequest {
  inputPath: string
  voxelSize: number
  voxelRes: number
  flowDirection: string
  inletBuffer: number
  outletBuffer: number
  connectivity: number
}

/** Matches backend PredictionRequest */
export interface PredictionRequest {
  inputPath: string
  voxelRes: number
  modelRes: number
  modelsDir: string
  flowDirections: string[]
}

/** Matches backend SimulationRequest */
export interface SimulationRequest {
  caseDir: string
  solver: string
  maxIter: number
  writeInterval: number
}

/** Matches backend PostProcessRequest */
export interface PostProcessRequest {
  caseDir: string
  method: string
  fibrousRegionOnly: boolean
}

/** Matches backend JobStatus */
export interface JobStatus {
  jobId: string
  status: 'queued' | 'running' | 'completed' | 'error'
  progress: number
  returncode: number | null
  log: string
}

/** Matches backend PipelineModeEnum */
export type PipelineMode = 'mesh_only' | 'predict_only' | 'mesh_predict' | 'full'

/** Matches backend PipelineRequest */
export interface PipelineRequest {
  mode: PipelineMode
  inputPath: string
  flowDirections: string[]
  voxelSize: number
  voxelRes: number
  modelRes: number
  modelFolder?: string
  inletBuffer: number
  outletBuffer: number
  connectivity: boolean
  solver: string
  maxIter: number
  writeInterval: number
  convWindow?: number
  convSlope?: number
  convErrorBound?: number
  outputDir?: string
  viscosity?: number
  deltaP?: number
}

/** Matches backend BatchRequest */
export interface BatchRequest {
  mode: PipelineMode
  flowDirections: string[]
  voxelSize: number
  voxelRes: number
  modelRes: number
  inletBuffer: number
  outletBuffer: number
  connectivity: boolean
  solver: string
  maxIter: number
  writeInterval: number
  convWindow?: number
  convSlope?: number
  convErrorBound?: number
  inputFiles: string[]
  outputDir?: string
  remapPoreValue?: number | null
  remapOtherMapping?: string
  autoAlign?: boolean
}

export interface PipelineStepStatus {
  name: string
  status: string
  jobId?: string
  progress: number
  log?: string[]
  residuals?: { iteration: number; [field: string]: number }[]
}

export interface PipelineStatus {
  pipelineId: string
  status: 'queued' | 'running' | 'completed' | 'error' | 'cancelled' | 'pending' | 'failed'
  steps: PipelineStepStatus[]
  currentStep: string | null
  progress: number
  error?: string
  results?: Record<string, PermeabilityResult>
}

export interface BatchStatus {
  batchId: string
  status: 'queued' | 'running' | 'completed' | 'error'
  totalFiles: number
  completedFiles: number
  pipelines: PipelineStatus[]
}

export interface HealthResponse {
  status: string
  version?: string
  solverAvailable?: boolean
}

// Legacy types kept for existing pages
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
  permVolAvgMain?: number
  permVolAvgSecondary?: number
  permVolAvgTertiary?: number
  permFlowRate?: number
  fiberVolumeContent?: number
  flowLength?: number
  crossSectionArea?: number
}

export interface QuickPredictionData {
  direction: string
  permeability: number
  fiberVolumeContent: number
  meanVelocity: number
  flowLength: number
}

export interface QuickPredictionResponse {
  results: QuickPredictionData[]
}

export interface PostProcessResult {
  permeability: PermeabilityResult[]
  convergencePlot: ConvergencePoint[]
}

export interface ConvergencePoint {
  iteration: number
  residual: number
}

// ---------------------------------------------------------------------------
// Preprocessing types
// ---------------------------------------------------------------------------

export interface AnalyzeResult {
  filename: string
  shape: number[]
  uniqueValues: number[]
  valueCounts: Record<string, number>
}

export interface OrientationResult {
  xyAngle: number
  xyRawAngle: number
  xyRotation: number
  xzAngle: number
  xzRawAngle: number
  xzRotation: number
}

export interface PreprocessResult {
  filename: string
  resolution: number
  fluidFraction: number
  shape: number[]
  uniqueValues: number[]
}

export interface AutoAlignResult {
  xyAngle: number
  xzAngle: number
  xyRotationApplied: number
  xzRotationApplied: number
  filename: string
  resolution: number
  fluidFraction: number
  shape: number[]
}

// ---------------------------------------------------------------------------
// Preprocessing endpoints
// ---------------------------------------------------------------------------

export async function analyzeGeometry(filename: string): Promise<AnalyzeResult> {
  const res = await api.get<AnalyzeResult>(`/preprocess/analyze/${encodeURIComponent(filename)}`)
  return res.data
}

export async function remapValues(params: {
  filename: string
  poreValue: number
  otherMapping?: 'solid' | 'pore'
}): Promise<PreprocessResult> {
  const res = await api.post<PreprocessResult>('/preprocess/remap', params)
  return res.data
}

export async function estimateOrientation(filename: string): Promise<OrientationResult> {
  const res = await api.get<OrientationResult>(`/preprocess/orientation/${encodeURIComponent(filename)}`)
  return res.data
}

export async function rotateGeometry(params: {
  filename: string
  axis: string
  angle: number
}): Promise<PreprocessResult> {
  const res = await api.post<PreprocessResult>('/preprocess/rotate', params, {
    timeout: 300_000,
  })
  return res.data
}

export async function autoAlignGeometry(filename: string): Promise<AutoAlignResult> {
  const res = await api.post<AutoAlignResult>('/preprocess/auto-align', { filename }, {
    timeout: 300_000,
  })
  return res.data
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

export async function listGeometries(): Promise<string[]> {
  const res = await api.get<string[]>('/geometry/list')
  return res.data
}

export async function getGeometryVoxels(filename: string): Promise<{
  positions: number[][]
  dimensions: [number, number, number]
}> {
  const res = await api.get(`/geometry/voxels/${encodeURIComponent(filename)}`)
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
export interface ModelSet {
  folder: string
  resolution: number
  directions: string[]
}

export interface ModelSetsResponse {
  models: string[]
  modelSets: ModelSet[]
  modelsDir: string
}

export async function listModels(): Promise<string[]> {
  const res = await api.get<ModelSetsResponse>('/prediction/models')
  return res.data.models
}

export async function listModelSets(): Promise<ModelSetsResponse> {
  const res = await api.get<ModelSetsResponse>('/prediction/models')
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

export async function runQuickPrediction(params: {
  inputPath: string
  flowDirections: string[]
  voxelSize: number
  voxelRes: number
  modelRes: number
  inletBuffer: number
  outletBuffer: number
  viscosity?: number
  deltaP?: number
}): Promise<QuickPredictionResponse> {
  const res = await api.post<QuickPredictionResponse>('/prediction/quick', params, {
    timeout: 300_000,
  })
  return res.data
}

export async function exportPredictionVtk(pipelineId: string, destPath: string): Promise<{ status: string; outputDir: string; files: string[] }> {
  const res = await api.post(`/prediction/export-vtk/${pipelineId}`, { destPath }, { timeout: 120_000 })
  return res.data
}

export async function getPredictionStatus(jobId: string): Promise<JobStatus> {
  const res = await api.get<JobStatus>(`/prediction/status/${jobId}`)
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

export async function getMeshStatus(jobId: string): Promise<JobStatus> {
  const res = await api.get<JobStatus>(`/mesh/status/${jobId}`)
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
  const res = await api.post<JobStatus>('/simulation/run', config, {
    timeout: 600_000,
  })
  return res.data
}

export async function stopSimulation(jobId: string): Promise<{ status: string }> {
  const res = await api.post(`/simulation/cancel/${jobId}`)
  return res.data
}

export async function getSimulationStatus(jobId: string): Promise<SimulationStatus> {
  const res = await api.get<SimulationStatus>(`/simulation/status/${jobId}`)
  return res.data
}

// ---------------------------------------------------------------------------
// Post-processing endpoints
// ---------------------------------------------------------------------------
export async function runPostProcessing(
  config: PostProcessRequest,
): Promise<PostProcessResult> {
  const res = await api.post<PostProcessResult>('/postprocess/run', config)
  return res.data
}

export async function getPostProcessStatus(jobId: string): Promise<JobStatus> {
  const res = await api.get<JobStatus>(`/postprocess/status/${jobId}`)
  return res.data
}

export async function getPermeabilityResults(jobId: string): Promise<
  PermeabilityResult[]
> {
  const res = await api.get<PermeabilityResult[]>(`/postprocess/results/${jobId}`)
  return res.data
}

export async function exportCsv(): Promise<Blob> {
  const res = await api.get('/postprocess/export-csv', {
    responseType: 'blob',
  })
  return res.data
}

// ---------------------------------------------------------------------------
// Pipeline endpoints
// ---------------------------------------------------------------------------
export async function runPipeline(config: PipelineRequest): Promise<{ pipelineId: string }> {
  const res = await api.post<{ pipelineId: string }>('/pipeline/run', config)
  return res.data
}

export async function getPipelineStatus(pipelineId: string): Promise<PipelineStatus> {
  const res = await api.get<PipelineStatus>(`/pipeline/status/${pipelineId}`)
  return res.data
}

export async function cancelPipeline(pipelineId: string): Promise<void> {
  await api.post(`/pipeline/cancel/${pipelineId}`)
}

export async function stopSolver(pipelineId: string): Promise<{ status: string }> {
  const res = await api.post<{ status: string }>(`/pipeline/stop-solver/${pipelineId}`)
  return res.data
}

// ---------------------------------------------------------------------------
// Batch endpoints
// ---------------------------------------------------------------------------
export async function runBatch(config: BatchRequest): Promise<{ batchId: string }> {
  const res = await api.post<{ batchId: string }>('/pipeline/batch', config)
  return res.data
}

export async function getBatchStatus(batchId: string): Promise<BatchStatus> {
  const res = await api.get<BatchStatus>(`/pipeline/batch/${batchId}`)
  return res.data
}

export interface InputFileEntry {
  filename: string
  source: 'input' | 'uploaded'
  path: string
}

export async function listInputFiles(): Promise<{ files: InputFileEntry[] }> {
  const res = await api.get<{ files: InputFileEntry[] }>('/pipeline/input-files')
  return res.data
}

// ---------------------------------------------------------------------------
// Results endpoints
// ---------------------------------------------------------------------------
export async function downloadResults(pipelineId: string): Promise<Blob> {
  const res = await api.get(`/results/download/${pipelineId}`, {
    responseType: 'blob',
  })
  return res.data
}

export async function getResultsCsv(pipelineId: string): Promise<Blob> {
  const res = await api.get(`/results/csv/${pipelineId}`, {
    responseType: 'blob',
  })
  return res.data
}

export async function getResultsFiles(pipelineId: string): Promise<{ files: string[] }> {
  const res = await api.get<{ files: string[] }>(`/results/files/${pipelineId}`)
  return res.data
}

// ---------------------------------------------------------------------------
// Filesystem browsing
// ---------------------------------------------------------------------------

export interface BrowseEntry {
  name: string
  type: 'directory' | 'file'
  size?: number
}

export interface BrowseResult {
  path: string
  absPath: string
  root: string
  dirs: BrowseEntry[]
  files: BrowseEntry[]
}

export async function browseFilesystem(path: string = ''): Promise<BrowseResult> {
  const res = await api.get<BrowseResult>('/filesystem/browse', { params: { path } })
  return res.data
}

export async function createDirectory(path: string): Promise<{ path: string; absPath: string }> {
  const res = await api.post('/filesystem/mkdir', { path })
  return res.data
}

export interface SaveResultsResponse {
  destPath: string
  absPath: string
  copiedDirs: string[]
  skippedDirs?: string[]
  alreadyExists?: boolean
  warnings?: string[]
}

export async function saveResultsToFolder(pipelineId: string, destPath: string, force = false): Promise<SaveResultsResponse> {
  const res = await api.post<SaveResultsResponse>('/filesystem/save-results', { pipelineId, destPath, force })
  return res.data
}

// ---------------------------------------------------------------------------
// ParaView integration
// ---------------------------------------------------------------------------
export interface ParaViewAvailability {
  inDocker: boolean
  foamToVTK: boolean
  paraview: boolean
}

export interface VtkExportResult {
  status: string
  returncode: number
  vtkDir: string
  vtkFiles: string[]
  log: string
}

export interface CaseDir {
  name: string
  path: string
  hasMesh: boolean
  hasResults: boolean
  timeSteps: string[]
}

export async function checkParaView(): Promise<ParaViewAvailability> {
  const res = await api.get<ParaViewAvailability>('/paraview/available')
  return res.data
}

export async function exportVtk(caseDir: string, latestTime = true): Promise<VtkExportResult> {
  const res = await api.post<VtkExportResult>('/paraview/export-vtk', { caseDir, latestTime }, { timeout: 300_000 })
  return res.data
}

export async function openParaView(caseDir: string, mode = 'case'): Promise<{ status: string; hint?: string; foamFile?: string; message?: string }> {
  const res = await api.post('/paraview/open', { caseDir, mode })
  return res.data
}

export async function listCaseDirs(pipelineId: string): Promise<{ pipelineId: string; outputDir: string; cases: CaseDir[] }> {
  const res = await api.get(`/paraview/case-dirs/${pipelineId}`)
  return res.data
}

// ---------------------------------------------------------------------------
// Health & Job status
// ---------------------------------------------------------------------------
export async function getHealth(): Promise<HealthResponse> {
  const res = await api.get<HealthResponse>('/health')
  return res.data
}

export async function getJobStatus(jobId: string): Promise<JobStatus> {
  const res = await api.get<JobStatus>(`/jobs/${jobId}`)
  return res.data
}

export interface SystemStats {
  totalGb: number
  usedGb: number
  availableGb: number
  percent: number
}

export async function getSystemStats(): Promise<SystemStats> {
  const res = await api.get<SystemStats>('/system/stats')
  return res.data
}

// ---------------------------------------------------------------------------
// Feedback
// ---------------------------------------------------------------------------
export async function submitFeedback(params: {
  category: string
  message: string
  contact?: string
}): Promise<{ status: string; id: string }> {
  const res = await api.post<{ status: string; id: string }>('/feedback', params)
  return res.data
}

export default api
