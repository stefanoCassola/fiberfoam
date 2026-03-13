import { useState, useEffect, useCallback, useMemo } from 'react'
import { getBackendUrl, setBackendUrl, getHealth, checkForUpdates, type UpdateCheckResponse } from '../api/client'

// The frontend always ships at the latest version — use it to detect
// outdated backends even when the backend's own update check is broken.
const FRONTEND_VERSION = __APP_VERSION__

interface SetupPageProps {
  connected: boolean
  onContinue: () => void
  onRecheck: () => Promise<void>
}

type OS = 'windows' | 'mac' | 'linux' | 'unknown'

function detectOS(): OS {
  const ua = navigator.userAgent.toLowerCase()
  if (ua.includes('win')) return 'windows'
  if (ua.includes('mac')) return 'mac'
  if (ua.includes('linux')) return 'linux'
  return 'unknown'
}

const DOCKER_INSTALL: Record<OS, { name: string; url: string; steps: string[] }> = {
  windows: {
    name: 'Windows',
    url: 'https://docs.docker.com/desktop/install/windows-install/',
    steps: [
      'Download Docker Desktop for Windows from the link below',
      'Run the installer and follow the prompts',
      'Restart your computer if prompted',
      'Open Docker Desktop and wait for it to start',
    ],
  },
  mac: {
    name: 'macOS',
    url: 'https://docs.docker.com/desktop/install/mac-install/',
    steps: [
      'Download Docker Desktop for Mac from the link below',
      'Open the .dmg file and drag Docker to Applications',
      'Open Docker from Applications and wait for it to start',
    ],
  },
  linux: {
    name: 'Linux',
    url: 'https://docs.docker.com/engine/install/',
    steps: [
      'Install Docker Engine using your package manager',
      'For Ubuntu/Debian: sudo apt-get update && sudo apt-get install docker.io docker-compose-v2',
      'Add your user to the docker group: sudo usermod -aG docker $USER',
      'Log out and back in, then start Docker: sudo systemctl start docker',
    ],
  },
  unknown: {
    name: 'your OS',
    url: 'https://docs.docker.com/get-docker/',
    steps: ['Visit the Docker website and follow the instructions for your platform'],
  },
}

const DOCKER_IMAGE = 'ghcr.io/stefanocassola/fiberfoam:latest'
const DOCKER_RUN_CMD = `docker run -d --name fiberfoam -p 3000:8000 -v fiberfoam-data:/data -v /:/host ${DOCKER_IMAGE}`
const DOCKER_RUN_CMD_WIN = `docker run -d --name fiberfoam -p 3000:8000 -v fiberfoam-data:/data -v C:\\:/host ${DOCKER_IMAGE}`
const DOCKER_UPDATE_CMD = `docker pull ${DOCKER_IMAGE} && docker rm -f fiberfoam && docker run -d --name fiberfoam -p 3000:8000 -v fiberfoam-data:/data -v /:/host ${DOCKER_IMAGE}`
const DOCKER_UPDATE_CMD_WIN = `docker pull ${DOCKER_IMAGE} && docker rm -f fiberfoam && docker run -d --name fiberfoam -p 3000:8000 -v fiberfoam-data:/data -v C:\\:/host ${DOCKER_IMAGE}`

const PARAVIEW_INSTALL: Record<OS, { cmd: string; url: string }> = {
  windows: {
    cmd: 'Download the installer from paraview.org',
    url: 'https://www.paraview.org/download/',
  },
  mac: {
    cmd: 'brew install --cask paraview',
    url: 'https://www.paraview.org/download/',
  },
  linux: {
    cmd: 'sudo apt install paraview',
    url: 'https://www.paraview.org/download/',
  },
  unknown: {
    cmd: 'Download from paraview.org',
    url: 'https://www.paraview.org/download/',
  },
}

function StatusDot({ ok }: { ok: boolean }) {
  return (
    <div className={`w-3 h-3 rounded-full shrink-0 ${ok ? 'bg-green-500' : 'bg-red-500 animate-pulse'}`} />
  )
}

export default function SetupPage({ connected, onContinue, onRecheck }: SetupPageProps) {
  const [os] = useState<OS>(detectOS)
  const [copied, setCopied] = useState<string | false>(false)
  const [customUrl, setCustomUrl] = useState(getBackendUrl())
  const [version, setVersion] = useState<string | null>(null)
  const [updateInfo, setUpdateInfo] = useState<UpdateCheckResponse | null>(null)
  const [checkingUpdate, setCheckingUpdate] = useState(false)

  // Auto-retry connection every 5s when not connected
  useEffect(() => {
    if (connected) return
    const timer = setInterval(() => onRecheck(), 5000)
    return () => clearInterval(timer)
  }, [connected, onRecheck])

  const fetchVersionAndUpdates = useCallback(() => {
    setVersion(null)
    setUpdateInfo(null)
    getHealth().then((h) => setVersion(h.version ?? null)).catch(() => {})
    setCheckingUpdate(true)
    checkForUpdates()
      .then(setUpdateInfo)
      .catch(() => {})
      .finally(() => setCheckingUpdate(false))
  }, [])

  // Client-side outdated detection: compare backend version against frontend version
  const backendOutdated = useMemo(() => {
    if (!version) return false
    // Parse semver-like "0.2.0", "0.2.0+sha.abc", "sha-abc", "dev"
    const parse = (v: string) => {
      const m = v.match(/^v?(\d+)\.(\d+)\.(\d+)/)
      return m ? [+m[1], +m[2], +m[3]] as const : null
    }
    const bv = parse(version)
    const fv = parse(FRONTEND_VERSION)
    if (!fv) return false
    if (!bv) return true // backend is "dev" or unparseable → outdated
    return bv[0] < fv[0] || (bv[0] === fv[0] && bv[1] < fv[1]) || (bv[0] === fv[0] && bv[1] === fv[1] && bv[2] < fv[2])
  }, [version])

  // Effective update flag: backend's own check OR client-side detection
  const showUpdate = backendOutdated || updateInfo?.updateAvailable === true

  // Fetch version and check for updates once connected
  useEffect(() => {
    if (!connected) return
    fetchVersionAndUpdates()
  }, [connected, fetchVersionAndUpdates])

  const handleCopy = (text: string, label?: string) => {
    navigator.clipboard.writeText(text)
    setCopied(label ?? 'default')
    setTimeout(() => setCopied(false), 2000)
  }

  const handleUrlSave = () => {
    setBackendUrl(customUrl)
    onRecheck()
    // Re-fetch version and update info from the new backend
    setTimeout(fetchVersionAndUpdates, 500)
  }

  const install = DOCKER_INSTALL[os]
  const runCmd = os === 'windows' ? DOCKER_RUN_CMD_WIN : DOCKER_RUN_CMD
  const pv = PARAVIEW_INSTALL[os]

  return (
    <div className="min-h-screen bg-gray-950 flex items-center justify-center p-8">
      <div className="max-w-2xl w-full">
        {/* Header */}
        <div className="text-center mb-10">
          <div className="flex h-16 w-16 items-center justify-center rounded-2xl bg-primary-600 mx-auto mb-4">
            <svg className="w-9 h-9 text-white" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M21 7.5l-9-5.25L3 7.5m18 0l-9 5.25m9-5.25v9l-9 5.25M3 7.5l9 5.25M3 7.5v9l9 5.25m0-9v9" />
            </svg>
          </div>
          <h1 className="text-3xl font-bold text-white mb-2">FiberFoam</h1>
          <p className="text-gray-400">Permeability simulation for fibrous microstructures</p>
        </div>

        {/* Status overview */}
        <div className="card mb-6">
          <h2 className="text-lg font-semibold text-white mb-4">Status</h2>
          <div className="space-y-3">
            <div className="flex items-center gap-3">
              <StatusDot ok={true} />
              <span className="text-sm text-gray-300">Docker required</span>
              <a href={install.url} target="_blank" rel="noopener noreferrer"
                className="ml-auto text-xs text-primary-400 hover:text-primary-300">
                Install Docker
              </a>
            </div>
            <div className="flex items-center gap-3">
              <StatusDot ok={connected} />
              <span className="text-sm text-gray-300">
                FiberFoam backend {connected ? 'connected' : 'not detected'}
              </span>
              {connected && (
                <span className="ml-auto text-xs text-green-400">Ready</span>
              )}
              {!connected && (
                <div className="ml-auto flex items-center gap-2">
                  <div className="w-4 h-4 border-2 border-primary-500 border-t-transparent rounded-full animate-spin" />
                  <span className="text-xs text-gray-500">Checking...</span>
                </div>
              )}
            </div>
          </div>

          {connected && version && (
            <div className="flex items-center gap-3 mt-3">
              <StatusDot ok={true} />
              <span className="text-sm text-gray-300">
                Version: <code className="text-gray-200">{version}</code>
              </span>
              {checkingUpdate && (
                <span className="ml-auto text-xs text-gray-500">Checking for updates...</span>
              )}
              {showUpdate && (
                <span className="ml-auto text-xs text-yellow-400">
                  Update available{updateInfo?.latestVersion ? ` (${updateInfo.latestVersion})` : ` (${FRONTEND_VERSION})`}
                </span>
              )}
              {!showUpdate && !checkingUpdate && version && (
                <span className="ml-auto text-xs text-green-400">Up to date</span>
              )}
            </div>
          )}

          {/* Update available banner */}
          {connected && showUpdate && (
            <div className="mt-4 p-4 rounded-lg bg-yellow-900/20 border border-yellow-800">
              <div className="flex items-center gap-2 mb-3">
                <svg className="w-5 h-5 text-yellow-400 shrink-0" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M12 9v3.75m9-.75a9 9 0 11-18 0 9 9 0 0118 0zm-9 3.75h.008v.008H12v-.008z" />
                </svg>
                <p className="text-sm font-semibold text-yellow-300">
                  Update available: {version} &rarr; {updateInfo?.latestVersion ?? FRONTEND_VERSION}
                </p>
              </div>
              <p className="text-sm text-gray-400 mb-2">
                Copy and paste this single command into your terminal{os === 'windows' ? ' (PowerShell)' : ''} to update:
              </p>
              <div className="relative">
                <pre className="bg-gray-800 rounded-lg p-3 text-xs text-gray-300 overflow-x-auto font-mono whitespace-pre-wrap">{os === 'windows' ? DOCKER_UPDATE_CMD_WIN : DOCKER_UPDATE_CMD}</pre>
                <button
                  onClick={() =>
                    handleCopy(
                      os === 'windows' ? DOCKER_UPDATE_CMD_WIN : DOCKER_UPDATE_CMD,
                      'update',
                    )
                  }
                  className="absolute top-2 right-2 px-2 py-1 text-xs bg-gray-700 hover:bg-gray-600 text-gray-300 rounded transition-colors"
                >
                  {copied === 'update' ? 'Copied!' : 'Copy'}
                </button>
              </div>
              <p className="text-xs text-gray-500 mt-2">
                Your simulation data is stored in Docker volumes and will be preserved across updates.
              </p>
            </div>
          )}

          {connected && (
            <button
              onClick={onContinue}
              className="btn-primary w-full mt-6 py-3 text-base font-semibold"
            >
              Continue to FiberFoam
            </button>
          )}
        </div>

        {/* Advanced: custom URL — always visible */}
        <details className="card mb-4">
          <summary className="text-sm text-gray-400 cursor-pointer hover:text-gray-200">
            Advanced: Custom backend URL
          </summary>
          <div className="mt-4 flex gap-2">
            <input
              type="text"
              value={customUrl}
              onChange={(e) => setCustomUrl(e.target.value)}
              className="flex-1 bg-gray-800 border border-gray-700 rounded-lg px-3 py-2 text-sm text-gray-200 focus:outline-none focus:border-primary-500"
              placeholder="http://localhost:3000/api"
            />
            <button onClick={handleUrlSave} className="btn-primary px-4 text-sm">Save</button>
          </div>
          <p className="text-xs text-gray-500 mt-2">
            Change this if the Docker container is running on a different port or remote machine.
          </p>
        </details>

        {/* Setup instructions — shown when not connected */}
        {!connected && (
          <>
            {/* Step 1: Install Docker */}
            <div className="card mb-4">
              <div className="flex items-center gap-3 mb-4">
                <div className="flex h-8 w-8 items-center justify-center rounded-full bg-primary-600/20 text-primary-400 text-sm font-bold">1</div>
                <h2 className="text-lg font-semibold text-white">Install Docker</h2>
                <span className="text-xs text-gray-500 ml-auto">Detected: {install.name}</span>
              </div>
              <ol className="space-y-2 mb-4">
                {install.steps.map((s, i) => (
                  <li key={i} className="text-sm text-gray-400 flex gap-2">
                    <span className="text-gray-600 shrink-0">{i + 1}.</span>
                    {s}
                  </li>
                ))}
              </ol>
              <a
                href={install.url}
                target="_blank"
                rel="noopener noreferrer"
                className="btn-primary inline-flex items-center gap-2 px-4 py-2 text-sm"
              >
                <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
                  <path strokeLinecap="round" strokeLinejoin="round" d="M13.5 6H5.25A2.25 2.25 0 003 8.25v10.5A2.25 2.25 0 005.25 21h10.5A2.25 2.25 0 0018 18.75V10.5m-10.5 6L21 3m0 0h-5.25M21 3v5.25" />
                </svg>
                Download Docker for {install.name}
              </a>
            </div>

            {/* Step 2: Run container */}
            <div className="card mb-4">
              <div className="flex items-center gap-3 mb-4">
                <div className="flex h-8 w-8 items-center justify-center rounded-full bg-primary-600/20 text-primary-400 text-sm font-bold">2</div>
                <h2 className="text-lg font-semibold text-white">Run FiberFoam</h2>
              </div>
              <p className="text-sm text-gray-400 mb-3">
                Open a terminal{os === 'windows' ? ' (PowerShell or Command Prompt)' : ''} and run:
              </p>
              <div className="relative">
                <pre className="bg-gray-800 rounded-lg p-4 text-sm text-gray-300 overflow-x-auto font-mono">
                  {runCmd}
                </pre>
                <button
                  onClick={() => handleCopy(runCmd, 'run')}
                  className="absolute top-2 right-2 px-2 py-1 text-xs bg-gray-700 hover:bg-gray-600 text-gray-300 rounded transition-colors"
                >
                  {copied === 'run' ? 'Copied!' : 'Copy'}
                </button>
              </div>
              <p className="text-xs text-gray-500 mt-2">
                This downloads and starts FiberFoam. First run may take a few minutes to download (~2 GB).
              </p>
            </div>

            {/* Step 3: Waiting */}
            <div className="card mb-4">
              <div className="flex items-center gap-3 mb-4">
                <div className="flex h-8 w-8 items-center justify-center rounded-full bg-primary-600/20 text-primary-400 text-sm font-bold">3</div>
                <h2 className="text-lg font-semibold text-white">Waiting for connection...</h2>
              </div>
              <div className="flex items-center gap-3">
                <div className="w-5 h-5 border-2 border-primary-500 border-t-transparent rounded-full animate-spin" />
                <p className="text-sm text-gray-400">
                  Checking for backend at <code className="text-gray-300">{getBackendUrl()}</code> every 5 seconds...
                </p>
              </div>
              <p className="text-xs text-gray-500 mt-2">
                The "Continue" button will appear above once the backend is detected.
              </p>
            </div>

          </>
        )}

        {/* ParaView hint — always visible */}
        <div className="card">
          <h2 className="text-lg font-semibold text-white mb-3">Visualization with ParaView</h2>
          <p className="text-sm text-gray-400 mb-3">
            FiberFoam exports OpenFOAM cases and VTK files that can be visualized in
            {' '}<span className="text-gray-200">ParaView</span>, a free open-source tool for scientific visualization.
            Install it on your host machine to view meshes, velocity fields, and simulation results.
          </p>
          <div className="bg-gray-800 rounded-lg p-3 mb-3">
            <p className="text-xs text-gray-500 mb-1">Install on {install.name}:</p>
            <code className="text-sm text-gray-300">{pv.cmd}</code>
          </div>
          <a
            href={pv.url}
            target="_blank"
            rel="noopener noreferrer"
            className="text-xs text-primary-400 hover:text-primary-300 inline-flex items-center gap-1"
          >
            <svg className="w-3 h-3" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M13.5 6H5.25A2.25 2.25 0 003 8.25v10.5A2.25 2.25 0 005.25 21h10.5A2.25 2.25 0 0018 18.75V10.5m-10.5 6L21 3m0 0h-5.25M21 3v5.25" />
            </svg>
            paraview.org/download
          </a>
          <p className="text-xs text-gray-500 mt-3">
            After running a simulation, open the case with: <code className="text-gray-400">paraview case.foam</code>
          </p>
        </div>
      </div>
    </div>
  )
}
