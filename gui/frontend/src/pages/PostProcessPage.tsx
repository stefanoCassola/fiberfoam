import { useState, useCallback } from 'react'
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  ResponsiveContainer,
} from 'recharts'
import PermeabilityTable from '../components/PermeabilityTable'
import ProgressIndicator from '../components/ProgressIndicator'
import {
  runPostProcessing,
  getPermeabilityResults,
  exportCsv,
  type PermeabilityResult,
  type ConvergencePoint,
} from '../api/client'

export default function PostProcessPage() {
  const [results, setResults] = useState<PermeabilityResult[]>([])
  const [convergence, setConvergence] = useState<ConvergencePoint[]>([])
  const [processing, setProcessing] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [selectedDir, setSelectedDir] = useState<string>('all')

  const handleRunPostProcess = useCallback(async () => {
    setError(null)
    setProcessing(true)

    try {
      if (selectedDir === 'all') {
        // Run for all 3 directions
        const dirs = ['x', 'y', 'z']
        const allConvergence: ConvergencePoint[] = []

        for (const d of dirs) {
          const res = await runPostProcessing(d)
          allConvergence.push(...res.convergencePlot)
        }

        const perms = await getPermeabilityResults()
        setResults(perms)
        setConvergence(allConvergence)
      } else {
        const res = await runPostProcessing(selectedDir)
        const perms = await getPermeabilityResults()
        setResults(perms)
        setConvergence(res.convergencePlot)
      }
    } catch (err) {
      setError(
        err instanceof Error ? err.message : 'Post-processing failed',
      )
    } finally {
      setProcessing(false)
    }
  }, [selectedDir])

  const handleExportCsv = useCallback(async () => {
    try {
      const blob = await exportCsv()
      const url = URL.createObjectURL(blob)
      const a = document.createElement('a')
      a.href = url
      a.download = 'permeability_results.csv'
      document.body.appendChild(a)
      a.click()
      document.body.removeChild(a)
      URL.revokeObjectURL(url)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'CSV export failed')
    }
  }, [])

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold text-white">Post-Processing</h2>
          <p className="text-sm text-gray-400 mt-1">
            Compute permeability and analyze simulation convergence.
          </p>
        </div>
        {results.length > 0 && (
          <button onClick={handleExportCsv} className="btn-secondary flex items-center gap-2">
            <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M3 16.5v2.25A2.25 2.25 0 005.25 21h13.5A2.25 2.25 0 0021 18.75V16.5M16.5 12L12 16.5m0 0L7.5 12m4.5 4.5V3" />
            </svg>
            Export CSV
          </button>
        )}
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
            <h3 className="card-header">Post-Processing Controls</h3>
            <div className="space-y-4">
              <div>
                <label className="label">Direction</label>
                <select
                  className="select-field"
                  value={selectedDir}
                  onChange={(e) => setSelectedDir(e.target.value)}
                  disabled={processing}
                >
                  <option value="all">All directions (X, Y, Z)</option>
                  <option value="x">X only</option>
                  <option value="y">Y only</option>
                  <option value="z">Z only</option>
                </select>
              </div>

              <button
                onClick={handleRunPostProcess}
                disabled={processing}
                className="btn-primary w-full"
              >
                {processing ? 'Processing...' : 'Run Post-Processing'}
              </button>

              {processing && (
                <ProgressIndicator
                  progress={-1}
                  label="Computing permeability"
                  status="Analyzing flow fields..."
                />
              )}
            </div>
          </div>

          {/* Quick summary */}
          {results.length > 0 && (
            <div className="card">
              <h3 className="card-header">Summary</h3>
              <div className="space-y-3">
                {results.map((r) => (
                  <div
                    key={r.direction}
                    className="flex items-center justify-between py-2 border-b border-gray-800 last:border-0"
                  >
                    <span className="flex items-center gap-2">
                      <span className="inline-flex items-center justify-center w-7 h-7 rounded-md bg-primary-600/20 text-primary-400 font-bold text-xs">
                        {r.direction.toUpperCase()}
                      </span>
                      <span className="text-sm text-gray-400">Darcy</span>
                    </span>
                    <span className="font-mono text-sm text-gray-200">
                      {r.darcyMethod.toExponential(3)}
                    </span>
                  </div>
                ))}
              </div>
            </div>
          )}
        </div>

        {/* Right column: Table + Convergence */}
        <div className="xl:col-span-2 space-y-6">
          {/* Permeability table */}
          <PermeabilityTable results={results} loading={processing} />

          {/* Convergence plot */}
          <div className="card">
            <h3 className="card-header">Convergence Plot</h3>
            <div style={{ width: '100%', height: 300 }}>
              {convergence.length === 0 ? (
                <div className="flex items-center justify-center h-full text-gray-600 text-sm">
                  No convergence data yet.
                </div>
              ) : (
                <ResponsiveContainer>
                  <LineChart data={convergence}>
                    <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
                    <XAxis
                      dataKey="iteration"
                      stroke="#6b7280"
                      tick={{ fontSize: 11 }}
                      label={{
                        value: 'Iteration',
                        position: 'insideBottom',
                        offset: -5,
                        style: { fill: '#6b7280', fontSize: 12 },
                      }}
                    />
                    <YAxis
                      scale="log"
                      domain={['auto', 'auto']}
                      stroke="#6b7280"
                      tick={{ fontSize: 11 }}
                      tickFormatter={(v: number) => v.toExponential(0)}
                      label={{
                        value: 'Residual',
                        angle: -90,
                        position: 'insideLeft',
                        style: { fill: '#6b7280', fontSize: 12 },
                      }}
                    />
                    <Tooltip
                      contentStyle={{
                        backgroundColor: '#1f2937',
                        border: '1px solid #374151',
                        borderRadius: '8px',
                        fontSize: 12,
                      }}
                    />
                    <Line
                      type="monotone"
                      dataKey="residual"
                      stroke="#3b82f6"
                      dot={false}
                      strokeWidth={2}
                      name="Residual"
                    />
                  </LineChart>
                </ResponsiveContainer>
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  )
}
