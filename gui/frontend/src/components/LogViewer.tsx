import { useEffect, useRef } from 'react'
import type { LogLine } from '../hooks/useWebSocket'

interface LogViewerProps {
  lines: LogLine[]
  connected: boolean
  onClear?: () => void
  maxHeight?: string
}

function levelClass(level: LogLine['level']): string {
  switch (level) {
    case 'error':
      return 'log-line-error'
    case 'warning':
      return 'log-line-warning'
    case 'success':
      return 'log-line-success'
    default:
      return 'log-line-info'
  }
}

function levelBadge(level: LogLine['level']): string {
  switch (level) {
    case 'error':
      return 'bg-red-900/50 text-red-400'
    case 'warning':
      return 'bg-yellow-900/50 text-yellow-400'
    case 'success':
      return 'bg-green-900/50 text-green-400'
    default:
      return 'bg-gray-800 text-gray-500'
  }
}

export default function LogViewer({
  lines,
  connected,
  onClear,
  maxHeight = '500px',
}: LogViewerProps) {
  const containerRef = useRef<HTMLDivElement>(null)
  const autoScrollRef = useRef(true)

  // Track whether user has scrolled up
  const handleScroll = () => {
    if (!containerRef.current) return
    const { scrollTop, scrollHeight, clientHeight } = containerRef.current
    autoScrollRef.current = scrollHeight - scrollTop - clientHeight < 40
  }

  // Auto-scroll to bottom when new lines arrive
  useEffect(() => {
    if (autoScrollRef.current && containerRef.current) {
      containerRef.current.scrollTop = containerRef.current.scrollHeight
    }
  }, [lines])

  return (
    <div className="flex flex-col rounded-xl border border-gray-700 bg-gray-950 overflow-hidden">
      {/* Header bar */}
      <div className="flex items-center justify-between px-4 py-2 bg-gray-900 border-b border-gray-800">
        <div className="flex items-center gap-2">
          <span
            className={`inline-block h-2 w-2 rounded-full ${
              connected ? 'bg-green-500 animate-pulse' : 'bg-gray-600'
            }`}
          />
          <span className="text-xs font-medium text-gray-400">
            {connected ? 'Connected' : 'Disconnected'}
          </span>
          <span className="text-xs text-gray-600 ml-2">
            {lines.length} lines
          </span>
        </div>
        {onClear && (
          <button
            onClick={onClear}
            className="text-xs text-gray-500 hover:text-gray-300 transition-colors"
          >
            Clear
          </button>
        )}
      </div>

      {/* Log content */}
      <div
        ref={containerRef}
        onScroll={handleScroll}
        className="overflow-y-auto p-4 space-y-0.5"
        style={{ maxHeight }}
      >
        {lines.length === 0 ? (
          <p className="text-gray-600 text-sm font-mono">
            Waiting for output...
          </p>
        ) : (
          lines.map((line, idx) => (
            <div key={idx} className="log-line flex items-start gap-2">
              <span className="text-[10px] text-gray-600 whitespace-nowrap pt-0.5 min-w-[70px]">
                {new Date(line.timestamp).toLocaleTimeString()}
              </span>
              <span
                className={`text-[9px] uppercase font-bold px-1.5 py-0.5 rounded min-w-[46px] text-center ${levelBadge(
                  line.level,
                )}`}
              >
                {line.level}
              </span>
              <span className={`flex-1 ${levelClass(line.level)} break-all`}>
                {line.message}
              </span>
            </div>
          ))
        )}
      </div>
    </div>
  )
}
