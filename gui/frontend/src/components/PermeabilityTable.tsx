import type { PermeabilityResult } from '../api/client'

interface PermeabilityTableProps {
  results: PermeabilityResult[]
  loading?: boolean
}

function formatScientific(value: number): string {
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
                Darcy Method
              </th>
              <th className="pb-3 pr-4 text-gray-400 font-medium">
                Pressure Drop Method
              </th>
              <th className="pb-3 text-gray-400 font-medium">Unit</th>
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
                  {formatScientific(row.darcyMethod)}
                </td>
                <td className="py-3 pr-4 font-mono text-gray-200">
                  {formatScientific(row.pressureDropMethod)}
                </td>
                <td className="py-3 text-gray-400">{row.unit}</td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>

      {/* Summary row */}
      {results.length > 1 && (
        <div className="mt-4 pt-4 border-t border-gray-700">
          <div className="flex items-center gap-6 text-xs text-gray-500">
            <span>
              Average (Darcy):{' '}
              <span className="font-mono text-gray-300">
                {formatScientific(
                  results.reduce((s, r) => s + r.darcyMethod, 0) / results.length,
                )}
              </span>
            </span>
            <span>
              Average (Pressure Drop):{' '}
              <span className="font-mono text-gray-300">
                {formatScientific(
                  results.reduce((s, r) => s + r.pressureDropMethod, 0) /
                    results.length,
                )}
              </span>
            </span>
          </div>
        </div>
      )}
    </div>
  )
}
