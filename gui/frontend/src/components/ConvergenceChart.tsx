import { useState } from 'react'

interface DataPoint {
  iteration: number
  [field: string]: number
}

const DEFAULT_VISIBLE = 50

interface ConvergenceChartProps {
  residuals: DataPoint[]
  convWindow?: number
  directionLabel?: string
}

interface LogChartProps {
  title: string
  data: DataPoint[]
  fields: { key: string; label: string; color: string }[]
  yLabel?: string
  /** If set, draw a regression line over the last `window` points for this field */
  regressionField?: string
  regressionWindow?: number
}

/**
 * Compute a simple linear regression line over the last `window` data points
 * for the given field key.  Returns two endpoints [{iteration, value}, ...] or
 * null when there aren't enough points.
 */
function computeRegression(
  data: DataPoint[],
  fieldKey: string,
  window: number,
): { x1: number; y1: number; x2: number; y2: number } | null {
  const valid = data.filter((d) => d[fieldKey] !== undefined && d[fieldKey] > 0)
  if (valid.length < 2 || window < 2) return null

  const slice = valid.slice(-window)
  if (slice.length < 2) return null

  const n = slice.length
  let sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0
  for (const pt of slice) {
    const x = pt.iteration
    const y = pt[fieldKey]
    sumX += x
    sumY += y
    sumXY += x * y
    sumX2 += x * x
  }
  const denom = n * sumX2 - sumX * sumX
  if (Math.abs(denom) < 1e-30) return null

  const slope = (n * sumXY - sumX * sumY) / denom
  const intercept = (sumY - slope * sumX) / n

  const x1 = slice[0].iteration
  const x2 = slice[slice.length - 1].iteration

  return {
    x1,
    y1: slope * x1 + intercept,
    x2,
    y2: slope * x2 + intercept,
  }
}

function LogChart({ title, data, fields, yLabel, regressionField, regressionWindow }: LogChartProps) {
  const W = 600
  const H = 250
  const pad = { top: 28, right: 20, bottom: 36, left: 56 }
  const plotW = W - pad.left - pad.right
  const plotH = H - pad.top - pad.bottom

  const iters = data.map((r) => r.iteration)
  const minIter = Math.min(...iters)
  const maxIter = Math.max(...iters)

  let minVal = Infinity
  let maxVal = -Infinity
  for (const r of data) {
    for (const f of fields) {
      const v = r[f.key]
      if (v !== undefined && v > 0) {
        minVal = Math.min(minVal, v)
        maxVal = Math.max(maxVal, v)
      }
    }
  }

  if (!isFinite(minVal)) minVal = 1e-10
  if (!isFinite(maxVal)) maxVal = 1
  const logMin = Math.floor(Math.log10(minVal))
  const logMax = Math.ceil(Math.log10(maxVal))

  const xScale = (iter: number) =>
    pad.left + ((iter - minIter) / (maxIter - minIter || 1)) * plotW

  const yScale = (val: number) => {
    if (val <= 0) return pad.top + plotH
    const logVal = Math.log10(val)
    return pad.top + plotH - ((logVal - logMin) / (logMax - logMin || 1)) * plotH
  }

  const yTicks: number[] = []
  for (let exp = logMin; exp <= logMax; exp++) yTicks.push(exp)

  const nXTicks = Math.min(6, maxIter - minIter)
  const xTicks: number[] = []
  for (let i = 0; i <= nXTicks; i++)
    xTicks.push(Math.round(minIter + (i / (nXTicks || 1)) * (maxIter - minIter)))

  // Compute regression line if requested
  const regression =
    regressionField && regressionWindow
      ? computeRegression(data, regressionField, regressionWindow)
      : null

  return (
    <div>
      <h4 className="text-sm font-semibold text-gray-300 mb-2">{title}</h4>
      <svg viewBox={`0 0 ${W} ${H}`} className="w-full" style={{ maxHeight: '250px' }}>
        {yTicks.map((exp) => (
          <line
            key={exp}
            x1={pad.left} y1={yScale(10 ** exp)}
            x2={pad.left + plotW} y2={yScale(10 ** exp)}
            stroke="#374151" strokeWidth={0.5}
          />
        ))}

        {yTicks.map((exp) => (
          <text
            key={exp}
            x={pad.left - 8} y={yScale(10 ** exp) + 4}
            textAnchor="end" fill="#9ca3af" fontSize={10}
          >
            1e{exp}
          </text>
        ))}

        {xTicks.map((iter, i) => (
          <text
            key={i}
            x={xScale(iter)} y={pad.top + plotH + 18}
            textAnchor="middle" fill="#9ca3af" fontSize={10}
          >
            {iter}
          </text>
        ))}

        <line
          x1={pad.left} y1={pad.top}
          x2={pad.left} y2={pad.top + plotH}
          stroke="#6b7280" strokeWidth={1}
        />
        <line
          x1={pad.left} y1={pad.top + plotH}
          x2={pad.left + plotW} y2={pad.top + plotH}
          stroke="#6b7280" strokeWidth={1}
        />

        {fields.map((f) => {
          const points = data
            .filter((r) => r[f.key] !== undefined && r[f.key] > 0)
            .map((r) => `${xScale(r.iteration)},${yScale(r[f.key])}`)
          if (points.length < 2) return null
          return (
            <polyline
              key={f.key}
              points={points.join(' ')}
              fill="none"
              stroke={f.color}
              strokeWidth={1.5}
            />
          )
        })}

        {/* Regression line over last window points */}
        {regression && regression.y1 > 0 && regression.y2 > 0 && (
          <line
            x1={xScale(regression.x1)}
            y1={yScale(regression.y1)}
            x2={xScale(regression.x2)}
            y2={yScale(regression.y2)}
            stroke="#f97316"
            strokeWidth={2}
            strokeDasharray="6 3"
          />
        )}

        {/* Window boundary marker */}
        {regression && (() => {
          const windowStart = regression.x1
          const sx = xScale(windowStart)
          return (
            <line
              x1={sx} y1={pad.top}
              x2={sx} y2={pad.top + plotH}
              stroke="#f97316"
              strokeWidth={0.5}
              strokeDasharray="3 3"
              opacity={0.5}
            />
          )
        })()}

        <text
          x={pad.left + plotW / 2} y={H - 4}
          textAnchor="middle" fill="#9ca3af" fontSize={11}
        >
          Iteration
        </text>

        {yLabel && (
          <text
            x={14} y={pad.top + plotH / 2}
            textAnchor="middle" fill="#9ca3af" fontSize={10}
            transform={`rotate(-90, 14, ${pad.top + plotH / 2})`}
          >
            {yLabel}
          </text>
        )}

        {/* Legend */}
        {fields.map((f, i) => (
          <g key={f.key} transform={`translate(${pad.left + i * 80}, ${pad.top - 14})`}>
            <line x1={0} y1={0} x2={16} y2={0} stroke={f.color} strokeWidth={2} />
            <text x={20} y={4} fill="#d1d5db" fontSize={10}>{f.label}</text>
          </g>
        ))}
        {regression && (
          <g transform={`translate(${pad.left + fields.length * 80}, ${pad.top - 14})`}>
            <line x1={0} y1={0} x2={16} y2={0} stroke="#f97316" strokeWidth={2} strokeDasharray="4 2" />
            <text x={20} y={4} fill="#d1d5db" fontSize={10}>Regression</text>
          </g>
        )}
      </svg>
    </div>
  )
}

const RESIDUAL_FIELDS = [
  { key: 'Ux', label: 'Ux', color: '#3b82f6' },
  { key: 'Uy', label: 'Uy', color: '#10b981' },
  { key: 'Uz', label: 'Uz', color: '#f59e0b' },
  { key: 'p', label: 'p', color: '#ef4444' },
]

const PERM_FIELDS = [
  { key: 'permVolAvg', label: 'K vol-avg', color: '#a855f7' },
]

export default function ConvergenceChart({ residuals, convWindow, directionLabel }: ConvergenceChartProps) {
  const [windowSize, setWindowSize] = useState(DEFAULT_VISIBLE)
  const [offset, setOffset] = useState<number | null>(null) // null = follow latest

  const title = directionLabel
    ? `Solver Convergence — ${directionLabel} Direction`
    : 'Solver Convergence'

  if (!residuals || residuals.length < 2) {
    return (
      <div className="card">
        <h3 className="card-header">{title}</h3>
        <p className="text-gray-500 text-sm">Waiting for solver data...</p>
      </div>
    )
  }

  const total = residuals.length
  const start = offset !== null ? offset : Math.max(0, total - windowSize)
  const visibleData = residuals.slice(start, start + windowSize)
  const atEnd = start + windowSize >= total

  const hasResiduals = residuals.some((r) =>
    RESIDUAL_FIELDS.some((f) => r[f.key] !== undefined && r[f.key] > 0)
  )
  const hasPerm = residuals.some((r) => r.permVolAvg !== undefined && r.permVolAvg > 0)

  const scrollBack = () => {
    const newStart = Math.max(0, start - Math.floor(windowSize / 2))
    setOffset(newStart)
  }
  const scrollForward = () => {
    const newStart = Math.min(total - windowSize, start + Math.floor(windowSize / 2))
    if (newStart + windowSize >= total) {
      setOffset(null) // snap to latest
    } else {
      setOffset(Math.max(0, newStart))
    }
  }
  const goToEnd = () => setOffset(null)
  const zoomIn = () => setWindowSize((w) => Math.max(10, Math.floor(w / 2)))
  const zoomOut = () => setWindowSize((w) => Math.min(total, w * 2))
  const showAll = () => { setWindowSize(total); setOffset(0) }

  return (
    <div className="card space-y-4">
      <div className="flex items-center justify-between">
        <h3 className="card-header mb-0">{title}</h3>
        <div className="flex items-center gap-1 text-xs">
          <span className="text-gray-500 mr-2">
            {start + 1}–{Math.min(start + windowSize, total)} / {total}
          </span>
          <button onClick={scrollBack} disabled={start === 0}
            className="px-1.5 py-0.5 rounded bg-gray-700 text-gray-300 hover:bg-gray-600 disabled:opacity-30 disabled:cursor-default"
            title="Scroll back">&#9664;</button>
          <button onClick={scrollForward} disabled={atEnd}
            className="px-1.5 py-0.5 rounded bg-gray-700 text-gray-300 hover:bg-gray-600 disabled:opacity-30 disabled:cursor-default"
            title="Scroll forward">&#9654;</button>
          <button onClick={zoomIn} disabled={windowSize <= 10}
            className="px-1.5 py-0.5 rounded bg-gray-700 text-gray-300 hover:bg-gray-600 disabled:opacity-30 disabled:cursor-default"
            title="Zoom in">+</button>
          <button onClick={zoomOut} disabled={windowSize >= total}
            className="px-1.5 py-0.5 rounded bg-gray-700 text-gray-300 hover:bg-gray-600 disabled:opacity-30 disabled:cursor-default"
            title="Zoom out">&minus;</button>
          {windowSize < total && (
            <button onClick={showAll}
              className="px-1.5 py-0.5 rounded bg-gray-700 text-gray-300 hover:bg-gray-600"
              title="Show all">All</button>
          )}
          {!atEnd && (
            <button onClick={goToEnd}
              className="px-1.5 py-0.5 rounded bg-gray-700 text-gray-300 hover:bg-gray-600"
              title="Jump to latest">Latest</button>
          )}
        </div>
      </div>
      {hasResiduals && (
        <LogChart title="Residuals" data={visibleData} fields={RESIDUAL_FIELDS} />
      )}
      {hasPerm && (
        <LogChart
          title="Permeability"
          data={visibleData}
          fields={PERM_FIELDS}
          yLabel="K (m²)"
          regressionField="permVolAvg"
          regressionWindow={convWindow}
        />
      )}
      {!hasResiduals && !hasPerm && (
        <p className="text-gray-500 text-sm">Waiting for solver data...</p>
      )}
    </div>
  )
}
