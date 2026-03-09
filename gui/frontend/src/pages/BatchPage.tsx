import { useState, useEffect, useCallback, useRef } from 'react'
import NumberInput from '../components/NumberInput'
import FolderPicker from '../components/FolderPicker'
import {
  browseFilesystem,
  listModelSets,
  runBatch,
  getBatchStatus,
  getResultsCsv,
  type PipelineMode,
  type BatchStatus,
  type PipelineStatus,
  type ModelSet,
  type BrowseEntry,
} from '../api/client'

const MODE_LABELS: Record<PipelineMode, { title: string; desc: string }> = {
  mesh_only: { title: 'Mesh Only', desc: 'Generate OpenFOAM hex-mesh' },
  predict_only: { title: 'Prediction Only', desc: 'ML-based permeability prediction' },
  mesh_predict: { title: 'Mesh + Predict', desc: 'Mesh generation with ML prediction' },
  full: { title: 'Full Simulation', desc: 'Mesh, predict, and CFD solve' },
}

const STATUS_COLORS: Record<string, string> = {
  completed: 'text-green-400',
  running: 'text-blue-400',
  queued: 'text-yellow-400',
  pending: 'text-gray-500',
  error: 'text-red-400',
  failed: 'text-red-400',
}

const STATUS_BG: Record<string, string> = {
  completed: 'bg-green-600',
  running: 'bg-blue-600',
  queued: 'bg-yellow-600',
  pending: 'bg-gray-700',
  error: 'bg-red-600',
  failed: 'bg-red-600',
}

function StatusIcon({ status }: { status: string }) {
  if (status === 'completed') {
    return (
      <svg className="w-4 h-4 text-green-400" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2.5}>
        <path strokeLinecap="round" strokeLinejoin="round" d="M4.5 12.75l6 6 9-13.5" />
      </svg>
    )
  }
  if (status === 'running') {
    return (
      <svg className="w-4 h-4 text-blue-400 animate-spin" fill="none" viewBox="0 0 24 24">
        <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
        <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
      </svg>
    )
  }
  if (status === 'error' || status === 'failed') {
    return (
      <svg className="w-4 h-4 text-red-400" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
        <path strokeLinecap="round" strokeLinejoin="round" d="M6 18L18 6M6 6l12 12" />
      </svg>
    )
  }
  return <div className="w-4 h-4 rounded-full bg-gray-700" />
}

const GEOM_EXTENSIONS = ['.dat', '.npy', '.raw']

function isGeomFile(name: string): boolean {
  return GEOM_EXTENSIONS.some((ext) => name.toLowerCase().endsWith(ext))
}

/** Selected file from server-side browsing */
interface SelectedFile {
  name: string
  absPath: string
  selected: boolean
}

function pipelineProgress(p: PipelineStatus): number {
  if (!p.steps || p.steps.length === 0) {
    if (p.status === 'completed') return 100
    return 0
  }
  const total = p.steps.length
  let done = 0
  for (const s of p.steps) {
    if (s.status === 'completed') done += 1
    else if (s.status === 'running') done += s.progress / 100
  }
  return (done / total) * 100
}

export default function BatchPage() {
  // Server-side folder browser state
  const [browserOpen, setBrowserOpen] = useState(false)
  const [browserPath, setBrowserPath] = useState('')
  const [browserDirs, setBrowserDirs] = useState<BrowseEntry[]>([])
  const [browserFiles, setBrowserFiles] = useState<BrowseEntry[]>([])
  const [browserRoot, setBrowserRoot] = useState('')
  const [browserAbsPath, setBrowserAbsPath] = useState('')
  const [browserLoading, setBrowserLoading] = useState(false)
  const [browserError, setBrowserError] = useState<string | null>(null)

  // Selected files for processing
  const [selectedFiles, setSelectedFiles] = useState<SelectedFile[]>([])
  const [selectedFolderDisplay, setSelectedFolderDisplay] = useState('')

  // Config
  const [mode, setMode] = useState<PipelineMode>('full')
  const [flowDirs, setFlowDirs] = useState<Record<string, boolean>>({ x: true, y: false, z: false })
  const [voxelSize, setVoxelSize] = useState(1e-6)
  const [voxelRes, setVoxelRes] = useState(80)
  const [modelRes, setModelRes] = useState(80)
  const [inletBuffer, setInletBuffer] = useState(5)
  const [outletBuffer, setOutletBuffer] = useState(5)
  const [connectivity, setConnectivity] = useState(true)
  const [outputDir, setOutputDir] = useState('')
  const [modelSets, setModelSets] = useState<ModelSet[]>([])
  const [selectedModelFolder, setSelectedModelFolder] = useState('')

  // Preprocessing
  const [enableRemap, setEnableRemap] = useState(false)
  const [poreValue, setPoreValue] = useState(0)
  const [otherMapping, setOtherMapping] = useState<'solid' | 'pore'>('solid')
  const [autoAlign, setAutoAlign] = useState(false)

  const solver = 'simpleFoamMod'
  const maxIter = 10000
  const writeInterval = 10000

  // Batch status — restore from sessionStorage so navigation doesn't lose track
  const [batchId, setBatchIdRaw] = useState<string | null>(
    () => sessionStorage.getItem('fiberfoam_batch_id'),
  )
  const [batchStatus, setBatchStatus] = useState<BatchStatus | null>(null)
  const [running, setRunning] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const pollingRef = useRef<ReturnType<typeof setInterval> | null>(null)

  const setBatchId = useCallback((id: string | null) => {
    setBatchIdRaw(id)
    if (id) {
      sessionStorage.setItem('fiberfoam_batch_id', id)
    } else {
      sessionStorage.removeItem('fiberfoam_batch_id')
    }
  }, [])

  // On mount, if we have a stored batchId, start polling immediately
  useEffect(() => {
    if (batchId && !running) {
      // Check if the batch is still active
      getBatchStatus(batchId)
        .then((status) => {
          setBatchStatus(status)
          if (status.status === 'running' || status.status === 'queued') {
            setRunning(true)
          }
        })
        .catch(() => {
          // Batch no longer exists, clear it
          setBatchId(null)
        })
    }
  }, []) // eslint-disable-line react-hooks/exhaustive-deps

  // Load models
  useEffect(() => {
    listModelSets()
      .then((data) => {
        setModelSets(data.modelSets)
        if (data.modelSets.length > 0 && !selectedModelFolder) {
          setSelectedModelFolder(data.modelSets[0].folder)
          setModelRes(data.modelSets[0].resolution)
        }
      })
      .catch(() => {})
  }, []) // eslint-disable-line react-hooks/exhaustive-deps

  // Browse a directory on the server
  const browse = useCallback(async (path: string) => {
    setBrowserLoading(true)
    setBrowserError(null)
    try {
      const result = await browseFilesystem(path)
      setBrowserDirs(result.dirs)
      setBrowserFiles(result.files)
      setBrowserPath(result.path)
      setBrowserRoot(result.root)
      setBrowserAbsPath(result.absPath)
    } catch (err) {
      setBrowserError(err instanceof Error ? err.message : 'Failed to browse')
    } finally {
      setBrowserLoading(false)
    }
  }, [])

  const handleOpenBrowser = useCallback(() => {
    setBrowserOpen(true)
    browse('')
  }, [browse])

  const handleBrowserNavigate = useCallback(
    (dirName: string) => {
      const next = browserPath ? `${browserPath}/${dirName}` : dirName
      browse(next)
    },
    [browserPath, browse],
  )

  const handleBrowserUp = useCallback(() => {
    const parts = browserPath.split('/').filter(Boolean)
    parts.pop()
    browse(parts.join('/'))
  }, [browserPath, browse])

  // Select this folder: take its files and show them with checkboxes
  const handleSelectFolder = useCallback(() => {
    const geomFiles = browserFiles
      .filter((f) => f.size && f.size > 0)
      .map((f) => ({
        name: f.name,
        absPath: `${browserAbsPath}/${f.name}`,
        selected: isGeomFile(f.name),
      }))
      .sort((a, b) => {
        const aGeom = isGeomFile(a.name) ? 0 : 1
        const bGeom = isGeomFile(b.name) ? 0 : 1
        if (aGeom !== bGeom) return aGeom - bGeom
        return a.name.localeCompare(b.name)
      })

    setSelectedFiles(geomFiles)
    setSelectedFolderDisplay(`${browserRoot}/${browserPath}`)
    setBrowserOpen(false)
    setError(null)
    setBatchStatus(null)
    setBatchId(null)

    if (geomFiles.length === 0) {
      setError('No files found in selected folder')
    }
  }, [browserFiles, browserAbsPath, browserRoot, browserPath])

  const toggleFile = useCallback((idx: number) => {
    setSelectedFiles((prev) =>
      prev.map((f, i) => (i === idx ? { ...f, selected: !f.selected } : f)),
    )
  }, [])

  const toggleAll = useCallback(() => {
    setSelectedFiles((prev) => {
      const allSelected = prev.every((f) => f.selected)
      return prev.map((f) => ({ ...f, selected: !allSelected }))
    })
  }, [])

  const selectedCount = selectedFiles.filter((f) => f.selected).length

  // Run batch — files are already on the server, just pass absolute paths
  const handleRunBatch = useCallback(async () => {
    setError(null)
    const selectedDirs = Object.entries(flowDirs)
      .filter(([, v]) => v)
      .map(([k]) => k)

    const filesToProcess = selectedFiles.filter((f) => f.selected)
    if (filesToProcess.length === 0) {
      setError('Select at least one geometry file')
      return
    }
    if (selectedDirs.length === 0) {
      setError('Select at least one flow direction')
      return
    }

    setRunning(true)

    try {
      const res = await runBatch({
        mode,
        flowDirections: selectedDirs,
        voxelSize,
        voxelRes,
        modelRes,
        inletBuffer,
        outletBuffer,
        connectivity,
        solver,
        maxIter,
        writeInterval,
        inputFiles: filesToProcess.map((f) => f.absPath),
        ...(outputDir ? { outputDir } : {}),
        ...(enableRemap ? { remapPoreValue: poreValue, remapOtherMapping: otherMapping } : {}),
        autoAlign,
      })
      setBatchId(res.batchId)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to start batch')
      setRunning(false)
    }
  }, [mode, flowDirs, voxelSize, voxelRes, modelRes, inletBuffer, outletBuffer, connectivity, selectedFiles, outputDir])

  // Poll batch status
  useEffect(() => {
    if (!batchId || !running) return

    const poll = async () => {
      try {
        const status = await getBatchStatus(batchId)
        setBatchStatus(status)
        if (status.status === 'completed' || status.status === 'error') {
          setRunning(false)
        }
      } catch {
        // Ignore transient errors
      }
    }

    poll()
    pollingRef.current = setInterval(poll, 3000)
    return () => {
      if (pollingRef.current) clearInterval(pollingRef.current)
    }
  }, [batchId, running])

  const handleExportAll = useCallback(async () => {
    if (!batchStatus?.pipelines) return
    for (const p of batchStatus.pipelines) {
      if (p.status === 'completed') {
        try {
          const blob = await getResultsCsv(p.pipelineId)
          const url = URL.createObjectURL(blob)
          const a = document.createElement('a')
          a.href = url
          a.download = `results_${p.pipelineId}.csv`
          document.body.appendChild(a)
          a.click()
          document.body.removeChild(a)
          URL.revokeObjectURL(url)
        } catch {
          // Skip
        }
      }
    }
  }, [batchStatus])

  const completedCount = batchStatus?.pipelines?.filter((p) => p.status === 'completed').length ?? 0
  const failedCount = batchStatus?.pipelines?.filter((p) => p.status === 'error' || p.status === 'failed').length ?? 0
  const totalCount = batchStatus?.totalFiles ?? 0
  const overallProgress = totalCount > 0 ? (batchStatus?.completedFiles ?? 0) / totalCount * 100 : 0

  const browserPathParts = browserPath ? browserPath.split('/').filter(Boolean) : []

  return (
    <div className="space-y-6">
      <div>
        <h2 className="text-2xl font-bold text-white">Batch Processing</h2>
        <p className="text-sm text-gray-400 mt-1">
          Select a folder, pick geometry files, and run pipelines with the same settings.
        </p>
      </div>

      {error && (
        <div className="p-4 rounded-lg bg-red-900/30 border border-red-800 text-red-300 text-sm whitespace-pre-line">
          {error}
        </div>
      )}

      {/* Folder browser modal */}
      {browserOpen && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm">
          <div className="bg-gray-900 border border-gray-700 rounded-xl shadow-2xl w-full max-w-lg mx-4">
            <div className="flex items-center justify-between px-5 py-4 border-b border-gray-800">
              <h3 className="text-white font-semibold">Select Input Folder</h3>
              <button
                onClick={() => setBrowserOpen(false)}
                className="text-gray-500 hover:text-gray-300 transition-colors"
              >
                <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M6 18L18 6M6 6l12 12" />
                </svg>
              </button>
            </div>

            {/* Breadcrumb */}
            <div className="px-5 py-3 border-b border-gray-800 flex items-center gap-1 text-sm overflow-x-auto">
              <button
                onClick={() => browse('')}
                className="text-primary-400 hover:text-primary-300 shrink-0 font-medium"
              >
                Home
              </button>
              {browserPathParts.map((part, i) => (
                <span key={i} className="flex items-center gap-1">
                  <span className="text-gray-600">/</span>
                  <button
                    onClick={() => browse(browserPathParts.slice(0, i + 1).join('/'))}
                    className="text-primary-400 hover:text-primary-300 font-mono text-xs"
                  >
                    {part}
                  </button>
                </span>
              ))}
            </div>

            {/* Directory list */}
            <div className="px-5 py-3 max-h-64 overflow-y-auto min-h-[160px]">
              {browserLoading ? (
                <div className="flex items-center gap-2 text-sm text-gray-400 py-4">
                  <svg className="animate-spin h-4 w-4 text-primary-500" fill="none" viewBox="0 0 24 24">
                    <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
                    <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
                  </svg>
                  Loading...
                </div>
              ) : browserError ? (
                <div className="text-sm text-red-400 py-4">{browserError}</div>
              ) : (
                <div className="space-y-1">
                  {browserPath && (
                    <button
                      onClick={handleBrowserUp}
                      className="flex items-center gap-3 w-full px-3 py-2 rounded-lg hover:bg-gray-800/50 transition-colors text-left"
                    >
                      <svg className="w-4 h-4 text-gray-500" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                        <path strokeLinecap="round" strokeLinejoin="round" d="M9 15L3 9m0 0l6-6M3 9h12a6 6 0 010 12h-3" />
                      </svg>
                      <span className="text-sm text-gray-400">..</span>
                    </button>
                  )}

                  {browserDirs.map((d) => (
                    <button
                      key={d.name}
                      onClick={() => handleBrowserNavigate(d.name)}
                      className="flex items-center gap-3 w-full px-3 py-2 rounded-lg hover:bg-gray-800/50 transition-colors text-left"
                    >
                      <svg className="w-4 h-4 text-yellow-500" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                        <path strokeLinecap="round" strokeLinejoin="round" d="M2.25 12.75V12A2.25 2.25 0 014.5 9.75h15A2.25 2.25 0 0121.75 12v.75m-8.69-6.44l-2.12-2.12a1.5 1.5 0 00-1.061-.44H4.5A2.25 2.25 0 002.25 6v12a2.25 2.25 0 002.25 2.25h15A2.25 2.25 0 0021.75 18V9a2.25 2.25 0 00-2.25-2.25h-5.379a1.5 1.5 0 01-1.06-.44z" />
                      </svg>
                      <span className="text-sm text-gray-200 font-mono">{d.name}</span>
                    </button>
                  ))}

                  {/* Show file count hint */}
                  {browserFiles.length > 0 && (
                    <div className="text-xs text-gray-500 pt-2 px-3">
                      {browserFiles.length} file{browserFiles.length !== 1 ? 's' : ''} in this folder
                      {' '}({browserFiles.filter((f) => isGeomFile(f.name)).length} geometry)
                    </div>
                  )}

                  {browserDirs.length === 0 && browserFiles.length === 0 && (
                    <p className="text-sm text-gray-500 py-2">Empty directory.</p>
                  )}
                </div>
              )}
            </div>

            {/* Footer */}
            <div className="px-5 py-4 border-t border-gray-800 flex items-center justify-between">
              <div className="text-xs text-gray-500 truncate mr-4">
                {browserRoot}/{browserPath || ''}
              </div>
              <div className="flex gap-2 shrink-0">
                <button onClick={() => setBrowserOpen(false)} className="btn-secondary text-sm">
                  Cancel
                </button>
                <button
                  onClick={handleSelectFolder}
                  disabled={browserFiles.length === 0}
                  className="btn-primary text-sm"
                >
                  Select This Folder
                </button>
              </div>
            </div>
          </div>
        </div>
      )}

      <div className="grid grid-cols-1 xl:grid-cols-3 gap-6">
        {/* Left: File selection + Config */}
        <div className="space-y-6">
          {/* Folder selection */}
          <div className="card">
            <h3 className="card-header">Geometry Files</h3>

            <button
              onClick={handleOpenBrowser}
              disabled={running}
              className="flex flex-col items-center justify-center w-full h-28 border-2 border-dashed border-gray-600 rounded-lg cursor-pointer hover:border-primary-500 hover:bg-gray-800/50 transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
            >
              <svg className="w-8 h-8 text-gray-500 mb-2" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                <path strokeLinecap="round" strokeLinejoin="round" d="M3.75 9.776c.112-.017.227-.026.344-.026h15.812c.117 0 .232.009.344.026m-16.5 0a2.25 2.25 0 00-1.883 2.542l.857 6a2.25 2.25 0 002.227 1.932H19.05a2.25 2.25 0 002.227-1.932l.857-6a2.25 2.25 0 00-1.883-2.542m-16.5 0V6A2.25 2.25 0 016 3.75h3.879a1.5 1.5 0 011.06.44l2.122 2.12a1.5 1.5 0 001.06.44H18A2.25 2.25 0 0120.25 9v.776" />
              </svg>
              <span className={`text-sm ${selectedFolderDisplay ? 'text-gray-200' : 'text-gray-400'}`}>
                {selectedFolderDisplay || 'Click to browse for a folder'}
              </span>
              <span className="text-xs text-gray-600 mt-1">Browse server filesystem to find geometry files</span>
            </button>

            {selectedFiles.length > 0 && (
              <div className="mt-3">
                <div className="flex items-center justify-between mb-2">
                  <span className="text-xs text-gray-500">
                    {selectedCount} / {selectedFiles.length} file{selectedFiles.length !== 1 ? 's' : ''} selected
                  </span>
                  <button
                    onClick={toggleAll}
                    disabled={running}
                    className="text-xs text-primary-400 hover:text-primary-300"
                  >
                    {selectedFiles.every((f) => f.selected) ? 'Deselect All' : 'Select All'}
                  </button>
                </div>
                <div className="space-y-1 max-h-64 overflow-y-auto">
                  {selectedFiles.map((f, idx) => (
                    <label
                      key={f.absPath}
                      className={`flex items-center gap-2 px-3 py-2 rounded-lg border cursor-pointer transition-colors ${
                        f.selected
                          ? 'bg-primary-600/10 border-primary-600/30'
                          : 'bg-gray-800/30 border-gray-700 hover:border-gray-600'
                      }`}
                    >
                      <input
                        type="checkbox"
                        checked={f.selected}
                        onChange={() => toggleFile(idx)}
                        disabled={running}
                        className="h-4 w-4 rounded border-gray-600 bg-gray-800 text-primary-500 focus:ring-primary-500 shrink-0"
                      />
                      <span className={`text-sm font-mono truncate ${isGeomFile(f.name) ? 'text-gray-200' : 'text-gray-500'}`}>
                        {f.name}
                      </span>
                    </label>
                  ))}
                </div>
              </div>
            )}
          </div>

          {/* Mode */}
          <div className="card">
            <h3 className="card-header">Pipeline Mode</h3>
            <div className="grid grid-cols-2 gap-2">
              {(Object.keys(MODE_LABELS) as PipelineMode[]).map((m) => (
                <button
                  key={m}
                  onClick={() => setMode(m)}
                  disabled={running}
                  className={`p-3 rounded-lg border text-left transition-all ${
                    mode === m
                      ? 'border-primary-500 bg-primary-600/10'
                      : 'border-gray-700 hover:border-gray-600 bg-gray-800/50'
                  }`}
                >
                  <p className={`text-xs font-semibold ${mode === m ? 'text-primary-400' : 'text-gray-300'}`}>
                    {MODE_LABELS[m].title}
                  </p>
                  <p className="text-[10px] text-gray-500 mt-0.5">{MODE_LABELS[m].desc}</p>
                </button>
              ))}
            </div>
          </div>

          {/* Preprocessing */}
          <div className="card">
            <h3 className="card-header">Preprocessing</h3>
            <p className="text-xs text-gray-500 mb-3">
              Applied identically to each selected file before running the pipeline.
            </p>
            <div className="space-y-3">
              {/* Remap */}
              <div className="bg-gray-900 border border-gray-700 rounded-lg p-4">
                <label className="flex items-center gap-3 cursor-pointer">
                  <input
                    type="checkbox"
                    checked={enableRemap}
                    onChange={(e) => setEnableRemap(e.target.checked)}
                    disabled={running}
                    className="h-5 w-5 rounded border-gray-600 bg-gray-800 text-primary-500 focus:ring-primary-500"
                  />
                  <span className="text-sm text-white font-bold">Remap Values</span>
                </label>
                <p className="text-xs text-gray-400 mt-2 ml-8">
                  Convert non-binary geometries to binary (pore=0, solid=1).
                </p>
                {enableRemap && (
                  <div className="mt-3 ml-8 space-y-3">
                    <div>
                      <label className="label text-xs">Pore Value</label>
                      <NumberInput
                        min={0}
                        max={255}
                        className="input-field"
                        value={poreValue}
                        onChange={setPoreValue}
                        disabled={running}
                      />
                      <p className="text-[10px] text-gray-500 mt-1">
                        Voxels with this value become fluid (0).
                      </p>
                    </div>
                    <div>
                      <label className="label text-xs">Other Values</label>
                      <select
                        className="input-field"
                        value={otherMapping}
                        onChange={(e) => setOtherMapping(e.target.value as 'solid' | 'pore')}
                        disabled={running}
                      >
                        <option value="solid">Treat as solid (1)</option>
                        <option value="pore">Treat as pore (0)</option>
                      </select>
                    </div>
                  </div>
                )}
              </div>

              {/* Auto-align */}
              <div className="bg-gray-900 border border-gray-700 rounded-lg p-4">
                <label className="flex items-center gap-3 cursor-pointer">
                  <input
                    type="checkbox"
                    checked={autoAlign}
                    onChange={(e) => setAutoAlign(e.target.checked)}
                    disabled={running}
                    className="h-5 w-5 rounded border-gray-600 bg-gray-800 text-primary-500 focus:ring-primary-500"
                  />
                  <span className="text-sm text-white font-bold">Auto-Align to X-Axis</span>
                </label>
                <p className="text-xs text-gray-400 mt-2 ml-8">
                  Estimate fiber orientation via FFT and rotate to align fibers with the X-axis.
                </p>
              </div>
            </div>
          </div>

          {/* Configuration */}
          <div className="card">
            <h3 className="card-header">Configuration</h3>
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
                        disabled={running}
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
                  disabled={running}
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
                  disabled={running}
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
                      disabled={running}
                    >
                      {modelSets.map((ms) => (
                        <option key={ms.folder} value={ms.folder}>
                          {ms.folder} (res {ms.resolution}, directions: {ms.directions.join(', ')})
                        </option>
                      ))}
                    </select>
                  ) : (
                    <p className="text-sm text-gray-500">No models found</p>
                  )}
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
                    disabled={running}
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
                    disabled={running}
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
                      disabled={running}
                      className="h-5 w-5 rounded border-gray-600 bg-gray-800 text-primary-500 focus:ring-primary-500"
                    />
                    <span className="text-sm text-white font-bold">Connectivity Check</span>
                  </label>
                  <p className="text-xs text-gray-400 mt-2 ml-8">
                    Remove disconnected fluid regions before meshing.
                  </p>
                </div>
              )}

              <div>
                <label className="label">Output Folder</label>
                <FolderPicker
                  value={outputDir}
                  onChange={setOutputDir}
                  disabled={running}
                />
                <p className="text-xs text-gray-500 mt-1">
                  Results will be saved directly to this folder.
                </p>
              </div>

              <button
                onClick={handleRunBatch}
                disabled={running || selectedCount === 0}
                className="btn-primary w-full flex items-center justify-center gap-2"
              >
                <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M5.25 5.653c0-.856.917-1.398 1.667-.986l11.54 6.348a1.125 1.125 0 010 1.971l-11.54 6.347a1.125 1.125 0 01-1.667-.985V5.653z" />
                </svg>
                {running ? 'Running...' : `Run Batch (${selectedCount} file${selectedCount !== 1 ? 's' : ''})`}
              </button>
            </div>
          </div>
        </div>

        {/* Right: Progress table + Summary */}
        <div className="xl:col-span-2 space-y-6">
          <div className="card">
            <h3 className="card-header">Batch Progress</h3>
            {!batchStatus ? (
              <div className="flex flex-col items-center justify-center py-12 text-center">
                <svg className="w-12 h-12 text-gray-700 mb-3" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M3.75 9.776c.112-.017.227-.026.344-.026h15.812c.117 0 .232.009.344.026m-16.5 0a2.25 2.25 0 00-1.883 2.542l.857 6a2.25 2.25 0 002.227 1.932H19.05a2.25 2.25 0 002.227-1.932l.857-6a2.25 2.25 0 00-1.883-2.542m-16.5 0V6A2.25 2.25 0 016 3.75h3.879a1.5 1.5 0 011.06.44l2.122 2.12a1.5 1.5 0 001.06.44H18A2.25 2.25 0 0120.25 9v.776" />
                </svg>
                <p className="text-sm text-gray-500">
                  Select a folder, pick geometry files, configure settings, then click Run Batch.
                </p>
              </div>
            ) : (
              <div className="overflow-x-auto">
                <table className="w-full text-sm">
                  <thead>
                    <tr className="border-b border-gray-800">
                      <th className="text-left py-3 px-4 text-xs font-medium text-gray-500 uppercase tracking-wider">#</th>
                      <th className="text-left py-3 px-4 text-xs font-medium text-gray-500 uppercase tracking-wider">File</th>
                      <th className="text-left py-3 px-4 text-xs font-medium text-gray-500 uppercase tracking-wider">Step</th>
                      <th className="text-left py-3 px-4 text-xs font-medium text-gray-500 uppercase tracking-wider">Status</th>
                      <th className="text-left py-3 px-4 text-xs font-medium text-gray-500 uppercase tracking-wider">Progress</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-gray-800">
                    {batchStatus.pipelines.map((p: PipelineStatus, idx: number) => {
                      const prog = pipelineProgress(p)
                      const inputFile = selectedFiles.filter((f) => f.selected)[idx]?.name ?? p.pipelineId
                      return (
                        <tr key={p.pipelineId} className="hover:bg-gray-800/50 transition-colors">
                          <td className="py-3 px-4 text-gray-500 text-xs">{idx + 1}</td>
                          <td className="py-3 px-4">
                            <span className="font-mono text-gray-300 text-xs">{inputFile}</span>
                          </td>
                          <td className="py-3 px-4">
                            <span className="text-xs text-gray-400">
                              {p.currentStep ?? (p.status === 'completed' ? 'Done' : '-')}
                            </span>
                          </td>
                          <td className="py-3 px-4">
                            <div className="flex items-center gap-2">
                              <StatusIcon status={p.status} />
                              <span className={`text-xs font-medium ${STATUS_COLORS[p.status] ?? 'text-gray-500'}`}>
                                {p.status.charAt(0).toUpperCase() + p.status.slice(1)}
                              </span>
                            </div>
                          </td>
                          <td className="py-3 px-4">
                            <div className="flex items-center gap-2">
                              <div className="flex-1 h-1.5 bg-gray-800 rounded-full overflow-hidden">
                                <div
                                  className={`h-full rounded-full transition-all ${STATUS_BG[p.status] ?? 'bg-gray-700'}`}
                                  style={{ width: `${Math.max(0, Math.min(100, prog))}%` }}
                                />
                              </div>
                              <span className="text-xs text-gray-500 w-8 text-right">
                                {Math.round(prog)}%
                              </span>
                            </div>
                          </td>
                        </tr>
                      )
                    })}
                  </tbody>
                </table>
              </div>
            )}
          </div>

          {/* Summary */}
          {batchStatus && !running && (
            <div className="card">
              <h3 className="card-header">Summary</h3>
              <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-4">
                <div>
                  <p className="stat-label">Total Files</p>
                  <p className="stat-value text-lg">{totalCount}</p>
                </div>
                <div>
                  <p className="stat-label">Completed</p>
                  <p className="stat-value text-lg text-green-400">{completedCount}</p>
                </div>
                <div>
                  <p className="stat-label">Failed</p>
                  <p className="stat-value text-lg text-red-400">{failedCount}</p>
                </div>
                <div>
                  <p className="stat-label">Overall Progress</p>
                  <p className="stat-value text-lg">{Math.round(overallProgress)}%</p>
                </div>
              </div>
              {completedCount > 0 && (
                <button onClick={handleExportAll} className="btn-secondary flex items-center gap-2">
                  <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                    <path strokeLinecap="round" strokeLinejoin="round" d="M3 16.5v2.25A2.25 2.25 0 005.25 21h13.5A2.25 2.25 0 0021 18.75V16.5M16.5 12L12 16.5m0 0L7.5 12m4.5 4.5V3" />
                  </svg>
                  Export All as CSV
                </button>
              )}
            </div>
          )}
        </div>
      </div>
    </div>
  )
}
