import { useState, useEffect, useCallback } from 'react'
import {
  getPipelineStatus,
  downloadResults,
  getResultsCsv,
  type PipelineStatus,
} from '../api/client'

interface HistoryEntry {
  pipelineId: string
  date: string
  geometry: string
  mode: string
  status: string
  progress: number
  permeability: string | null
}

const STATUS_STYLES: Record<string, string> = {
  completed: 'bg-green-900/40 text-green-400 border-green-800',
  running: 'bg-blue-900/40 text-blue-400 border-blue-800',
  queued: 'bg-yellow-900/40 text-yellow-400 border-yellow-800',
  error: 'bg-red-900/40 text-red-400 border-red-800',
  cancelled: 'bg-gray-800 text-gray-400 border-gray-700',
}

function statusBadge(status: string) {
  const style = STATUS_STYLES[status] ?? STATUS_STYLES.cancelled
  return (
    <span className={`inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium border ${style}`}>
      {status.charAt(0).toUpperCase() + status.slice(1)}
    </span>
  )
}

export default function JobHistoryPage() {
  const [entries, setEntries] = useState<HistoryEntry[]>([])
  const [loading, setLoading] = useState(true)
  const [error, setError] = useState<string | null>(null)

  const loadHistory = useCallback(async () => {
    setLoading(true)
    setError(null)
    try {
      // Load pipeline IDs from localStorage
      const stored = localStorage.getItem('fiberfoam_pipelines')
      const pipelineIds: string[] = stored ? JSON.parse(stored) : []

      if (pipelineIds.length === 0) {
        setEntries([])
        setLoading(false)
        return
      }

      const results = await Promise.allSettled(
        pipelineIds.map((id) => getPipelineStatus(id)),
      )

      const history: HistoryEntry[] = []
      for (let i = 0; i < results.length; i++) {
        const result = results[i]
        if (result.status === 'fulfilled') {
          const ps: PipelineStatus = result.value
          history.push({
            pipelineId: ps.pipelineId,
            date: new Date().toISOString(),
            geometry: ps.steps?.[0]?.name ?? 'Unknown',
            mode: ps.steps?.length > 2 ? 'full' : ps.steps?.length > 1 ? 'mesh_predict' : 'mesh_only',
            status: ps.status,
            progress: ps.progress,
            permeability: null,
          })
        }
      }

      history.sort((a, b) => b.date.localeCompare(a.date))
      setEntries(history)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to load job history')
    } finally {
      setLoading(false)
    }
  }, [])

  useEffect(() => {
    loadHistory()
  }, [loadHistory])

  const handleDownload = useCallback(async (pipelineId: string) => {
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
      setError('Failed to download results')
    }
  }, [])

  const handleExportCsv = useCallback(async (pipelineId: string) => {
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
      setError('Failed to export CSV')
    }
  }, [])

  const handleDelete = useCallback((pipelineId: string) => {
    const stored = localStorage.getItem('fiberfoam_pipelines')
    const pipelineIds: string[] = stored ? JSON.parse(stored) : []
    const updated = pipelineIds.filter((id) => id !== pipelineId)
    localStorage.setItem('fiberfoam_pipelines', JSON.stringify(updated))
    setEntries((prev) => prev.filter((e) => e.pipelineId !== pipelineId))
  }, [])

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold text-white">Job History</h2>
          <p className="text-sm text-gray-400 mt-1">
            View and manage previous pipeline runs and their results.
          </p>
        </div>
        <button onClick={loadHistory} className="btn-secondary flex items-center gap-2">
          <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M16.023 9.348h4.992v-.001M2.985 19.644v-4.992m0 0h4.992m-4.993 0l3.181 3.183a8.25 8.25 0 0013.803-3.7M4.031 9.865a8.25 8.25 0 0113.803-3.7l3.181 3.182" />
          </svg>
          Refresh
        </button>
      </div>

      {error && (
        <div className="p-4 rounded-lg bg-red-900/30 border border-red-800 text-red-300 text-sm">
          {error}
        </div>
      )}

      <div className="card">
        <h3 className="card-header">Pipeline Runs</h3>

        {loading ? (
          <div className="flex items-center justify-center py-12">
            <svg className="animate-spin h-6 w-6 text-primary-500" fill="none" viewBox="0 0 24 24">
              <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
              <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z" />
            </svg>
            <span className="ml-3 text-sm text-gray-400">Loading history...</span>
          </div>
        ) : entries.length === 0 ? (
          <div className="flex flex-col items-center justify-center py-16 text-center">
            <svg className="w-16 h-16 text-gray-700 mb-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M19.5 14.25v-2.625a3.375 3.375 0 00-3.375-3.375h-1.5A1.125 1.125 0 0113.5 7.125v-1.5a3.375 3.375 0 00-3.375-3.375H8.25m0 12.75h7.5m-7.5 3H12M10.5 2.25H5.625c-.621 0-1.125.504-1.125 1.125v17.25c0 .621.504 1.125 1.125 1.125h12.75c.621 0 1.125-.504 1.125-1.125V11.25a9 9 0 00-9-9z" />
            </svg>
            <p className="text-gray-400 text-sm font-medium">No pipeline runs yet</p>
            <p className="text-gray-600 text-xs mt-1">
              Run a pipeline from the Pipeline or Batch page to see results here.
            </p>
          </div>
        ) : (
          <div className="overflow-x-auto">
            <table className="w-full text-sm">
              <thead>
                <tr className="border-b border-gray-800">
                  <th className="text-left py-3 px-4 text-xs font-medium text-gray-500 uppercase tracking-wider">Pipeline ID</th>
                  <th className="text-left py-3 px-4 text-xs font-medium text-gray-500 uppercase tracking-wider">Mode</th>
                  <th className="text-left py-3 px-4 text-xs font-medium text-gray-500 uppercase tracking-wider">Status</th>
                  <th className="text-left py-3 px-4 text-xs font-medium text-gray-500 uppercase tracking-wider">Progress</th>
                  <th className="text-right py-3 px-4 text-xs font-medium text-gray-500 uppercase tracking-wider">Actions</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-800">
                {entries.map((entry) => (
                  <tr key={entry.pipelineId} className="hover:bg-gray-800/50 transition-colors">
                    <td className="py-3 px-4">
                      <span className="font-mono text-gray-300 text-xs">
                        {entry.pipelineId.slice(0, 12)}...
                      </span>
                    </td>
                    <td className="py-3 px-4">
                      <span className="text-gray-300">{entry.mode.replace('_', ' ')}</span>
                    </td>
                    <td className="py-3 px-4">{statusBadge(entry.status)}</td>
                    <td className="py-3 px-4">
                      <div className="flex items-center gap-2">
                        <div className="flex-1 h-1.5 bg-gray-800 rounded-full overflow-hidden">
                          <div
                            className="h-full bg-primary-600 rounded-full transition-all"
                            style={{ width: `${Math.max(0, Math.min(100, entry.progress))}%` }}
                          />
                        </div>
                        <span className="text-xs text-gray-500 w-8 text-right">
                          {Math.round(entry.progress)}%
                        </span>
                      </div>
                    </td>
                    <td className="py-3 px-4">
                      <div className="flex items-center justify-end gap-1">
                        {entry.status === 'completed' && (
                          <>
                            <button
                              onClick={() => handleDownload(entry.pipelineId)}
                              className="p-1.5 rounded-lg text-gray-400 hover:text-primary-400 hover:bg-gray-800 transition-colors"
                              title="Download results"
                            >
                              <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                                <path strokeLinecap="round" strokeLinejoin="round" d="M3 16.5v2.25A2.25 2.25 0 005.25 21h13.5A2.25 2.25 0 0021 18.75V16.5M16.5 12L12 16.5m0 0L7.5 12m4.5 4.5V3" />
                              </svg>
                            </button>
                            <button
                              onClick={() => handleExportCsv(entry.pipelineId)}
                              className="p-1.5 rounded-lg text-gray-400 hover:text-primary-400 hover:bg-gray-800 transition-colors"
                              title="Export CSV"
                            >
                              <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                                <path strokeLinecap="round" strokeLinejoin="round" d="M19.5 14.25v-2.625a3.375 3.375 0 00-3.375-3.375h-1.5A1.125 1.125 0 0113.5 7.125v-1.5a3.375 3.375 0 00-3.375-3.375H8.25m2.25 0H5.625c-.621 0-1.125.504-1.125 1.125v17.25c0 .621.504 1.125 1.125 1.125h12.75c.621 0 1.125-.504 1.125-1.125V11.25a9 9 0 00-9-9z" />
                              </svg>
                            </button>
                          </>
                        )}
                        <button
                          onClick={() => handleDelete(entry.pipelineId)}
                          className="p-1.5 rounded-lg text-gray-400 hover:text-red-400 hover:bg-gray-800 transition-colors"
                          title="Remove from history"
                        >
                          <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                            <path strokeLinecap="round" strokeLinejoin="round" d="M14.74 9l-.346 9m-4.788 0L9.26 9m9.968-3.21c.342.052.682.107 1.022.166m-1.022-.165L18.16 19.673a2.25 2.25 0 01-2.244 2.077H8.084a2.25 2.25 0 01-2.244-2.077L4.772 5.79m14.456 0a48.108 48.108 0 00-3.478-.397m-12 .562c.34-.059.68-.114 1.022-.165m0 0a48.11 48.11 0 013.478-.397m7.5 0v-.916c0-1.18-.91-2.164-2.09-2.201a51.964 51.964 0 00-3.32 0c-1.18.037-2.09 1.022-2.09 2.201v.916m7.5 0a48.667 48.667 0 00-7.5 0" />
                          </svg>
                        </button>
                      </div>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        )}
      </div>
    </div>
  )
}
