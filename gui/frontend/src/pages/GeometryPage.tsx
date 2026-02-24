import { useState, useCallback } from 'react'
import Viewer3D, { type VoxelData } from '../components/Viewer3D'
import ProgressIndicator from '../components/ProgressIndicator'
import {
  uploadGeometry,
  getGeometryVoxels,
  setBufferZone,
  type GeometryStats,
  type BufferZoneConfig,
} from '../api/client'

export default function GeometryPage() {
  const [stats, setStats] = useState<GeometryStats | null>(null)
  const [voxelData, setVoxelData] = useState<VoxelData | null>(null)
  const [uploading, setUploading] = useState(false)
  const [uploadProgress, setUploadProgress] = useState(-1)
  const [error, setError] = useState<string | null>(null)
  const [bufferConfig, setBufferConfig] = useState<BufferZoneConfig>({
    inlet: 5,
    outlet: 5,
  })
  const [bufferSaved, setBufferSaved] = useState(false)

  const handleFileChange = useCallback(
    async (e: React.ChangeEvent<HTMLInputElement>) => {
      const file = e.target.files?.[0]
      if (!file) return

      setError(null)
      setUploading(true)
      setUploadProgress(-1)
      setStats(null)
      setVoxelData(null)
      setBufferSaved(false)

      try {
        setUploadProgress(30)
        const result = await uploadGeometry(file)
        setStats(result)
        setUploadProgress(70)

        // Fetch voxel data for 3D viewer
        const voxels = await getGeometryVoxels()
        setVoxelData(voxels)
        setUploadProgress(100)
      } catch (err) {
        setError(
          err instanceof Error ? err.message : 'Failed to upload geometry',
        )
      } finally {
        setUploading(false)
      }
    },
    [],
  )

  const handleSaveBuffer = useCallback(async () => {
    setError(null)
    try {
      await setBufferZone(bufferConfig)
      setBufferSaved(true)
    } catch (err) {
      setError(
        err instanceof Error ? err.message : 'Failed to set buffer zone',
      )
    }
  }, [bufferConfig])

  return (
    <div className="space-y-6">
      {/* Page header */}
      <div>
        <h2 className="text-2xl font-bold text-white">Geometry</h2>
        <p className="text-sm text-gray-400 mt-1">
          Upload a voxelized geometry file and configure buffer zones.
        </p>
      </div>

      {error && (
        <div className="p-4 rounded-lg bg-red-900/30 border border-red-800 text-red-300 text-sm">
          {error}
        </div>
      )}

      <div className="grid grid-cols-1 xl:grid-cols-3 gap-6">
        {/* Left column: Upload + Stats + Buffer */}
        <div className="space-y-6">
          {/* Upload card */}
          <div className="card">
            <h3 className="card-header">Upload Geometry</h3>
            <label className="flex flex-col items-center justify-center w-full h-36 border-2 border-dashed border-gray-600 rounded-lg cursor-pointer hover:border-primary-500 hover:bg-gray-800/50 transition-colors">
              <svg
                className="w-10 h-10 text-gray-500 mb-2"
                fill="none"
                viewBox="0 0 24 24"
                stroke="currentColor"
                strokeWidth={1.5}
              >
                <path
                  strokeLinecap="round"
                  strokeLinejoin="round"
                  d="M3 16.5v2.25A2.25 2.25 0 005.25 21h13.5A2.25 2.25 0 0021 18.75V16.5m-13.5-9L12 3m0 0l4.5 4.5M12 3v13.5"
                />
              </svg>
              <span className="text-sm text-gray-400">
                Click to upload .raw / .npy / .stl
              </span>
              <input
                type="file"
                className="hidden"
                accept=".raw,.npy,.stl,.dat"
                onChange={handleFileChange}
                disabled={uploading}
              />
            </label>

            {uploading && (
              <div className="mt-4">
                <ProgressIndicator
                  progress={uploadProgress}
                  label="Uploading..."
                  status="Processing geometry file"
                />
              </div>
            )}
          </div>

          {/* Stats card */}
          {stats && (
            <div className="card">
              <h3 className="card-header">Geometry Statistics</h3>
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <p className="stat-label">Filename</p>
                  <p className="text-sm text-gray-200 font-mono truncate">
                    {stats.filename}
                  </p>
                </div>
                <div>
                  <p className="stat-label">File Size</p>
                  <p className="text-sm text-gray-200">
                    {(stats.fileSize / (1024 * 1024)).toFixed(2)} MB
                  </p>
                </div>
                <div>
                  <p className="stat-label">Dimensions</p>
                  <p className="stat-value text-lg">
                    {stats.dimensions.join(' x ')}
                  </p>
                </div>
                <div>
                  <p className="stat-label">Total Voxels</p>
                  <p className="stat-value text-lg">
                    {stats.voxelCount.toLocaleString()}
                  </p>
                </div>
                <div>
                  <p className="stat-label">Porosity</p>
                  <p className="stat-value text-lg">
                    {(stats.porosity * 100).toFixed(2)}%
                  </p>
                </div>
                <div>
                  <p className="stat-label">Voxel Size</p>
                  <p className="text-sm text-gray-200">
                    {stats.voxelSize} m
                  </p>
                </div>
              </div>
            </div>
          )}

          {/* Buffer zone config */}
          {stats && (
            <div className="card">
              <h3 className="card-header">Buffer Zone Configuration</h3>
              <div className="space-y-4">
                <div>
                  <label className="label">Inlet Buffer (voxels)</label>
                  <input
                    type="number"
                    min={0}
                    max={50}
                    className="input-field"
                    value={bufferConfig.inlet}
                    onChange={(e) =>
                      setBufferConfig((c) => ({
                        ...c,
                        inlet: parseInt(e.target.value) || 0,
                      }))
                    }
                  />
                </div>
                <div>
                  <label className="label">Outlet Buffer (voxels)</label>
                  <input
                    type="number"
                    min={0}
                    max={50}
                    className="input-field"
                    value={bufferConfig.outlet}
                    onChange={(e) =>
                      setBufferConfig((c) => ({
                        ...c,
                        outlet: parseInt(e.target.value) || 0,
                      }))
                    }
                  />
                </div>
                <button onClick={handleSaveBuffer} className="btn-primary w-full">
                  {bufferSaved ? 'Saved' : 'Save Buffer Configuration'}
                </button>
                {bufferSaved && (
                  <p className="text-xs text-green-400">
                    Buffer zone configuration saved successfully.
                  </p>
                )}
              </div>
            </div>
          )}
        </div>

        {/* Right column: 3D Viewer */}
        <div className="xl:col-span-2">
          <div className="card p-0 overflow-hidden" style={{ height: '600px' }}>
            <Viewer3D voxelData={voxelData} />
          </div>
        </div>
      </div>
    </div>
  )
}
