import { useState, useCallback, useEffect, useRef } from 'react'
import PermeabilityTable from '../components/PermeabilityTable'
import ConvergenceChart from '../components/ConvergenceChart'
import NumberInput from '../components/NumberInput'
import FolderPicker from '../components/FolderPicker'
import RamMonitor from '../components/RamMonitor'
import Viewer3D, { type VoxelData } from '../components/Viewer3D'
import { useWorkflow } from '../context/WorkflowContext'
import {
  uploadGeometry,
  analyzeGeometry,
  remapValues,
  estimateOrientation,
  autoAlignGeometry,
  rotateGeometry,
  runPipeline,
  getPipelineStatus,
  cancelPipeline,
  stopSolver,
  listModelSets,
  downloadResults,
  getResultsCsv,
  saveResultsToFolder,
  getGeometryVoxels,
  type GeometryStats,
  type AnalyzeResult,
  type PreprocessResult,
  type OrientationResult,
  type AutoAlignResult,
  type PipelineMode,
  type PipelineStatus,
  type PermeabilityResult,
  type ModelSet,
} from '../api/client'

const STEPS = ['Upload', 'Preprocess', 'Mode', 'Configure', 'Review', 'Progress', 'Results']

function StepIndicator({ current, steps }: { current: number; steps: string[] }) {
  return (
    <div className="flex items-center gap-1">
      {steps.map((label, i) => (
        <div key={label} className="flex items-center">
          <div className="flex items-center gap-2">
            <div
              className={`flex items-center justify-center w-8 h-8 rounded-full text-xs font-bold transition-colors ${
                i < current
                  ? 'bg-primary-600 text-white'
                  : i === current
                    ? 'bg-primary-600/30 text-primary-400 ring-2 ring-primary-500'
                    : 'bg-gray-800 text-gray-600'
              }`}
            >
              {i < current ? (
                <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={3}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M4.5 12.75l6 6 9-13.5" />
                </svg>
              ) : (
                i + 1
              )}
            </div>
            <span
              className={`text-xs font-medium hidden sm:inline ${
                i <= current ? 'text-gray-300' : 'text-gray-600'
              }`}
            >
              {label}
            </span>
          </div>
          {i < steps.length - 1 && (
            <div
              className={`w-8 h-0.5 mx-2 rounded ${
                i < current ? 'bg-primary-600' : 'bg-gray-800'
              }`}
            />
          )}
        </div>
      ))}
    </div>
  )
}

export default function PipelinePage() {
  const workflow = useWorkflow()

  const [step, setStep] = useState(0)
  const [stats, setStats] = useState<GeometryStats | null>(null)
  const [uploading, setUploading] = useState(false)
  const [error, setError] = useState<string | null>(null)

  // Preprocessing
  const [analysis, setAnalysis] = useState<AnalyzeResult | null>(null)
  const [analyzing, setAnalyzing] = useState(false)
  const [poreValue, setPoreValue] = useState(0)
  const [otherMapping, setOtherMapping] = useState<'solid' | 'pore'>('solid')
  const [remapResult, setRemapResult] = useState<PreprocessResult | null>(null)
  const [remapping, setRemapping] = useState(false)
  const [orientation, setOrientation] = useState<OrientationResult | null>(null)
  const [estimatingOrientation, setEstimatingOrientation] = useState(false)
  const [alignResult, setAlignResult] = useState<AutoAlignResult | null>(null)
  const [aligning, setAligning] = useState(false)
  const [manualRotateResult, setManualRotateResult] = useState<PreprocessResult | null>(null)
  const [manualRotating, setManualRotating] = useState(false)
  const [rotateAxis, setRotateAxis] = useState<'x' | 'y' | 'z'>('z')
  const [rotateAngle, setRotateAngle] = useState(0)

  // Mode selection
  const [mode, setMode] = useState<PipelineMode>('full')

  // Configuration
  const [flowDirs, setFlowDirs] = useState<Record<string, boolean>>({
    x: true,
    y: false,
    z: false,
  })
  const [voxelSize, setVoxelSize] = useState(1e-6)
  const [voxelRes, setVoxelRes] = useState(80)
  const [modelRes, setModelRes] = useState(80)
  const [inletBuffer, setInletBuffer] = useState(5)
  const [outletBuffer, setOutletBuffer] = useState(5)
  const [connectivity, setConnectivity] = useState(true)
  // Solver is always simpleFoamMod with permeability-based convergence.
  // Run for 10000 iterations max; only write the final timestep.
  const solver = 'simpleFoamMod'
  const maxIter = 10000
  const writeInterval = 10000
  const convWindow = 10
  const convSlope = 0.01
  const convErrorBound = 0.01
  const [outputDir, setOutputDir] = useState('')
  const [modelSets, setModelSets] = useState<ModelSet[]>([])
  const [selectedModelFolder, setSelectedModelFolder] = useState('')

  // Fetch available model sets on mount
  useEffect(() => {
    listModelSets().then((data) => {
      setModelSets(data.modelSets)
      if (data.modelSets.length > 0 && !selectedModelFolder) {
        setSelectedModelFolder(data.modelSets[0].folder)
        setModelRes(data.modelSets[0].resolution)
      }
    }).catch(() => {})
  }, []) // eslint-disable-line react-hooks/exhaustive-deps

  // Progress
  const [pipelineId, setPipelineId] = useState<string | null>(null)
  const [pipelineStatus, setPipelineStatus] = useState<PipelineStatus | null>(null)
  const [results, setResults] = useState<PermeabilityResult[]>([])
  const pollingRef = useRef<ReturnType<typeof setInterval> | null>(null)

  // 3D viewer
  const [showViewer, setShowViewer] = useState(false)
  const [voxelData, setVoxelData] = useState<VoxelData | null>(null)
  const [loadingVoxels, setLoadingVoxels] = useState(false)
  // Bump this counter to force a re-fetch even when the filename stays the same
  const [voxelGeneration, setVoxelGeneration] = useState(0)

  // The file that will be passed to the pipeline (latest preprocessed output, or original)
  const activeFile = manualRotateResult?.filename ?? alignResult?.filename ?? remapResult?.filename ?? stats?.filename ?? ''

  // Whether the raw geometry has values other than 0 and 1
  const hasNonBinary = analysis ? analysis.uniqueValues.some((v) => v !== 0 && v !== 1) : false
  // Whether remapping is required before continuing
  const needsRemap = hasNonBinary && !remapResult && !alignResult

  // Auto-resume: if workflow has an active pipelineId, fetch its status on mount
  useEffect(() => {
    if (pipelineId || step !== 0) return // already have one, or user navigated forward
    const activeId = workflow.pipelineId
    if (!activeId) return

    let cancelled = false
    ;(async () => {
      try {
        const status = await getPipelineStatus(activeId)
        if (cancelled) return
        setPipelineId(activeId)
        setPipelineStatus(status)
        if (status.status === 'completed') {
          if (status.results) {
            setResults(Object.values(status.results))
          }
          setStep(5) // Show progress view with "View Results" button
        } else if (status.status === 'error' || status.status === 'failed') {
          setError(status.error ?? 'Pipeline failed')
          setStep(5)
        } else {
          // Still running — jump to progress view
          setStep(5)
        }
      } catch {
        // Pipeline no longer exists on backend — clear stale id
        workflow.setPipelineId(null)
      }
    })()
    return () => { cancelled = true }
  }, []) // eslint-disable-line react-hooks/exhaustive-deps

  // Step 0: Upload
  const handleUpload = useCallback(
    async (e: React.ChangeEvent<HTMLInputElement>) => {
      const file = e.target.files?.[0]
      if (!file) return
      setError(null)
      setUploading(true)
      try {
        const result = await uploadGeometry(file)
        setStats(result)
        workflow.setGeometryStats(result)
        workflow.setGeometryPath(result.filename)

        // Auto-analyze for preprocessing
        setAnalyzing(true)
        try {
          const analysisResult = await analyzeGeometry(result.filename)
          setAnalysis(analysisResult)
          // Default pore value to 0 (common convention)
          setPoreValue(0)
        } catch {
          // Analysis failed — non-blocking, user can still proceed
        } finally {
          setAnalyzing(false)
        }

        setStep(1) // Go to Preprocess step
      } catch (err) {
        setError(err instanceof Error ? err.message : 'Upload failed')
      } finally {
        setUploading(false)
      }
    },
    [workflow],
  )

  // Preprocessing: remap
  const handleRemap = useCallback(async () => {
    if (!activeFile) return
    setRemapping(true)
    setError(null)
    try {
      const result = await remapValues({
        filename: activeFile,
        poreValue,
        ...(hasNonBinary ? { otherMapping } : {}),
      })
      setRemapResult(result)
      setVoxelGeneration((g) => g + 1)
      setAlignResult(null)
      setOrientation(null)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to remap values')
    } finally {
      setRemapping(false)
    }
  }, [activeFile, poreValue, otherMapping, hasNonBinary])

  // Preprocessing: estimate orientation
  const handleEstimateOrientation = useCallback(async () => {
    if (!activeFile) return
    setEstimatingOrientation(true)
    setError(null)
    try {
      const result = await estimateOrientation(activeFile)
      setOrientation(result)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to estimate orientation')
    } finally {
      setEstimatingOrientation(false)
    }
  }, [activeFile])

  // Preprocessing: auto-align
  const handleAutoAlign = useCallback(async () => {
    if (!activeFile) return
    setAligning(true)
    setError(null)
    try {
      const result = await autoAlignGeometry(activeFile)
      setAlignResult(result)
      setVoxelGeneration((g) => g + 1)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to auto-align geometry')
    } finally {
      setAligning(false)
    }
  }, [activeFile])

  // Preprocessing: manual rotate
  const handleManualRotate = useCallback(async () => {
    if (!activeFile || rotateAngle === 0) return
    setManualRotating(true)
    setError(null)
    try {
      const result = await rotateGeometry({ filename: activeFile, axis: rotateAxis, angle: rotateAngle })
      setManualRotateResult(result)
      setVoxelGeneration((g) => g + 1)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to rotate geometry')
    } finally {
      setManualRotating(false)
    }
  }, [activeFile, rotateAxis, rotateAngle])

  // 3D viewer: fetch voxels when toggled on
  const handleToggleViewer = useCallback(() => {
    if (showViewer) {
      setShowViewer(false)
    } else if (activeFile) {
      setShowViewer(true)
    }
  }, [showViewer, activeFile])

  // Fetch voxels whenever the viewer is open and activeFile or generation changes
  useEffect(() => {
    if (!showViewer || !activeFile) return
    let cancelled = false
    setLoadingVoxels(true)
    getGeometryVoxels(activeFile).then((data) => {
      if (cancelled) return
      setVoxelData({ positions: data.positions, dimensions: data.dimensions as [number, number, number] })
    }).catch(() => {
      if (!cancelled) setVoxelData(null)
    }).finally(() => {
      if (!cancelled) setLoadingVoxels(false)
    })
    return () => { cancelled = true }
  }, [showViewer, activeFile, voxelGeneration])

  // Step 4: Launch
  const handleLaunch = useCallback(async () => {
    setError(null)
    const selectedDirs = Object.entries(flowDirs)
      .filter(([, v]) => v)
      .map(([k]) => k)

    if (selectedDirs.length === 0) {
      setError('Select at least one flow direction')
      return
    }

    try {
      const response = await runPipeline({
        mode,
        inputPath: activeFile,
        flowDirections: selectedDirs,
        voxelSize,
        voxelRes,
        modelRes,
        modelFolder: selectedModelFolder,
        inletBuffer,
        outletBuffer,
        connectivity,
        solver,
        maxIter,
        writeInterval,
        convWindow,
        convSlope,
        convErrorBound,
        ...(outputDir ? { outputDir } : {}),
      })
      setPipelineId(response.pipelineId)
      workflow.setPipelineId(response.pipelineId)

      // Store in local history
      const stored = localStorage.getItem('fiberfoam_pipelines')
      const ids: string[] = stored ? JSON.parse(stored) : []
      ids.unshift(response.pipelineId)
      localStorage.setItem('fiberfoam_pipelines', JSON.stringify(ids))

      setStep(5)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to start pipeline')
    }
  }, [mode, flowDirs, voxelSize, voxelRes, modelRes, inletBuffer, outletBuffer, connectivity, outputDir, workflow, activeFile])

  // Poll pipeline status
  useEffect(() => {
    if (step !== 5 || !pipelineId) return

    const poll = async () => {
      try {
        const status = await getPipelineStatus(pipelineId)
        setPipelineStatus(status)
        workflow.setCurrentStep(status.currentStep)

        if (status.status === 'completed') {
          if (status.results) {
            setResults(Object.values(status.results))
          }
          // Stay on progress step — user clicks "View Results" to continue
          if (pollingRef.current) clearInterval(pollingRef.current)
        } else if (status.status === 'error' || status.status === 'failed') {
          setError(status.error ?? 'Pipeline failed')
          if (pollingRef.current) clearInterval(pollingRef.current)
        }
      } catch {
        // Ignore transient polling errors
      }
    }

    poll()
    pollingRef.current = setInterval(poll, 2000)
    return () => {
      if (pollingRef.current) clearInterval(pollingRef.current)
    }
  }, [step, pipelineId, workflow])

  const handleCancel = useCallback(async () => {
    if (!pipelineId) return
    try {
      await cancelPipeline(pipelineId)
    } catch {
      // Ignore
    }
    if (pollingRef.current) clearInterval(pollingRef.current)
    setStep(4)
  }, [pipelineId])

  const [stopping, setStopping] = useState(false)

  const handleStopSolver = useCallback(async () => {
    if (!pipelineId) return
    setStopping(true)
    try {
      await stopSolver(pipelineId)
    } catch {
      // Ignore — solver may have already finished
    }
  }, [pipelineId])

  const handleDownload = useCallback(async () => {
    if (!pipelineId) return
    try {
      const blob = await downloadResults(pipelineId)
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `results_${pipelineId}.tar.gz`
      document.body.appendChild(a)
      a.click()
      document.body.removeChild(a)
      URL.revokeObjectURL(url)
    } catch {
      setError('Download failed')
    }
  }, [pipelineId])

  const handleExportCsv = useCallback(async () => {
    if (!pipelineId) return
    try {
      const blob = await getResultsCsv(pipelineId)
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = `results_${pipelineId}.csv`
      document.body.appendChild(a)
      a.click()
      document.body.removeChild(a)
      URL.revokeObjectURL(url)
    } catch {
      setError('CSV export failed')
    }
  }, [pipelineId])

  const [saving, setSaving] = useState(false)
  const [saveSuccess, setSaveSuccess] = useState<string | null>(null)
  const [confirmOverwrite, setConfirmOverwrite] = useState<string[] | null>(null)

  const handleSaveToFolder = useCallback(async (force = false) => {
    if (!pipelineId || !outputDir) return
    setSaving(true)
    setSaveSuccess(null)
    setConfirmOverwrite(null)
    setError(null)
    try {
      const res = await saveResultsToFolder(pipelineId, outputDir, force)
      if (res.alreadyExists && !force) {
        setConfirmOverwrite(res.skippedDirs ?? [])
        setSaving(false)
        return
      }
      const count = res.copiedDirs.length + (res.skippedDirs?.length ?? 0)
      setSaveSuccess(`Saved ${count} case(s) to ${res.absPath}`)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to save results')
    } finally {
      setSaving(false)
    }
  }, [pipelineId, outputDir])

  const handleReset = useCallback(() => {
    setStep(0)
    setStats(null)
    setAnalysis(null)
    setPoreValue(0)
    setOtherMapping('solid')
    setRemapResult(null)
    setOrientation(null)
    setAlignResult(null)
    setManualRotateResult(null)
    setRotateAxis('z')
    setRotateAngle(0)
    setMode('full')
    setFlowDirs({ x: true, y: false, z: false })
    setConnectivity(true)
    setOutputDir('')
    setPipelineId(null)
    setPipelineStatus(null)
    setResults([])
    setSaveSuccess(null)
    setError(null)
    workflow.setGeometryPath(null)
    workflow.setGeometryStats(null)
    workflow.setPipelineId(null)
    workflow.setCurrentStep(null)
  }, [workflow])

  const selectedDirs = Object.entries(flowDirs)
    .filter(([, v]) => v)
    .map(([k]) => k)

  const modeLabels: Record<PipelineMode, { title: string; desc: string }> = {
    mesh_only: {
      title: 'Mesh Only',
      desc: 'Generate OpenFOAM mesh from voxelized geometry',
    },
    predict_only: {
      title: 'Prediction Only',
      desc: 'ML-based permeability prediction (near real-time)',
    },
    mesh_predict: {
      title: 'Mesh + Prediction',
      desc: 'OpenFOAM case with ML-predicted velocity as initial condition',
    },
    full: {
      title: 'Full Pipeline',
      desc: 'ML prediction + CFD simulation + permeability calculation',
    },
  }

  return (
    <div className="space-y-6">
      <div>
        <h2 className="text-2xl font-bold text-white">Pipeline</h2>
        <p className="text-sm text-gray-400 mt-1">
          Run an end-to-end permeability computation pipeline.
        </p>
      </div>

      {/* Step indicator */}
      <div className="card">
        <StepIndicator current={step} steps={STEPS} />
      </div>

      {error && (
        <div className="p-4 rounded-lg bg-red-900/30 border border-red-800 text-red-300 text-sm">
          {error}
        </div>
      )}

      {/* Step 0: Upload */}
      {step === 0 && (
        <div className="card">
          <h3 className="card-header">Upload Geometry</h3>
          <label className="flex flex-col items-center justify-center w-full h-44 border-2 border-dashed border-gray-600 rounded-lg cursor-pointer hover:border-primary-500 hover:bg-gray-800/50 transition-colors">
            <svg className="w-10 h-10 text-gray-500 mb-2" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M3 16.5v2.25A2.25 2.25 0 005.25 21h13.5A2.25 2.25 0 0021 18.75V16.5m-13.5-9L12 3m0 0l4.5 4.5M12 3v13.5" />
            </svg>
            <span className="text-sm text-gray-400">
              {uploading ? 'Uploading...' : 'Click to upload .raw / .npy / .stl'}
            </span>
            <input
              type="file"
              className="hidden"
              accept=".raw,.npy,.stl,.dat"
              onChange={handleUpload}
              disabled={uploading}
            />
          </label>
          {uploading && (
            <div className="mt-4 flex items-center gap-2 text-sm text-gray-400">
              <svg className="animate-spin h-4 w-4 text-primary-500" fill="none" viewBox="0 0 24 24">
                <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
                <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z" />
              </svg>
              Processing geometry file...
            </div>
          )}
        </div>
      )}

      {/* Step 1: Preprocess */}
      {step === 1 && (
        <div className="space-y-6">
          {/* Geometry info */}
          {stats && (
            <div className="card">
              <h3 className="card-header">Geometry Info</h3>
              <div className="grid grid-cols-3 gap-4">
                <div>
                  <p className="stat-label">Filename</p>
                  <p className="text-sm text-gray-200 font-mono truncate">{stats.filename}</p>
                </div>
                <div>
                  <p className="stat-label">Resolution</p>
                  <p className="stat-value text-lg">{stats.resolution}</p>
                </div>
                <div>
                  <p className="stat-label">Shape</p>
                  <p className="stat-value text-lg">{stats.shape.join(' × ')}</p>
                </div>
              </div>
            </div>
          )}

          <div className="grid grid-cols-1 xl:grid-cols-2 gap-6">
            {/* Value analysis & mapping */}
            <div className="card">
              <h3 className="card-header">Value Analysis & Mapping</h3>

              {analyzing && (
                <div className="flex items-center gap-2 text-sm text-gray-400">
                  <svg className="animate-spin h-4 w-4 text-primary-500" fill="none" viewBox="0 0 24 24">
                    <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
                    <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
                  </svg>
                  Analyzing voxel values...
                </div>
              )}

              {analysis && !analyzing && (
                <div className="space-y-4">
                  {/* Value distribution table */}
                  <div>
                    <p className="text-sm text-gray-400 mb-2">
                      {analysis.uniqueValues.length} unique value{analysis.uniqueValues.length !== 1 ? 's' : ''} found
                      {hasNonBinary && (
                        <span className="text-yellow-400 ml-1">
                          — contains values other than 0 and 1
                        </span>
                      )}
                    </p>
                    <div className="bg-gray-900 rounded-lg border border-gray-700 overflow-hidden">
                      <table className="w-full text-sm">
                        <thead>
                          <tr className="border-b border-gray-700">
                            <th className="text-left px-3 py-2 text-gray-500 font-medium">Value</th>
                            <th className="text-right px-3 py-2 text-gray-500 font-medium">Voxels</th>
                            <th className="text-right px-3 py-2 text-gray-500 font-medium">Fraction</th>
                          </tr>
                        </thead>
                        <tbody>
                          {analysis.uniqueValues.map((v) => {
                            const count = analysis.valueCounts[String(v)] ?? 0
                            const total = Object.values(analysis.valueCounts).reduce((a, b) => a + b, 0)
                            return (
                              <tr key={v} className="border-b border-gray-800 last:border-0">
                                <td className="px-3 py-2 text-gray-200 font-mono">{v}</td>
                                <td className="px-3 py-2 text-gray-200 text-right font-mono">{count.toLocaleString()}</td>
                                <td className="px-3 py-2 text-gray-400 text-right">{((count / total) * 100).toFixed(2)}%</td>
                              </tr>
                            )
                          })}
                        </tbody>
                      </table>
                    </div>
                  </div>

                  {/* Pore value selection */}
                  <div>
                    <label className="text-sm text-gray-300 font-medium block mb-2">
                      Which value represents pore (fluid) space?
                    </label>
                    <select
                      className="select-field"
                      value={poreValue}
                      onChange={(e) => setPoreValue(Number(e.target.value))}
                    >
                      {analysis.uniqueValues.map((v) => (
                        <option key={v} value={v}>
                          {v}
                        </option>
                      ))}
                    </select>

                    {hasNonBinary && (
                      <div className="mt-3">
                        <label className="text-sm text-gray-300 font-medium block mb-2">
                          Other values (not 0 or 1) should be treated as:
                        </label>
                        <div className="flex gap-4">
                          <label className="flex items-center gap-2 text-sm text-gray-300 cursor-pointer">
                            <input
                              type="radio"
                              name="otherMapping"
                              value="solid"
                              checked={otherMapping === 'solid'}
                              onChange={() => setOtherMapping('solid')}
                              className="accent-primary-500"
                            />
                            Solid
                          </label>
                          <label className="flex items-center gap-2 text-sm text-gray-300 cursor-pointer">
                            <input
                              type="radio"
                              name="otherMapping"
                              value="pore"
                              checked={otherMapping === 'pore'}
                              onChange={() => setOtherMapping('pore')}
                              className="accent-primary-500"
                            />
                            Pore (fluid)
                          </label>
                        </div>
                      </div>
                    )}

                    <p className="text-xs text-gray-500 mt-1">
                      The meshing will use the pore space.
                    </p>
                  </div>

                  <button
                    className="btn btn-primary w-full"
                    onClick={handleRemap}
                    disabled={remapping}
                  >
                    {remapping ? 'Applying...' : 'Apply Mapping'}
                  </button>

                  {remapResult && (
                    <div className="p-3 rounded-lg bg-green-900/20 border border-green-800 space-y-1">
                      <p className="text-sm text-green-300">
                        Mapping applied. Fluid fraction: <span className="font-mono font-bold">{(remapResult.fluidFraction * 100).toFixed(2)}%</span>
                      </p>
                      <p className="text-xs text-gray-500 font-mono truncate">{remapResult.filename}</p>
                    </div>
                  )}

                  {!hasNonBinary && !remapResult && (
                    <p className="text-xs text-gray-500">
                      Geometry is already binary (0 and 1). You can apply mapping to change the pore/solid convention, or skip to continue.
                    </p>
                  )}
                </div>
              )}
            </div>

            {/* Fiber orientation (optional) */}
            <div className="card">
              <h3 className="card-header">Fiber Alignment (Optional)</h3>
              <div className="space-y-4">
                <p className="text-xs text-gray-500">
                  For directed fibers, the geometry should be aligned with the x-axis for correct permeability calculation.
                </p>

                {/* Auto-align section */}
                <div className="space-y-3">
                  <p className="text-sm text-gray-300 font-medium">Auto-Align</p>

                  <button
                    className="btn btn-secondary w-full"
                    onClick={handleEstimateOrientation}
                    disabled={estimatingOrientation || !activeFile}
                  >
                    {estimatingOrientation ? 'Estimating...' : 'Estimate Orientation'}
                  </button>

                  {orientation && (
                    <div className="p-3 rounded-lg bg-gray-800/50 border border-gray-700 space-y-3">
                      <div>
                        <p className="text-xs text-gray-500 mb-1 font-medium uppercase tracking-wider">XY Plane (around Z)</p>
                        <div className="flex justify-between text-sm">
                          <span className="text-gray-400">Angle:</span>
                          <span className="text-gray-200 font-mono">{orientation.xyAngle}°</span>
                        </div>
                        <div className="flex justify-between text-sm">
                          <span className="text-gray-400">Correction:</span>
                          <span className={`font-mono ${Math.abs(orientation.xyRotation) < 0.5 ? 'text-green-400' : 'text-yellow-400'}`}>
                            {orientation.xyRotation}°
                          </span>
                        </div>
                      </div>
                      <div className="border-t border-gray-700 pt-2">
                        <p className="text-xs text-gray-500 mb-1 font-medium uppercase tracking-wider">XZ Plane (around Y)</p>
                        <div className="flex justify-between text-sm">
                          <span className="text-gray-400">Angle:</span>
                          <span className="text-gray-200 font-mono">{orientation.xzAngle}°</span>
                        </div>
                        <div className="flex justify-between text-sm">
                          <span className="text-gray-400">Correction:</span>
                          <span className={`font-mono ${Math.abs(orientation.xzRotation) < 0.5 ? 'text-green-400' : 'text-yellow-400'}`}>
                            {orientation.xzRotation}°
                          </span>
                        </div>
                      </div>
                    </div>
                  )}

                  <button
                    className="btn btn-primary w-full"
                    onClick={handleAutoAlign}
                    disabled={aligning || !activeFile || !orientation}
                    title={!orientation ? 'Estimate orientation first' : undefined}
                  >
                    {aligning ? 'Aligning...' : 'Auto-Align to X-Axis'}
                  </button>

                  {alignResult && (
                    <div className="p-3 rounded-lg bg-green-900/20 border border-green-800 space-y-1">
                      <p className="text-sm text-green-300">
                        Rotations applied:
                        {alignResult.xzRotationApplied !== 0 && (
                          <span className="font-mono font-bold ml-1">XZ {alignResult.xzRotationApplied}°</span>
                        )}
                        {alignResult.xyRotationApplied !== 0 && (
                          <span className="font-mono font-bold ml-1">XY {alignResult.xyRotationApplied}°</span>
                        )}
                        {alignResult.xzRotationApplied === 0 && alignResult.xyRotationApplied === 0 && (
                          <span className="font-mono font-bold ml-1">none needed</span>
                        )}
                      </p>
                      <p className="text-sm text-gray-300">
                        Shape: <span className="font-mono">{alignResult.shape.join(' × ')}</span>,
                        fluid: <span className="font-mono">{(alignResult.fluidFraction * 100).toFixed(2)}%</span>
                      </p>
                      <p className="text-xs text-gray-500 font-mono truncate">{alignResult.filename}</p>
                    </div>
                  )}
                </div>

                {/* Manual rotation section */}
                <div className="border-t border-gray-700 pt-4 space-y-3">
                  <p className="text-sm text-gray-300 font-medium">Manual Rotation</p>
                  <div className="flex gap-3">
                    <div className="flex-1">
                      <label className="text-xs text-gray-500 block mb-1">Axis</label>
                      <select
                        className="select-field"
                        value={rotateAxis}
                        onChange={(e) => setRotateAxis(e.target.value as 'x' | 'y' | 'z')}
                      >
                        <option value="x">X</option>
                        <option value="y">Y</option>
                        <option value="z">Z</option>
                      </select>
                    </div>
                    <div className="flex-1">
                      <label className="text-xs text-gray-500 block mb-1">Angle (°)</label>
                      <NumberInput value={rotateAngle} onChange={setRotateAngle} min={-180} max={180} step={0.5} />
                    </div>
                  </div>
                  <button
                    className="btn btn-secondary w-full"
                    onClick={handleManualRotate}
                    disabled={manualRotating || !activeFile || rotateAngle === 0}
                  >
                    {manualRotating ? 'Rotating...' : 'Rotate'}
                  </button>

                  {manualRotateResult && (
                    <div className="p-3 rounded-lg bg-green-900/20 border border-green-800 space-y-1">
                      <p className="text-sm text-green-300">
                        Rotated <span className="font-mono font-bold">{rotateAngle}°</span> around {rotateAxis.toUpperCase()}-axis
                      </p>
                      <p className="text-sm text-gray-300">
                        Shape: <span className="font-mono">{manualRotateResult.shape.join(' × ')}</span>,
                        fluid: <span className="font-mono">{(manualRotateResult.fluidFraction * 100).toFixed(2)}%</span>
                      </p>
                      <p className="text-xs text-gray-500 font-mono truncate">{manualRotateResult.filename}</p>
                    </div>
                  )}
                </div>
              </div>
            </div>
          </div>

          {/* Warning if non-binary and not remapped */}
          {needsRemap && (
            <div className="p-4 rounded-lg bg-yellow-900/20 border border-yellow-800 text-yellow-300 text-sm">
              Geometry contains values other than 0 and 1. Apply a value mapping before continuing to ensure the correct phase is meshed.
            </div>
          )}

          {/* Active file summary */}
          {activeFile && activeFile !== stats?.filename && (
            <div className="text-xs text-gray-500">
              Pipeline will use: <span className="font-mono text-gray-300">{activeFile}</span>
            </div>
          )}

          {/* 3D Geometry Viewer */}
          {activeFile && (
            <div className="space-y-3">
              <button
                onClick={handleToggleViewer}
                className={`flex items-center gap-2 px-4 py-2 rounded-lg text-sm font-medium transition-colors ${
                  showViewer
                    ? 'bg-primary-600/20 text-primary-400'
                    : 'bg-gray-800 text-gray-400 hover:text-gray-200 hover:bg-gray-700'
                }`}
              >
                <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M21 7.5l-9-5.25L3 7.5m18 0l-9 5.25m9-5.25v9l-9 5.25M3 7.5l9 5.25M3 7.5v9l9 5.25m0-9v9" />
                </svg>
                {showViewer ? 'Hide 3D View' : 'View Geometry'}
              </button>
              {showViewer && (
                <div className="h-[450px]">
                  {loadingVoxels ? (
                    <div className="h-full flex items-center justify-center rounded-xl border border-gray-700 bg-gray-950">
                      <div className="flex items-center gap-3 text-gray-400">
                        <svg className="animate-spin h-5 w-5" fill="none" viewBox="0 0 24 24">
                          <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
                          <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
                        </svg>
                        Loading geometry...
                      </div>
                    </div>
                  ) : (
                    <Viewer3D voxelData={voxelData} />
                  )}
                </div>
              )}
            </div>
          )}

          <div className="flex gap-3">
            <button onClick={() => setStep(0)} className="btn-secondary">
              Back
            </button>
            <button
              onClick={() => setStep(2)}
              className="btn-primary"
              disabled={needsRemap}
            >
              Continue to Mode Selection
            </button>
          </div>
        </div>
      )}

      {/* Step 2: Mode */}
      {step === 2 && (
        <div className="space-y-6">
          <div className="card">
            <h3 className="card-header">Select Pipeline Mode</h3>
            <div className="grid grid-cols-1 md:grid-cols-2 xl:grid-cols-4 gap-4">
              {(Object.keys(modeLabels) as PipelineMode[]).map((m) => (
                <button
                  key={m}
                  onClick={() => {
                    setMode(m)
                    setStep(3)
                  }}
                  className={`p-5 rounded-xl border-2 text-left transition-all ${
                    mode === m
                      ? 'border-primary-500 bg-primary-600/10'
                      : 'border-gray-700 hover:border-gray-600 hover:bg-gray-800/50'
                  }`}
                >
                  <div className="flex items-center gap-3 mb-2">
                    <div className={`w-10 h-10 rounded-lg flex items-center justify-center ${
                      m === 'mesh_only' ? 'bg-blue-600/20 text-blue-400' :
                      m === 'predict_only' ? 'bg-amber-600/20 text-amber-400' :
                      m === 'mesh_predict' ? 'bg-purple-600/20 text-purple-400' :
                      'bg-green-600/20 text-green-400'
                    }`}>
                      {m === 'mesh_only' && (
                        <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                          <path strokeLinecap="round" strokeLinejoin="round" d="M3.75 6A2.25 2.25 0 016 3.75h2.25A2.25 2.25 0 0110.5 6v2.25a2.25 2.25 0 01-2.25 2.25H6a2.25 2.25 0 01-2.25-2.25V6z" />
                        </svg>
                      )}
                      {m === 'predict_only' && (
                        <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                          <path strokeLinecap="round" strokeLinejoin="round" d="M3.75 3v11.25A2.25 2.25 0 006 16.5h2.25M3.75 3h-1.5m1.5 0h16.5m0 0h1.5m-1.5 0v11.25A2.25 2.25 0 0118 16.5h-2.25m-7.5 0h7.5m-7.5 0l-1 3m8.5-3l1 3m0 0l.5 1.5m-.5-1.5h-9.5m0 0l-.5 1.5" />
                        </svg>
                      )}
                      {m === 'mesh_predict' && (
                        <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                          <path strokeLinecap="round" strokeLinejoin="round" d="M9.75 3.104v5.714a2.25 2.25 0 01-.659 1.591L5 14.5" />
                        </svg>
                      )}
                      {m === 'full' && (
                        <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                          <path strokeLinecap="round" strokeLinejoin="round" d="M3.75 13.5l10.5-11.25L12 10.5h8.25L9.75 21.75 12 13.5H3.75z" />
                        </svg>
                      )}
                    </div>
                    <h4 className="text-white font-semibold">{modeLabels[m].title}</h4>
                  </div>
                  <p className="text-xs text-gray-400">{modeLabels[m].desc}</p>
                </button>
              ))}
            </div>
          </div>

          <div className="flex gap-3">
            <button onClick={() => setStep(1)} className="btn-secondary">
              Back
            </button>
          </div>
        </div>
      )}

      {/* Step 3: Configure */}
      {step === 3 && (
        <div className="grid grid-cols-1 xl:grid-cols-2 gap-6">
          <div className="card">
            <h3 className="card-header">
              {mode === 'predict_only' ? 'Prediction Configuration' : 'Mesh Configuration'}
            </h3>
            <div className="space-y-4">
              <div>
                <label className="label">Flow Directions</label>
                <div className="flex gap-3">
                  {['x', 'y', 'z'].map((d) => (
                    <label key={d} className="flex items-center gap-2 cursor-pointer">
                      <input
                        type="checkbox"
                        checked={flowDirs[d]}
                        onChange={(e) =>
                          setFlowDirs((prev) => ({ ...prev, [d]: e.target.checked }))
                        }
                        className="h-4 w-4 rounded border-gray-600 bg-gray-800 text-primary-500 focus:ring-primary-500"
                      />
                      <span className="text-sm text-gray-300 font-bold">{d.toUpperCase()}</span>
                    </label>
                  ))}
                </div>
              </div>

              <div>
                <label className="label">Voxel Size (m)</label>
                <NumberInput
                  step="1e-7"
                  className="input-field"
                  value={voxelSize}
                  onChange={setVoxelSize}
                />
              </div>

              <div>
                <label className="label">Voxel Resolution</label>
                <NumberInput
                  min={10}
                  max={640}
                  className="input-field"
                  value={voxelRes}
                  onChange={setVoxelRes}
                />
              </div>

              {(mode === 'predict_only' || mode === 'mesh_predict' || mode === 'full') && (
                <div>
                  <label className="label">ML Model</label>
                  {modelSets.length > 0 ? (
                    <select
                      className="input-field"
                      value={selectedModelFolder}
                      onChange={(e) => {
                        const folder = e.target.value
                        setSelectedModelFolder(folder)
                        const ms = modelSets.find((m) => m.folder === folder)
                        if (ms) setModelRes(ms.resolution)
                      }}
                    >
                      {modelSets.map((ms) => {
                        const isFNO = ms.folder.toLowerCase().includes('fno')
                        const arch = isFNO ? 'FNO, ~28.3M params' : '3D U-Net, ~1.4M params'
                        return (
                          <option key={ms.folder} value={ms.folder}>
                            {ms.folder} — {arch} (res {ms.resolution}, {ms.directions.join('/')})
                          </option>
                        )
                      })}
                    </select>
                  ) : (
                    <p className="text-sm text-gray-500">No models found</p>
                  )}
                  <p className="text-xs text-yellow-500/80 mt-2">
                    Only low-resolution models (80 voxels) are currently available. Inaccurate predictions may lead to longer convergence times — monitor residuals and consider disabling prediction if convergence stalls.
                  </p>
                </div>
              )}

              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="label">Inlet Buffer (voxels)</label>
                  <NumberInput
                    min={0}
                    max={50}
                    className="input-field"
                    value={inletBuffer}
                    onChange={setInletBuffer}
                  />
                </div>
                <div>
                  <label className="label">Outlet Buffer (voxels)</label>
                  <NumberInput
                    min={0}
                    max={50}
                    className="input-field"
                    value={outletBuffer}
                    onChange={setOutletBuffer}
                  />
                </div>
              </div>

              {mode !== 'predict_only' && (
                <div className="bg-gray-900 border border-gray-700 rounded-lg p-4">
                  <label className="flex items-center gap-3 cursor-pointer">
                    <input
                      type="checkbox"
                      checked={connectivity}
                      onChange={(e) => setConnectivity(e.target.checked)}
                      className="h-5 w-5 rounded border-gray-600 bg-gray-800 text-primary-500 focus:ring-primary-500"
                    />
                    <span className="text-sm text-white font-bold">Connectivity Check</span>
                  </label>
                  <p className="text-xs text-gray-400 mt-2 ml-8">
                    Remove disconnected fluid regions before meshing.
                    {' '}<span className="text-gray-500">Uncheck to skip this step.</span>
                  </p>
                </div>
              )}
            </div>
          </div>

          <div className="xl:col-span-2">
            <div className="card">
              <h3 className="card-header">Output Folder</h3>
              <div>
                <label className="label">Output Directory</label>
                <FolderPicker
                  value={outputDir}
                  onChange={setOutputDir}
                />
                <p className="text-xs text-gray-500 mt-1">
                  Results will be saved directly to this folder. Click to browse and select a directory.
                </p>
              </div>
            </div>
          </div>

          <div className="xl:col-span-2 flex gap-3">
            <button onClick={() => setStep(2)} className="btn-secondary">
              Back
            </button>
            <button onClick={() => setStep(4)} className="btn-primary">
              Review Settings
            </button>
          </div>
        </div>
      )}

      {/* Step 4: Review & Launch */}
      {step === 4 && (
        <div className="space-y-6">
          <div className="card">
            <h3 className="card-header">Review Configuration</h3>
            <div className="grid grid-cols-2 md:grid-cols-4 gap-6">
              <div>
                <p className="stat-label">Geometry</p>
                <p className="text-sm text-gray-200 font-mono truncate">
                  {activeFile || 'N/A'}
                </p>
              </div>
              <div>
                <p className="stat-label">Mode</p>
                <p className="text-sm text-gray-200 font-semibold">{modeLabels[mode].title}</p>
              </div>
              <div>
                <p className="stat-label">Flow Directions</p>
                <p className="text-sm text-gray-200 font-bold">
                  {selectedDirs.map((d) => d.toUpperCase()).join(', ') || 'None'}
                </p>
              </div>
              <div>
                <p className="stat-label">Resolution</p>
                <p className="text-sm text-gray-200">{voxelRes}</p>
              </div>
              <div>
                <p className="stat-label">Voxel Size</p>
                <p className="text-sm text-gray-200 font-mono">{voxelSize.toExponential(1)} m</p>
              </div>
              <div>
                <p className="stat-label">Buffer Zones</p>
                <p className="text-sm text-gray-200">In: {inletBuffer} / Out: {outletBuffer}</p>
              </div>
              <div>
                <p className="stat-label">Connectivity Check</p>
                <p className={`text-sm font-semibold ${connectivity ? 'text-green-400' : 'text-yellow-400'}`}>
                  {connectivity ? 'Enabled' : 'Skipped'}
                </p>
              </div>
              <div>
                <p className="stat-label">Output Folder</p>
                <p className="text-sm text-gray-200 font-mono truncate">
                  {outputDir || '(default)'}
                </p>
              </div>
              {(remapResult || alignResult) && (
                <div>
                  <p className="stat-label">Preprocessing</p>
                  <p className="text-sm text-green-400 font-semibold">
                    {remapResult ? 'Remapped' : ''}{remapResult && alignResult ? ' + ' : ''}{alignResult ? 'Aligned' : ''}
                  </p>
                </div>
              )}
            </div>
          </div>

          <div className="flex gap-3">
            <button onClick={() => setStep(3)} className="btn-secondary">
              Back
            </button>
            <button onClick={handleLaunch} className="btn-primary flex items-center gap-2">
              <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                <path strokeLinecap="round" strokeLinejoin="round" d="M5.25 5.653c0-.856.917-1.398 1.667-.986l11.54 6.348a1.125 1.125 0 010 1.971l-11.54 6.347a1.125 1.125 0 01-1.667-.985V5.653z" />
              </svg>
              Launch Pipeline
            </button>
          </div>
        </div>
      )}

      {/* Step 5: Progress */}
      {step === 5 && (
        <div className="grid grid-cols-1 xl:grid-cols-3 gap-6">
          <div className="space-y-6">
            <RamMonitor active={pipelineStatus?.status === 'running' || pipelineStatus?.status === 'queued'} />
            <div className="card">
              <h3 className="card-header">Pipeline Progress</h3>

              {/* Timeline / stepper */}
              <div className="space-y-4">
                {pipelineStatus?.steps?.map((s, i) => {
                  const isCurrent = s.name === pipelineStatus.currentStep
                  const isDone = s.status === 'completed'
                  const isFailed = s.status === 'error' || s.status === 'failed'

                  return (
                    <div key={i} className="flex items-start gap-3">
                      <div className="flex flex-col items-center">
                        <div
                          className={`w-8 h-8 rounded-full flex items-center justify-center text-xs font-bold ${
                            isDone
                              ? 'bg-green-600/30 text-green-400'
                              : isFailed
                                ? 'bg-red-600/30 text-red-400'
                                : isCurrent
                                  ? 'bg-primary-600/30 text-primary-400'
                                  : 'bg-gray-800 text-gray-600'
                          }`}
                        >
                          {isDone ? (
                            <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={3}>
                              <path strokeLinecap="round" strokeLinejoin="round" d="M4.5 12.75l6 6 9-13.5" />
                            </svg>
                          ) : isCurrent ? (
                            <svg className="w-4 h-4 animate-spin" fill="none" viewBox="0 0 24 24">
                              <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
                              <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
                            </svg>
                          ) : isFailed ? (
                            <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                              <path strokeLinecap="round" strokeLinejoin="round" d="M6 18L18 6M6 6l12 12" />
                            </svg>
                          ) : (
                            i + 1
                          )}
                        </div>
                        {i < (pipelineStatus?.steps?.length ?? 0) - 1 && (
                          <div className={`w-0.5 h-6 ${isDone ? 'bg-green-600/50' : 'bg-gray-800'}`} />
                        )}
                      </div>
                      <div className="pt-1">
                        <p className={`text-sm font-medium ${
                          isCurrent ? 'text-primary-400' : isDone ? 'text-green-400' : isFailed ? 'text-red-400' : 'text-gray-500'
                        }`}>
                          {s.name}
                        </p>
                        {isCurrent && (
                          <div className="mt-1">
                            <div className="flex items-center gap-2">
                              <div className="flex-1 h-1.5 bg-gray-800 rounded-full overflow-hidden w-24">
                                <div
                                  className="h-full bg-primary-600 rounded-full transition-all"
                                  style={{ width: `${Math.max(0, Math.min(100, s.progress))}%` }}
                                />
                              </div>
                              <span className="text-xs text-gray-500">{Math.round(s.progress)}%</span>
                            </div>
                          </div>
                        )}
                      </div>
                    </div>
                  )
                })}

                {!pipelineStatus?.steps?.length && (
                  <div className="flex items-center gap-2 text-sm text-gray-400">
                    <svg className="animate-spin h-4 w-4 text-primary-500" fill="none" viewBox="0 0 24 24">
                      <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
                      <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
                    </svg>
                    Starting pipeline...
                  </div>
                )}
              </div>

              <div className="mt-6 space-y-3">
                {pipelineStatus?.status === 'completed' ? (
                  <button onClick={() => setStep(6)} className="btn-primary w-full flex items-center justify-center gap-2">
                    <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                      <path strokeLinecap="round" strokeLinejoin="round" d="M13.5 4.5L21 12m0 0l-7.5 7.5M21 12H3" />
                    </svg>
                    View Results
                  </button>
                ) : (
                  <>
                    {/* Stop & Write — only during simulate steps */}
                    {pipelineStatus?.steps?.some(
                      (s) => s.name?.startsWith('simulate') && s.status === 'running'
                    ) && (
                      <button
                        onClick={handleStopSolver}
                        disabled={stopping}
                        className="btn-secondary w-full flex items-center justify-center gap-2"
                      >
                        <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                          <path strokeLinecap="round" strokeLinejoin="round" d="M5.25 7.5A2.25 2.25 0 017.5 5.25h9a2.25 2.25 0 012.25 2.25v9a2.25 2.25 0 01-2.25 2.25h-9a2.25 2.25 0 01-2.25-2.25v-9z" />
                        </svg>
                        {stopping ? 'Stopping...' : 'Stop & Write Current State'}
                      </button>
                    )}
                    <button onClick={handleCancel} className="btn-danger w-full">
                      Cancel Pipeline
                    </button>
                  </>
                )}
              </div>
            </div>
          </div>

          <div className="xl:col-span-2">
            <div className="card">
              <h3 className="card-header">Live Output</h3>
              <div
                className="bg-gray-950 rounded-lg p-4 font-mono text-xs text-gray-300 overflow-y-auto"
                style={{ maxHeight: '500px' }}
                ref={(el) => {
                  if (el) el.scrollTop = el.scrollHeight
                }}
              >
                {(() => {
                  // Show logs from all steps (completed + current)
                  const allLogs: string[] = []
                  for (const s of pipelineStatus?.steps ?? []) {
                    if (s.log && s.log.length > 0) {
                      allLogs.push(`--- ${s.name} ---`)
                      allLogs.push(...s.log)
                    }
                  }
                  if (allLogs.length === 0) {
                    return (
                      <div className="flex items-center gap-2 text-gray-500">
                        <svg className="animate-spin h-4 w-4" fill="none" viewBox="0 0 24 24">
                          <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
                          <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
                        </svg>
                        Waiting for output...
                      </div>
                    )
                  }
                  return allLogs.map((line, i) => (
                    <div
                      key={i}
                      className={
                        line.startsWith('---')
                          ? 'text-primary-400 font-bold mt-2'
                          : line.includes('[ERROR]') || line.includes('Error')
                            ? 'text-red-400'
                            : line.includes('[WARN]')
                              ? 'text-yellow-400'
                              : ''
                      }
                    >
                      {line}
                    </div>
                  ))
                })()}
              </div>
            </div>

            {/* Convergence chart for simulate steps */}
            {pipelineStatus?.steps?.some(
              (s) => s.name?.startsWith('simulate') && s.residuals && s.residuals.length >= 2,
            ) && (
              <ConvergenceChart
                residuals={
                  pipelineStatus.steps.find(
                    (s) =>
                      s.name?.startsWith('simulate') &&
                      (s.status === 'running' || s.status === 'completed') &&
                      s.residuals?.length,
                  )?.residuals ?? []
                }
                convWindow={convWindow}
              />
            )}
          </div>
        </div>
      )}

      {/* Step 6: Results */}
      {step === 6 && (
        <div className="space-y-6">
          <div className="card">
            <h3 className="card-header">Pipeline Complete</h3>
            <div className="flex items-center gap-3 text-green-400">
              <svg className="w-6 h-6" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                <path strokeLinecap="round" strokeLinejoin="round" d="M9 12.75L11.25 15 15 9.75M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
              </svg>
              <span className="font-medium">All pipeline steps completed successfully.</span>
            </div>
          </div>

          {(mode === 'full' || mode === 'predict_only') && (
            <PermeabilityTable results={results} />
          )}

          {/* Save to folder — not shown for predict_only (no OpenFOAM case) */}
          {mode !== 'predict_only' && <div className="card">
            <h3 className="card-header">Save Results</h3>
            <div className="space-y-3">
              <FolderPicker
                value={outputDir}
                onChange={setOutputDir}
              />
              <button
                onClick={() => handleSaveToFolder(false)}
                disabled={saving || !outputDir}
                className="btn-primary w-full flex items-center justify-center gap-2"
              >
                <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M2.25 12.75V12A2.25 2.25 0 014.5 9.75h15A2.25 2.25 0 0121.75 12v.75m-8.69-6.44l-2.12-2.12a1.5 1.5 0 00-1.061-.44H4.5A2.25 2.25 0 002.25 6v12a2.25 2.25 0 002.25 2.25h15A2.25 2.25 0 0021.75 18V9a2.25 2.25 0 00-2.25-2.25h-5.379a1.5 1.5 0 01-1.06-.44z" />
                </svg>
                {saving ? 'Saving...' : 'Save to Folder'}
              </button>
              {!outputDir && (
                <p className="text-xs text-gray-500">Select a folder above to save results.</p>
              )}
              {confirmOverwrite && (
                <div className="p-3 rounded-lg bg-yellow-900/20 border border-yellow-800 space-y-3">
                  <p className="text-sm text-yellow-300">
                    The following folder(s) already exist at the destination:
                  </p>
                  <ul className="text-xs text-yellow-200 font-mono list-disc list-inside">
                    {confirmOverwrite.map((d) => <li key={d}>{d}</li>)}
                  </ul>
                  <div className="flex gap-2">
                    <button
                      onClick={() => handleSaveToFolder(true)}
                      disabled={saving}
                      className="btn-primary text-sm flex-1"
                    >
                      {saving ? 'Overwriting...' : 'Overwrite'}
                    </button>
                    <button
                      onClick={() => setConfirmOverwrite(null)}
                      className="btn-secondary text-sm flex-1"
                    >
                      Cancel
                    </button>
                  </div>
                </div>
              )}
              {saveSuccess && (
                <div className="p-3 rounded-lg bg-green-900/20 border border-green-800 text-green-300 text-sm">
                  {saveSuccess}
                </div>
              )}
            </div>
          </div>}

          <div className="flex gap-3">
            {mode !== 'predict_only' && (
              <button onClick={handleDownload} className="btn-secondary flex items-center gap-2">
                <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M3 16.5v2.25A2.25 2.25 0 005.25 21h13.5A2.25 2.25 0 0021 18.75V16.5M16.5 12L12 16.5m0 0L7.5 12m4.5 4.5V3" />
                </svg>
                Download as Archive
              </button>
            )}
            <button onClick={handleExportCsv} className="btn-secondary flex items-center gap-2">
              <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                <path strokeLinecap="round" strokeLinejoin="round" d="M19.5 14.25v-2.625a3.375 3.375 0 00-3.375-3.375h-1.5A1.125 1.125 0 0113.5 7.125v-1.5a3.375 3.375 0 00-3.375-3.375H8.25m2.25 0H5.625c-.621 0-1.125.504-1.125 1.125v17.25c0 .621.504 1.125 1.125 1.125h12.75c.621 0 1.125-.504 1.125-1.125V11.25a9 9 0 00-9-9z" />
              </svg>
              Export CSV
            </button>
            <button onClick={handleReset} className="btn-secondary">
              New Pipeline
            </button>
          </div>
        </div>
      )}
    </div>
  )
}
