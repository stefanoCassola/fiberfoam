import { Routes, Route } from 'react-router-dom'
import Sidebar from './components/Sidebar'
import GeometryPage from './pages/GeometryPage'
import PredictionPage from './pages/PredictionPage'
import MeshPage from './pages/MeshPage'
import SimulationPage from './pages/SimulationPage'
import PostProcessPage from './pages/PostProcessPage'

export default function App() {
  return (
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
            <Route path="/" element={<GeometryPage />} />
            <Route path="/prediction" element={<PredictionPage />} />
            <Route path="/mesh" element={<MeshPage />} />
            <Route path="/simulation" element={<SimulationPage />} />
            <Route path="/postprocess" element={<PostProcessPage />} />
          </Routes>
        </div>
      </main>
    </div>
  )
}
