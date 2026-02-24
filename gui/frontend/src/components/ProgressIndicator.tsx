interface ProgressIndicatorProps {
  /** Progress percentage 0..100. Pass -1 for indeterminate. */
  progress: number
  /** Optional label shown above the bar */
  label?: string
  /** Optional status text shown below the bar */
  status?: string
  /** Bar color variant */
  variant?: 'primary' | 'success' | 'warning' | 'error'
}

const variantColors: Record<string, { bar: string; bg: string; text: string }> = {
  primary: {
    bar: 'bg-primary-500',
    bg: 'bg-primary-500/10',
    text: 'text-primary-400',
  },
  success: {
    bar: 'bg-green-500',
    bg: 'bg-green-500/10',
    text: 'text-green-400',
  },
  warning: {
    bar: 'bg-yellow-500',
    bg: 'bg-yellow-500/10',
    text: 'text-yellow-400',
  },
  error: {
    bar: 'bg-red-500',
    bg: 'bg-red-500/10',
    text: 'text-red-400',
  },
}

export default function ProgressIndicator({
  progress,
  label,
  status,
  variant = 'primary',
}: ProgressIndicatorProps) {
  const isIndeterminate = progress < 0
  const clampedProgress = Math.min(100, Math.max(0, progress))
  const colors = variantColors[variant]

  return (
    <div className="w-full">
      {/* Label row */}
      {(label || !isIndeterminate) && (
        <div className="flex items-center justify-between mb-2">
          {label && (
            <span className="text-sm font-medium text-gray-300">{label}</span>
          )}
          {!isIndeterminate && (
            <span className={`text-sm font-mono font-bold ${colors.text}`}>
              {Math.round(clampedProgress)}%
            </span>
          )}
        </div>
      )}

      {/* Bar track */}
      <div className={`relative w-full h-2.5 rounded-full overflow-hidden ${colors.bg}`}>
        {isIndeterminate ? (
          <div
            className={`absolute inset-y-0 w-1/3 rounded-full ${colors.bar} animate-indeterminate`}
            style={{
              animation: 'indeterminate 1.5s ease-in-out infinite',
            }}
          />
        ) : (
          <div
            className={`h-full rounded-full transition-all duration-500 ease-out ${colors.bar}`}
            style={{ width: `${clampedProgress}%` }}
          />
        )}
      </div>

      {/* Status text */}
      {status && (
        <p className="mt-1.5 text-xs text-gray-500">{status}</p>
      )}

      {/* Inline keyframes for indeterminate animation */}
      {isIndeterminate && (
        <style>{`
          @keyframes indeterminate {
            0% { left: -33%; }
            100% { left: 100%; }
          }
          .animate-indeterminate {
            animation: indeterminate 1.5s ease-in-out infinite;
          }
        `}</style>
      )}
    </div>
  )
}
