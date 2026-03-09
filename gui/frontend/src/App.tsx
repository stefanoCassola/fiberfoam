import { useState, useEffect, useCallback } from 'react'
import { Routes, Route } from 'react-router-dom'
import { WorkflowProvider } from './context/WorkflowContext'
import Sidebar from './components/Sidebar'
import PipelinePage from './pages/PipelinePage'
import BatchPage from './pages/BatchPage'
import JobHistoryPage from './pages/JobHistoryPage'
import SetupPage from './pages/SetupPage'
import { getBackendUrl } from './api/client'

function isHostedMode(): boolean {
  // Running on Vercel or any non-localhost host
  const h = window.location.hostname
  return h !== 'localhost' && h !== '127.0.0.1'
}

export default function App() {
  const [connected, setConnected] = useState<boolean | null>(null)

  const checkBackend = useCallback(async () => {
    try {
      const url = getBackendUrl().replace(/\/api$/, '/api/health')
      const res = await fetch(url, { signal: AbortSignal.timeout(3000) })
      if (res.ok) { setConnected(true); return }
    } catch { /* ignore */ }
    setConnected(false)
  }, [])

  useEffect(() => {
    if (!isHostedMode()) {
      // Local mode (Docker serves frontend) — skip setup
      setConnected(true)
      return
    }
    checkBackend()
  }, [checkBackend])

  // Show nothing while initial check runs
  if (connected === null) {
    return (
      <div className="min-h-screen bg-gray-950 flex items-center justify-center">
        <div className="w-10 h-10 border-4 border-primary-500 border-t-transparent rounded-full animate-spin" />
      </div>
    )
  }

  // Hosted mode + not connected → setup guide
  if (!connected && isHostedMode()) {
    return <SetupPage onConnected={() => setConnected(true)} />
  }

  return (
    <WorkflowProvider>
      <div className="flex min-h-screen bg-gray-950">
        <Sidebar />

        {/* Main content area offset by sidebar width */}
        <main className="flex-1 ml-60">
          {/* Top bar */}
          <header className="sticky top-0 z-20 flex h-16 items-center justify-between border-b border-gray-800 bg-gray-950/80 backdrop-blur-md px-8">
            <div />
            <div className="flex items-center gap-4">
              <span className="text-xs text-gray-500">
                FiberFoam Simulation Suite
              </span>
              <div className="h-8 w-8 rounded-full bg-primary-600/30 flex items-center justify-center text-primary-400 text-xs font-bold">
                FF
              </div>
            </div>
          </header>

          {/* Page content */}
          <div className="p-8">
            <Routes>
              <Route path="/" element={<PipelinePage />} />
              <Route path="/batch" element={<BatchPage />} />
              <Route path="/history" element={<JobHistoryPage />} />
            </Routes>
          </div>
        </main>
      </div>
    </WorkflowProvider>
  )
}
