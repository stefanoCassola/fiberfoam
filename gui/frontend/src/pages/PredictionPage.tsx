import { useState, useEffect, useCallback } from 'react'
import ProgressIndicator from '../components/ProgressIndicator'
import {
  listModels,
  runPrediction,
  getVelocitySlice,
  type PredictionConfig,
  type PredictionResult,
} from '../api/client'

type Direction = 'x' | 'y' | 'z'

export default function PredictionPage() {
  const [models, setModels] = useState<string[]>([])
  const [selectedModel, setSelectedModel] = useState('')
  const [direction, setDirection] = useState<Direction>('x')
  const [running, setRunning] = useState(false)
  const [result, setResult] = useState<PredictionResult | null>(null)
  const [error, setError] = useState<string | null>(null)
  const [sliceData, setSliceData] = useState<number[][] | null>(null)
  const [sliceAxis, setSliceAxis] = useState<'x' | 'y' | 'z'>('z')
  const [sliceIndex, setSliceIndex] = useState(0)

  // Load available models
  useEffect(() => {
    listModels()
      .then((m) => {
        setModels(m)
        if (m.length > 0) setSelectedModel(m[0])
      })
      .catch(() => {
        // Backend may not be running yet
        setModels([])
      })
  }, [])

  const handleRunPrediction = useCallback(async () => {
    if (!selectedModel) return
    setError(null)
    setRunning(true)
    setResult(null)
    setSliceData(null)

    try {
      const config: PredictionConfig = {
        modelName: selectedModel,
        direction,
      }
      const res = await runPrediction(config)
      setResult(res)

      // Fetch initial slice
      const slice = await getVelocitySlice(sliceAxis, sliceIndex)
      setSliceData(slice.data)
    } catch (err) {
      setError(
        err instanceof Error ? err.message : 'Prediction failed',
      )
    } finally {
      setRunning(false)
    }
  }, [selectedModel, direction, sliceAxis, sliceIndex])

  const handleSliceChange = useCallback(
    async (axis: 'x' | 'y' | 'z', index: number) => {
      setSliceAxis(axis)
      setSliceIndex(index)
      try {
        const slice = await getVelocitySlice(axis, index)
        setSliceData(slice.data)
      } catch {
        // Silently fail -- the user may be adjusting the slider
      }
    },
    [],
  )

  // Velocity field color mapping
  const velocityToColor = (val: number, max: number): string => {
    if (max === 0) return 'rgb(0,0,0)'
    const ratio = Math.min(val / max, 1)
    const r = Math.round(255 * ratio)
    const b = Math.round(255 * (1 - ratio))
    return `rgb(${r},0,${b})`
  }

  const sliceMax =
    sliceData?.reduce(
      (mx, row) => Math.max(mx, ...row.map(Math.abs)),
      0,
    ) ?? 1

  return (
    <div className="space-y-6">
      <div>
        <h2 className="text-2xl font-bold text-white">Prediction</h2>
        <p className="text-sm text-gray-400 mt-1">
          Run ML model predictions on the loaded geometry.
        </p>
      </div>

      {error && (
        <div className="p-4 rounded-lg bg-red-900/30 border border-red-800 text-red-300 text-sm">
          {error}
        </div>
      )}

      <div className="grid grid-cols-1 xl:grid-cols-3 gap-6">
        {/* Controls */}
        <div className="space-y-6">
          <div className="card">
            <h3 className="card-header">Model Configuration</h3>
            <div className="space-y-4">
              <div>
                <label className="label">ML Model</label>
                <select
                  className="select-field"
                  value={selectedModel}
                  onChange={(e) => setSelectedModel(e.target.value)}
                  disabled={running}
                >
                  {models.length === 0 && (
                    <option value="">No models available</option>
                  )}
                  {models.map((m) => (
                    <option key={m} value={m}>
                      {m}
                    </option>
                  ))}
                </select>
              </div>

              <div>
                <label className="label">Flow Direction</label>
                <div className="grid grid-cols-3 gap-2">
                  {(['x', 'y', 'z'] as const).map((d) => (
                    <button
                      key={d}
                      onClick={() => setDirection(d)}
                      disabled={running}
                      className={`py-2 rounded-lg text-sm font-bold transition-colors ${
                        direction === d
                          ? 'bg-primary-600 text-white'
                          : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
                      }`}
                    >
                      {d.toUpperCase()}
                    </button>
                  ))}
                </div>
              </div>

              <button
                onClick={handleRunPrediction}
                disabled={running || !selectedModel}
                className="btn-primary w-full"
              >
                {running ? 'Running...' : 'Run Prediction'}
              </button>

              {running && (
                <ProgressIndicator
                  progress={-1}
                  label="Running ML prediction"
                  status="This may take a few minutes"
                />
              )}
            </div>
          </div>

          {/* Result summary */}
          {result && (
            <div className="card">
              <h3 className="card-header">Prediction Result</h3>
              <div className="space-y-3">
                <div>
                  <p className="stat-label">Model</p>
                  <p className="text-sm text-gray-200">{result.modelName}</p>
                </div>
                <div>
                  <p className="stat-label">Direction</p>
                  <p className="text-sm text-gray-200 font-bold">
                    {result.direction.toUpperCase()}
                  </p>
                </div>
                <div>
                  <p className="stat-label">Predicted Permeability</p>
                  <p className="stat-value">
                    {result.permeability.toExponential(4)}
                  </p>
                  <p className="text-xs text-gray-500 mt-0.5">m^2</p>
                </div>
              </div>
            </div>
          )}
        </div>

        {/* Velocity field slice viewer */}
        <div className="xl:col-span-2 space-y-4">
          <div className="card">
            <h3 className="card-header">Velocity Field Slice Viewer</h3>

            {/* Slice controls */}
            <div className="flex items-center gap-4 mb-4">
              <div className="flex items-center gap-2">
                <label className="text-sm text-gray-400">Axis:</label>
                <div className="flex gap-1">
                  {(['x', 'y', 'z'] as const).map((a) => (
                    <button
                      key={a}
                      onClick={() => handleSliceChange(a, sliceIndex)}
                      className={`px-2.5 py-1 text-xs rounded font-bold ${
                        sliceAxis === a
                          ? 'bg-primary-600 text-white'
                          : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
                      }`}
                    >
                      {a.toUpperCase()}
                    </button>
                  ))}
                </div>
              </div>
              <div className="flex-1 flex items-center gap-2">
                <label className="text-sm text-gray-400">Index:</label>
                <input
                  type="range"
                  min={0}
                  max={100}
                  value={sliceIndex}
                  onChange={(e) =>
                    handleSliceChange(sliceAxis, parseInt(e.target.value))
                  }
                  className="flex-1 accent-primary-500"
                />
                <span className="text-sm font-mono text-gray-300 w-8 text-right">
                  {sliceIndex}
                </span>
              </div>
            </div>

            {/* Slice canvas */}
            <div className="relative bg-gray-950 rounded-lg border border-gray-800 overflow-hidden flex items-center justify-center"
              style={{ minHeight: '400px' }}
            >
              {!sliceData ? (
                <p className="text-gray-600 text-sm">
                  Run a prediction to view velocity field slices.
                </p>
              ) : (
                <div
                  className="inline-grid gap-0"
                  style={{
                    gridTemplateColumns: `repeat(${sliceData[0]?.length ?? 0}, 4px)`,
                  }}
                >
                  {sliceData.map((row, ri) =>
                    row.map((val, ci) => (
                      <div
                        key={`${ri}-${ci}`}
                        style={{
                          width: 4,
                          height: 4,
                          backgroundColor: velocityToColor(Math.abs(val), sliceMax),
                        }}
                      />
                    )),
                  )}
                </div>
              )}
            </div>

            {/* Color legend */}
            {sliceData && (
              <div className="flex items-center gap-2 mt-3">
                <span className="text-xs text-gray-500">0</span>
                <div className="flex-1 h-3 rounded" style={{
                  background: 'linear-gradient(to right, rgb(0,0,255), rgb(128,0,128), rgb(255,0,0))',
                }} />
                <span className="text-xs text-gray-500">
                  {sliceMax.toExponential(2)}
                </span>
                <span className="text-xs text-gray-600 ml-2">m/s</span>
              </div>
            )}
          </div>
        </div>
      </div>
    </div>
  )
}
