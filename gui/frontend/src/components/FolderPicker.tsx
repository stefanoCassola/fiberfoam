import { useState, useEffect, useCallback } from 'react'
import { browseFilesystem, createDirectory, type BrowseEntry } from '../api/client'

interface FolderPickerProps {
  value: string
  onChange: (path: string) => void
  disabled?: boolean
}

export default function FolderPicker({ value, onChange, disabled }: FolderPickerProps) {
  const [open, setOpen] = useState(false)
  const [currentPath, setCurrentPath] = useState('/')
  const [dirs, setDirs] = useState<BrowseEntry[]>([])
  const [loading, setLoading] = useState(false)
  const [error, setError] = useState<string | null>(null)
  const [newFolderName, setNewFolderName] = useState('')
  const [creatingFolder, setCreatingFolder] = useState(false)

  const browse = useCallback(async (path: string) => {
    setLoading(true)
    setError(null)
    try {
      const result = await browseFilesystem(path || '/')
      setDirs(result.dirs)
      setCurrentPath(result.path || '/')
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to browse')
    } finally {
      setLoading(false)
    }
  }, [])

  useEffect(() => {
    if (open) {
      // Start from the currently selected path, or home directory, or root
      browse(value || '/')
    }
  }, [open]) // eslint-disable-line react-hooks/exhaustive-deps

  const handleSelect = useCallback(() => {
    onChange(currentPath)
    setOpen(false)
  }, [currentPath, onChange])

  const handleNavigate = useCallback(
    (dirName: string) => {
      const base = currentPath === '/' ? '' : currentPath
      browse(`${base}/${dirName}`)
    },
    [currentPath, browse],
  )

  const handleUp = useCallback(() => {
    if (currentPath === '/') return
    const parent = currentPath.replace(/\/[^/]+\/?$/, '') || '/'
    browse(parent)
  }, [currentPath, browse])

  const handleCreateFolder = useCallback(async () => {
    if (!newFolderName.trim()) return
    setCreatingFolder(true)
    setError(null)
    try {
      const base = currentPath === '/' ? '' : currentPath
      await createDirectory(`${base}/${newFolderName.trim()}`)
      setNewFolderName('')
      browse(currentPath)
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to create folder')
    } finally {
      setCreatingFolder(false)
    }
  }, [currentPath, newFolderName, browse])

  // Split path into breadcrumb parts
  const pathParts = currentPath === '/'
    ? []
    : currentPath.split('/').filter(Boolean)

  return (
    <div>
      {/* Trigger — styled like the upload drop zone */}
      <button
        type="button"
        onClick={() => setOpen(true)}
        disabled={disabled}
        className="flex flex-col items-center justify-center w-full h-28 border-2 border-dashed border-gray-600 rounded-lg cursor-pointer hover:border-primary-500 hover:bg-gray-800/50 transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
      >
        <svg className="w-8 h-8 text-gray-500 mb-1" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
          <path strokeLinecap="round" strokeLinejoin="round" d="M2.25 12.75V12A2.25 2.25 0 014.5 9.75h15A2.25 2.25 0 0121.75 12v.75m-8.69-6.44l-2.12-2.12a1.5 1.5 0 00-1.061-.44H4.5A2.25 2.25 0 002.25 6v12a2.25 2.25 0 002.25 2.25h15A2.25 2.25 0 0021.75 18V9a2.25 2.25 0 00-2.25-2.25h-5.379a1.5 1.5 0 01-1.06-.44z" />
        </svg>
        <span className={`text-sm ${value ? 'text-gray-200' : 'text-gray-400'}`}>
          {value ? value : 'Click to select output folder'}
        </span>
      </button>

      {/* Modal backdrop + dialog */}
      {open && (
        <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60 backdrop-blur-sm">
          <div className="bg-gray-900 border border-gray-700 rounded-xl shadow-2xl w-full max-w-lg mx-4">
            {/* Header */}
            <div className="flex items-center justify-between px-5 py-4 border-b border-gray-800">
              <h3 className="text-white font-semibold">Select Output Folder</h3>
              <button
                onClick={() => setOpen(false)}
                className="text-gray-500 hover:text-gray-300 transition-colors"
              >
                <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M6 18L18 6M6 6l12 12" />
                </svg>
              </button>
            </div>

            {/* Breadcrumb */}
            <div className="px-5 py-3 border-b border-gray-800 flex items-center gap-1 text-sm overflow-x-auto">
              <button
                onClick={() => browse('/')}
                className="text-primary-400 hover:text-primary-300 shrink-0 font-medium"
              >
                /
              </button>
              {pathParts.map((part, i) => (
                <span key={i} className="flex items-center gap-1">
                  <span className="text-gray-600">/</span>
                  <button
                    onClick={() => browse('/' + pathParts.slice(0, i + 1).join('/'))}
                    className="text-primary-400 hover:text-primary-300 font-mono text-xs"
                  >
                    {part}
                  </button>
                </span>
              ))}
            </div>

            {/* Directory list */}
            <div className="px-5 py-3 max-h-64 overflow-y-auto min-h-[160px]">
              {loading ? (
                <div className="flex items-center gap-2 text-sm text-gray-400 py-4">
                  <svg className="animate-spin h-4 w-4 text-primary-500" fill="none" viewBox="0 0 24 24">
                    <circle className="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" strokeWidth="4" />
                    <path className="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4z" />
                  </svg>
                  Loading...
                </div>
              ) : error ? (
                <div className="text-sm text-red-400 py-4">{error}</div>
              ) : (
                <div className="space-y-1">
                  {/* Up directory */}
                  {currentPath !== '/' && (
                    <button
                      onClick={handleUp}
                      className="flex items-center gap-3 w-full px-3 py-2 rounded-lg hover:bg-gray-800/50 transition-colors text-left"
                    >
                      <svg className="w-4 h-4 text-gray-500" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                        <path strokeLinecap="round" strokeLinejoin="round" d="M9 15L3 9m0 0l6-6M3 9h12a6 6 0 010 12h-3" />
                      </svg>
                      <span className="text-sm text-gray-400">..</span>
                    </button>
                  )}

                  {dirs.length === 0 && currentPath === '/' && (
                    <p className="text-sm text-gray-500 py-2">
                      Empty directory. Create a new folder below.
                    </p>
                  )}

                  {dirs.map((d) => (
                    <button
                      key={d.name}
                      onClick={() => handleNavigate(d.name)}
                      className="flex items-center gap-3 w-full px-3 py-2 rounded-lg hover:bg-gray-800/50 transition-colors text-left"
                    >
                      <svg className="w-4 h-4 text-yellow-500" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
                        <path strokeLinecap="round" strokeLinejoin="round" d="M2.25 12.75V12A2.25 2.25 0 014.5 9.75h15A2.25 2.25 0 0121.75 12v.75m-8.69-6.44l-2.12-2.12a1.5 1.5 0 00-1.061-.44H4.5A2.25 2.25 0 002.25 6v12a2.25 2.25 0 002.25 2.25h15A2.25 2.25 0 0021.75 18V9a2.25 2.25 0 00-2.25-2.25h-5.379a1.5 1.5 0 01-1.06-.44z" />
                      </svg>
                      <span className="text-sm text-gray-200 font-mono">{d.name}</span>
                    </button>
                  ))}
                </div>
              )}
            </div>

            {/* New folder */}
            <div className="px-5 py-3 border-t border-gray-800">
              <div className="flex gap-2">
                <input
                  type="text"
                  className="input-field flex-1 text-sm"
                  placeholder="New folder name..."
                  value={newFolderName}
                  onChange={(e) => setNewFolderName(e.target.value)}
                  onKeyDown={(e) => {
                    if (e.key === 'Enter') handleCreateFolder()
                  }}
                />
                <button
                  onClick={handleCreateFolder}
                  disabled={creatingFolder || !newFolderName.trim()}
                  className="btn-secondary text-sm px-3"
                >
                  {creatingFolder ? '...' : 'Create'}
                </button>
              </div>
            </div>

            {/* Footer: current selection + confirm */}
            <div className="px-5 py-4 border-t border-gray-800 flex items-center justify-between">
              <div className="text-xs text-gray-500 truncate mr-4 font-mono">
                {currentPath}
              </div>
              <div className="flex gap-2 shrink-0">
                <button onClick={() => setOpen(false)} className="btn-secondary text-sm">
                  Cancel
                </button>
                <button onClick={handleSelect} className="btn-primary text-sm">
                  Select This Folder
                </button>
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  )
}
