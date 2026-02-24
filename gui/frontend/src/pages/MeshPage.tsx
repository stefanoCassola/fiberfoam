import { useState, useCallback } from 'react'
import Viewer3D, { type MeshData } from '../components/Viewer3D'
import ProgressIndicator from '../components/ProgressIndicator'
import {
  generateMesh,
  getMeshStats,
  getMeshPreview,
  type MeshConfig,
  type MeshStats,
  type JobStatus,
  getJobStatus,
} from '../api/client'

export default function MeshPage() {
  const [config, setConfig] = useState<MeshConfig>({
    refinementLevel: 2,
    bufferZone: { inlet: 5, outlet: 5 },
    snapControls: true,
  })
  const [generating, setGenerating] = useState(false)
  const [progress, setProgress] = useState(0)
  const [progressMsg, setProgressMsg] = useState('')
  const [stats, setStats] = useState<MeshStats | null>(null)
  const [meshData, setMeshData] = useState<MeshData | null>(null)
  const [error, setError] = useState<string | null>(null)

  const pollJob = useCallback(async (jobId: string) => {
    const poll = async (): Promise<void> => {
      const status: JobStatus = await getJobStatus(jobId)
      setProgress(status.progress)
      setProgressMsg(status.message)

      if (status.status === 'completed') {
        const [meshStats, meshPreview] = await Promise.all([
          getMeshStats(),
          getMeshPreview(),
        ])
        setStats(meshStats)
        setMeshData(meshPreview)
        setGenerating(false)
        return
      }
      if (status.status === 'error') {
        setError(status.message || 'Mesh generation failed')
        setGenerating(false)
        return
      }
      // Still running -- poll again
      await new Promise((r) => setTimeout(r, 2000))
      return poll()
    }
    return poll()
  }, [])

  const handleGenerate = useCallback(async () => {
    setError(null)
    setGenerating(true)
    setProgress(0)
    setStats(null)
    setMeshData(null)

    try {
      const job = await generateMesh(config)
      await pollJob(job.jobId)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Mesh generation failed')
      setGenerating(false)
    }
  }, [config, pollJob])

  const qualityColor = (quality: string) => {
    switch (quality.toLowerCase()) {
      case 'good':
        return 'text-green-400'
      case 'acceptable':
        return 'text-yellow-400'
      case 'poor':
        return 'text-red-400'
      default:
        return 'text-gray-400'
    }
  }

  return (
    <div className="space-y-6">
      <div>
        <h2 className="text-2xl font-bold text-white">Mesh Generation</h2>
        <p className="text-sm text-gray-400 mt-1">
          Generate an OpenFOAM mesh from the voxelized geometry.
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
            <h3 className="card-header">Mesh Settings</h3>
            <div className="space-y-4">
              <div>
                <label className="label">Refinement Level</label>
                <select
                  className="select-field"
                  value={config.refinementLevel}
                  onChange={(e) =>
                    setConfig((c) => ({
                      ...c,
                      refinementLevel: parseInt(e.target.value),
                    }))
                  }
                  disabled={generating}
                >
                  {[0, 1, 2, 3, 4].map((l) => (
                    <option key={l} value={l}>
                      Level {l}
                    </option>
                  ))}
                </select>
              </div>

              <div>
                <label className="label">Inlet Buffer (voxels)</label>
                <input
                  type="number"
                  min={0}
                  max={50}
                  className="input-field"
                  value={config.bufferZone.inlet}
                  onChange={(e) =>
                    setConfig((c) => ({
                      ...c,
                      bufferZone: {
                        ...c.bufferZone,
                        inlet: parseInt(e.target.value) || 0,
                      },
                    }))
                  }
                  disabled={generating}
                />
              </div>

              <div>
                <label className="label">Outlet Buffer (voxels)</label>
                <input
                  type="number"
                  min={0}
                  max={50}
                  className="input-field"
                  value={config.bufferZone.outlet}
                  onChange={(e) =>
                    setConfig((c) => ({
                      ...c,
                      bufferZone: {
                        ...c.bufferZone,
                        outlet: parseInt(e.target.value) || 0,
                      },
                    }))
                  }
                  disabled={generating}
                />
              </div>

              <div className="flex items-center gap-3">
                <input
                  type="checkbox"
                  id="snap"
                  className="h-4 w-4 rounded border-gray-600 bg-gray-800 text-primary-500 focus:ring-primary-500"
                  checked={config.snapControls}
                  onChange={(e) =>
                    setConfig((c) => ({ ...c, snapControls: e.target.checked }))
                  }
                  disabled={generating}
                />
                <label htmlFor="snap" className="text-sm text-gray-300">
                  Enable snap controls
                </label>
              </div>

              <button
                onClick={handleGenerate}
                disabled={generating}
                className="btn-primary w-full"
              >
                {generating ? 'Generating...' : 'Generate Mesh'}
              </button>

              {generating && (
                <ProgressIndicator
                  progress={progress}
                  label="Mesh Generation"
                  status={progressMsg}
                />
              )}
            </div>
          </div>

          {/* Mesh statistics */}
          {stats && (
            <div className="card">
              <h3 className="card-header">Mesh Statistics</h3>
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <p className="stat-label">Cells</p>
                  <p className="stat-value text-lg">
                    {stats.cellCount.toLocaleString()}
                  </p>
                </div>
                <div>
                  <p className="stat-label">Points</p>
                  <p className="stat-value text-lg">
                    {stats.pointCount.toLocaleString()}
                  </p>
                </div>
                <div>
                  <p className="stat-label">Faces</p>
                  <p className="text-sm text-gray-200">
                    {stats.faceCount.toLocaleString()}
                  </p>
                </div>
                <div>
                  <p className="stat-label">Quality</p>
                  <p className={`text-sm font-bold ${qualityColor(stats.quality)}`}>
                    {stats.quality}
                  </p>
                </div>
                <div>
                  <p className="stat-label">Max Non-Orthogonality</p>
                  <p className="text-sm text-gray-200">
                    {stats.maxNonOrthogonality.toFixed(2)}
                  </p>
                </div>
                <div>
                  <p className="stat-label">Max Skewness</p>
                  <p className="text-sm text-gray-200">
                    {stats.maxSkewness.toFixed(4)}
                  </p>
                </div>
              </div>
            </div>
          )}
        </div>

        {/* 3D Viewer */}
        <div className="xl:col-span-2">
          <div className="card p-0 overflow-hidden" style={{ height: '600px' }}>
            <Viewer3D meshData={meshData} showWireframe />
          </div>
        </div>
      </div>
    </div>
  )
}
