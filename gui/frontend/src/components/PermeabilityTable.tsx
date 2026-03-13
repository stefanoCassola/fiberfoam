import type { PermeabilityResult } from '../api/client'

interface PermeabilityTableProps {
  results: PermeabilityResult[]
  loading?: boolean
}

function formatScientific(value: number | undefined): string {
  if (value === undefined || value === null) return '-'
  if (value === 0) return '0'
  return value.toExponential(4)
}

export default function PermeabilityTable({
  results,
  loading = false,
}: PermeabilityTableProps) {
  if (loading) {
    return (
      <div className="card animate-pulse">
        <div className="h-6 bg-gray-700 rounded w-48 mb-4" />
        <div className="space-y-3">
          {[1, 2, 3].map((i) => (
            <div key={i} className="h-10 bg-gray-700 rounded" />
          ))}
        </div>
      </div>
    )
  }

  if (results.length === 0) {
    return (
      <div className="card">
        <h3 className="card-header">Permeability Results</h3>
        <p className="text-gray-500 text-sm">
          No results available. Run post-processing to compute permeability.
        </p>
      </div>
    )
  }

  return (
    <div className="card">
      <h3 className="card-header">Permeability Results</h3>
      <div className="overflow-x-auto">
        <table className="w-full text-sm">
          <thead>
            <tr className="text-left border-b border-gray-700">
              <th className="pb-3 pr-4 text-gray-400 font-medium">Direction</th>
              <th className="pb-3 pr-4 text-gray-400 font-medium">
                Vol. Avg. Permeability
              </th>
              <th className="pb-3 pr-4 text-gray-400 font-medium">
                Flow Rate Permeability
              </th>
              <th className="pb-3 text-gray-400 font-medium">Fiber Vol. Content</th>
            </tr>
          </thead>
          <tbody className="divide-y divide-gray-800">
            {results.map((row) => (
              <tr
                key={row.direction}
                className="hover:bg-gray-800/50 transition-colors"
              >
                <td className="py-3 pr-4">
                  <span className="inline-flex items-center justify-center w-8 h-8 rounded-lg bg-primary-600/20 text-primary-400 font-bold text-sm">
                    {row.direction.toUpperCase()}
                  </span>
                </td>
                <td className="py-3 pr-4 font-mono text-gray-200">
                  {formatScientific(row.permVolAvgMain)}
                </td>
                <td className="py-3 pr-4 font-mono text-gray-200">
                  {formatScientific(row.permFlowRate)}
                </td>
                <td className="py-3 font-mono text-gray-400">
                  {row.fiberVolumeContent !== undefined
                    ? `${row.fiberVolumeContent.toFixed(2)}%`
                    : '-'}
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {/* Full permeability tensor when all 3 directions are available */}
      {(() => {
        const byDir: Record<string, PermeabilityResult> = {}
        for (const r of results) byDir[r.direction] = r
        if (!byDir.x || !byDir.y || !byDir.z) return null

        // Each simulation direction gives one column of the tensor:
        //   X flow -> Kxx (main), Kyx (secondary), Kzx (tertiary)
        //   Y flow -> Kyy (main), Kxy (secondary), Kzy (tertiary)
        //   Z flow -> Kzz (main), Kxz (secondary), Kyz (tertiary)
        const tensor = [
          [byDir.x.permVolAvgMain,      byDir.y.permVolAvgTertiary, byDir.z.permVolAvgSecondary],
          [byDir.x.permVolAvgSecondary,  byDir.y.permVolAvgMain,     byDir.z.permVolAvgTertiary],
          [byDir.x.permVolAvgTertiary,   byDir.y.permVolAvgSecondary, byDir.z.permVolAvgMain],
        ]
        const labels = ['X', 'Y', 'Z']

        return (
          <div className="mt-6 pt-4 border-t border-gray-700">
            <h4 className="text-sm font-semibold text-gray-300 mb-3">
              Permeability Tensor (Vol. Avg.)
            </h4>
            <div className="overflow-x-auto">
              <table className="text-sm mx-auto">
                <thead>
                  <tr className="text-gray-500">
                    <th className="px-3 pb-2" />
                    {labels.map((l) => (
                      <th key={l} className="px-3 pb-2 font-medium text-center">{l}</th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {tensor.map((row, i) => (
                    <tr key={labels[i]}>
                      <td className="px-3 py-1 text-gray-500 font-medium">{labels[i]}</td>
                      {row.map((val, j) => (
                        <td
                          key={j}
                          className={`px-3 py-1 font-mono text-center ${
                            i === j ? 'text-primary-400 font-semibold' : 'text-gray-400'
                          }`}
                        >
                          {formatScientific(val)}
                        </td>
                      ))}
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
            <p className="text-xs text-gray-600 mt-2 text-center">
              Diagonal elements highlighted. Based on volume-averaged velocity.
            </p>
          </div>
        )
      })()}
    </div>
  )
}
