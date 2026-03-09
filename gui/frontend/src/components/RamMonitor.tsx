import { useState, useEffect, useRef } from 'react'
import { getSystemStats, type SystemStats } from '../api/client'

interface RamMonitorProps {
  /** Poll faster when active (e.g. pipeline is running) */
  active?: boolean
  /** Poll interval in ms when active (default 3000) */
  interval?: number
}

export default function RamMonitor({ active = false, interval = 3000 }: RamMonitorProps) {
  const [stats, setStats] = useState<SystemStats | null>(null)
  const [error, setError] = useState(false)
  const timerRef = useRef<ReturnType<typeof setInterval> | null>(null)

  useEffect(() => {
    const poll = () => {
      getSystemStats()
        .then((s) => { setStats(s); setError(false) })
        .catch(() => setError(true))
    }

    poll() // always fetch immediately

    // Poll at interval when active, slower (10s) when idle
    const ms = active ? interval : 10000
    timerRef.current = setInterval(poll, ms)

    return () => {
      if (timerRef.current) clearInterval(timerRef.current)
    }
  }, [active, interval])

  if (error || !stats) {
    return (
      <div className="flex items-center gap-3 px-4 py-2 rounded-lg bg-gray-800/50 border border-gray-700">
        <svg className="w-4 h-4 text-gray-500 shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
          <path strokeLinecap="round" strokeLinejoin="round" d="M8.25 3v1.5M4.5 8.25H3m18 0h-1.5M4.5 12H3m18 0h-1.5m-15 3.75H3m18 0h-1.5M8.25 19.5V21M12 3v1.5m0 15V21m3.75-18v1.5m0 15V21m-9-1.5h10.5a2.25 2.25 0 002.25-2.25V6.75a2.25 2.25 0 00-2.25-2.25H6.75A2.25 2.25 0 004.5 6.75v10.5a2.25 2.25 0 002.25 2.25z" />
        </svg>
        <span className="text-xs text-gray-500">RAM: --</span>
      </div>
    )
  }

  const pct = stats.percent
  const barColor =
    pct >= 90 ? 'bg-red-500' : pct >= 70 ? 'bg-yellow-500' : 'bg-primary-500'
  const textColor =
    pct >= 90 ? 'text-red-400' : pct >= 70 ? 'text-yellow-400' : 'text-gray-400'

  return (
    <div className="flex items-center gap-3 px-4 py-2 rounded-lg bg-gray-800/50 border border-gray-700">
      <svg className="w-4 h-4 text-gray-500 shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
        <path strokeLinecap="round" strokeLinejoin="round" d="M8.25 3v1.5M4.5 8.25H3m18 0h-1.5M4.5 12H3m18 0h-1.5m-15 3.75H3m18 0h-1.5M8.25 19.5V21M12 3v1.5m0 15V21m3.75-18v1.5m0 15V21m-9-1.5h10.5a2.25 2.25 0 002.25-2.25V6.75a2.25 2.25 0 00-2.25-2.25H6.75A2.25 2.25 0 004.5 6.75v10.5a2.25 2.25 0 002.25 2.25z" />
      </svg>
      <div className="flex-1 min-w-0">
        <div className="flex items-center justify-between mb-1">
          <span className="text-xs text-gray-500">RAM</span>
          <span className={`text-xs font-mono ${textColor}`}>
            {stats.usedGb} / {stats.totalGb} GB ({pct}%)
          </span>
        </div>
        <div className="h-1.5 bg-gray-700 rounded-full overflow-hidden">
          <div
            className={`h-full rounded-full transition-all duration-500 ${barColor}`}
            style={{ width: `${Math.min(100, pct)}%` }}
          />
        </div>
      </div>
    </div>
  )
}
