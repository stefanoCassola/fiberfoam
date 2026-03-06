import { createContext, useContext, useState, useCallback, type ReactNode } from 'react'
import type { GeometryStats } from '../api/client'

export interface WorkflowState {
  geometryPath: string | null
  geometryStats: GeometryStats | null
  caseDir: string | null
  pipelineId: string | null
  currentStep: string | null
  setGeometryPath: (path: string | null) => void
  setGeometryStats: (stats: GeometryStats | null) => void
  setCaseDir: (dir: string | null) => void
  setPipelineId: (id: string | null) => void
  setCurrentStep: (step: string | null) => void
}

export const WorkflowContext = createContext<WorkflowState | null>(null)

const ACTIVE_PIPELINE_KEY = 'fiberfoam_active_pipeline'

export function WorkflowProvider({ children }: { children: ReactNode }) {
  const [geometryPath, setGeometryPath] = useState<string | null>(null)
  const [geometryStats, setGeometryStats] = useState<GeometryStats | null>(null)
  const [caseDir, setCaseDir] = useState<string | null>(null)
  const [pipelineId, _setPipelineId] = useState<string | null>(
    () => localStorage.getItem(ACTIVE_PIPELINE_KEY),
  )
  const [currentStep, setCurrentStep] = useState<string | null>(null)

  const setPipelineId = useCallback((id: string | null) => {
    _setPipelineId(id)
    if (id) {
      localStorage.setItem(ACTIVE_PIPELINE_KEY, id)
    } else {
      localStorage.removeItem(ACTIVE_PIPELINE_KEY)
    }
  }, [])

  return (
    <WorkflowContext.Provider
      value={{
        geometryPath,
        geometryStats,
        caseDir,
        pipelineId,
        currentStep,
        setGeometryPath,
        setGeometryStats,
        setCaseDir,
        setPipelineId,
        setCurrentStep,
      }}
    >
      {children}
    </WorkflowContext.Provider>
  )
}

export function useWorkflow(): WorkflowState {
  const ctx = useContext(WorkflowContext)
  if (!ctx) {
    throw new Error('useWorkflow must be used within a WorkflowProvider')
  }
  return ctx
}
