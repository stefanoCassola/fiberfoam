import { useState, useCallback, useEffect, useRef } from 'react'
import {
  LineChart,
  Line,
  XAxis,
  YAxis,
  CartesianGrid,
  Tooltip,
  Legend,
  ResponsiveContainer,
} from 'recharts'
import LogViewer from '../components/LogViewer'
import ProgressIndicator from '../components/ProgressIndicator'
import { useWebSocket } from '../hooks/useWebSocket'
import {
  startSimulation,
  stopSimulation,
  getSimulationStatus,
  type SolverConfig,
  type SimulationStatus,
  type ResidualPoint,
} from '../api/client'

type Direction = 'x' | 'y' | 'z'

export default function SimulationPage() {
  const [config, setConfig] = useState<SolverConfig>({
    solver: 'simpleFoam',
    direction: 'x',
    nIterations: 1000,
    tolerance: 1e-6,
    relaxationFactor: 0.7,
    writeInterval: 100,
  })
  const [simStatus, setSimStatus] = useState<SimulationStatus | null>(null)
  const [residuals, setResiduals] = useState<ResidualPoint[]>([])
  const [running, setRunning] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const pollingRef = useRef<ReturnType<typeof setInterval> | null>(null)

  const { lines, connected, connect, disconnect, clear } = useWebSocket({
    url: '/ws/logs',
    autoConnect: false,
  })

  // Poll simulation status while running
  useEffect(() => {
    if (!running) {
      if (pollingRef.current) {
        clearInterval(pollingRef.current)
        pollingRef.current = null
      }
      return
    }

    pollingRef.current = setInterval(async () => {
      try {
        const status = await getSimulationStatus()
        setSimStatus(status)
        setResiduals(status.residuals)

        if (status.status === 'completed' || status.status === 'error') {
          setRunning(false)
          if (status.status === 'error') {
            setError('Simulation ended with an error. Check the log.')
          }
        }
      } catch {
        // Ignore intermittent polling errors
      }
    }, 2000)

    return () => {
      if (pollingRef.current) clearInterval(pollingRef.current)
    }
  }, [running])

  const handleStart = useCallback(async () => {
    setError(null)
    setRunning(true)
    setResiduals([])
    clear()

    try {
      connect()
      await startSimulation(config)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to start simulation')
      setRunning(false)
      disconnect()
    }
  }, [config, connect, disconnect, clear])

  const handleStop = useCallback(async () => {
    try {
      await stopSimulation()
    } catch {
      // Ignore stop errors
    }
    setRunning(false)
    disconnect()
  }, [disconnect])

  const progress = simStatus
    ? simStatus.totalIterations > 0
      ? (simStatus.currentIteration / simStatus.totalIterations) * 100
      : -1
    : 0

  return (
    <div className="space-y-6">
      <div>
        <h2 className="text-2xl font-bold text-white">Simulation</h2>
        <p className="text-sm text-gray-400 mt-1">
          Configure and run the OpenFOAM CFD simulation.
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
            <h3 className="card-header">Solver Configuration</h3>
            <div className="space-y-4">
              <div>
                <label className="label">Solver</label>
                <select
                  className="select-field"
                  value={config.solver}
                  onChange={(e) =>
                    setConfig((c) => ({ ...c, solver: e.target.value }))
                  }
                  disabled={running}
                >
                  <option value="simpleFoam">simpleFoam</option>
                  <option value="pisoFoam">pisoFoam</option>
                  <option value="pimpleFoam">pimpleFoam</option>
                </select>
              </div>

              <div>
                <label className="label">Flow Direction</label>
                <div className="grid grid-cols-3 gap-2">
                  {(['x', 'y', 'z'] as const).map((d: Direction) => (
                    <button
                      key={d}
                      onClick={() => setConfig((c) => ({ ...c, direction: d }))}
                      disabled={running}
                      className={`py-2 rounded-lg text-sm font-bold transition-colors ${
                        config.direction === d
                          ? 'bg-primary-600 text-white'
                          : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
                      }`}
                    >
                      {d.toUpperCase()}
                    </button>
                  ))}
                </div>
              </div>

              <div>
                <label className="label">Number of Iterations</label>
                <input
                  type="number"
                  min={1}
                  max={100000}
                  className="input-field"
                  value={config.nIterations}
                  onChange={(e) =>
                    setConfig((c) => ({
                      ...c,
                      nIterations: parseInt(e.target.value) || 1000,
                    }))
                  }
                  disabled={running}
                />
              </div>

              <div>
                <label className="label">Tolerance</label>
                <input
                  type="number"
                  step="1e-7"
                  min={0}
                  className="input-field"
                  value={config.tolerance}
                  onChange={(e) =>
                    setConfig((c) => ({
                      ...c,
                      tolerance: parseFloat(e.target.value) || 1e-6,
                    }))
                  }
                  disabled={running}
                />
              </div>

              <div>
                <label className="label">Relaxation Factor</label>
                <input
                  type="number"
                  step="0.05"
                  min={0.01}
                  max={1}
                  className="input-field"
                  value={config.relaxationFactor}
                  onChange={(e) =>
                    setConfig((c) => ({
                      ...c,
                      relaxationFactor: parseFloat(e.target.value) || 0.7,
                    }))
                  }
                  disabled={running}
                />
              </div>

              <div>
                <label className="label">Write Interval</label>
                <input
                  type="number"
                  min={1}
                  className="input-field"
                  value={config.writeInterval}
                  onChange={(e) =>
                    setConfig((c) => ({
                      ...c,
                      writeInterval: parseInt(e.target.value) || 100,
                    }))
                  }
                  disabled={running}
                />
              </div>

              <div className="flex gap-3">
                <button
                  onClick={handleStart}
                  disabled={running}
                  className="btn-primary flex-1"
                >
                  {running ? 'Running...' : 'Start Simulation'}
                </button>
                {running && (
                  <button onClick={handleStop} className="btn-danger flex-1">
                    Stop
                  </button>
                )}
              </div>

              {running && (
                <ProgressIndicator
                  progress={progress}
                  label={`Iteration ${simStatus?.currentIteration ?? 0} / ${simStatus?.totalIterations ?? config.nIterations}`}
                  status={
                    simStatus
                      ? `Elapsed: ${simStatus.elapsedTime.toFixed(1)}s`
                      : 'Starting...'
                  }
                />
              )}
            </div>
          </div>
        </div>

        {/* Right column: Residual plot + Log */}
        <div className="xl:col-span-2 space-y-6">
          {/* Residual plot */}
          <div className="card">
            <h3 className="card-header">Residuals</h3>
            <div style={{ width: '100%', height: 300 }}>
              {residuals.length === 0 ? (
                <div className="flex items-center justify-center h-full text-gray-600 text-sm">
                  No residual data yet. Start a simulation.
                </div>
              ) : (
                <ResponsiveContainer>
                  <LineChart data={residuals}>
                    <CartesianGrid strokeDasharray="3 3" stroke="#374151" />
                    <XAxis
                      dataKey="iteration"
                      stroke="#6b7280"
                      tick={{ fontSize: 11 }}
                    />
                    <YAxis
                      scale="log"
                      domain={['auto', 'auto']}
                      stroke="#6b7280"
                      tick={{ fontSize: 11 }}
                      tickFormatter={(v: number) => v.toExponential(0)}
                    />
                    <Tooltip
                      contentStyle={{
                        backgroundColor: '#1f2937',
                        border: '1px solid #374151',
                        borderRadius: '8px',
                        fontSize: 12,
                      }}
                    />
                    <Legend wrapperStyle={{ fontSize: 12 }} />
                    <Line
                      type="monotone"
                      dataKey="Ux"
                      stroke="#3b82f6"
                      dot={false}
                      strokeWidth={1.5}
                    />
                    <Line
                      type="monotone"
                      dataKey="Uy"
                      stroke="#10b981"
                      dot={false}
                      strokeWidth={1.5}
                    />
                    <Line
                      type="monotone"
                      dataKey="Uz"
                      stroke="#f59e0b"
                      dot={false}
                      strokeWidth={1.5}
                    />
                    <Line
                      type="monotone"
                      dataKey="p"
                      stroke="#ef4444"
                      dot={false}
                      strokeWidth={1.5}
                    />
                  </LineChart>
                </ResponsiveContainer>
              )}
            </div>
          </div>

          {/* Live log viewer */}
          <div className="card p-0">
            <LogViewer
              lines={lines}
              connected={connected}
              onClear={clear}
              maxHeight="350px"
            />
          </div>
        </div>
      </div>
    </div>
  )
}
