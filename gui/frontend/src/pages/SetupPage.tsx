import { useState, useEffect, useCallback } from 'react'
import { getBackendUrl, setBackendUrl } from '../api/client'

interface SetupPageProps {
  onConnected: () => void
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

const DOCKER_RUN_CMD = `docker run -d --name fiberfoam \\
  -p 3000:8000 \\
  -v fiberfoam-data:/data \\
  -v \${FIBERFOAM_BROWSE_ROOT:-/}:/host \\
  ghcr.io/stefanocassola/fiberfoam:latest`

const DOCKER_RUN_CMD_WIN = `docker run -d --name fiberfoam ^
  -p 3000:8000 ^
  -v fiberfoam-data:/data ^
  -v C:\\:/host ^
  ghcr.io/stefanocassola/fiberfoam:latest`

export default function SetupPage({ onConnected }: SetupPageProps) {
  const [os] = useState<OS>(detectOS)
  const [step, setStep] = useState<'checking' | 'setup' | 'running'>('checking')
  const [checking, setChecking] = useState(true)
  const [customUrl, setCustomUrl] = useState(getBackendUrl())
  const [copied, setCopied] = useState(false)

  const checkConnection = useCallback(async () => {
    setChecking(true)
    try {
      const url = getBackendUrl().replace(/\/api$/, '/api/health')
      const res = await fetch(url, { signal: AbortSignal.timeout(3000) })
      if (res.ok) {
        onConnected()
        return true
      }
    } catch { /* not reachable */ }
    setChecking(false)
    setStep('setup')
    return false
  }, [onConnected])

  // Check on mount
  useEffect(() => {
    checkConnection()
  }, [checkConnection])

  // Auto-retry every 5s when on setup/running step
  useEffect(() => {
    if (step === 'checking') return
    const timer = setInterval(() => {
      checkConnection()
    }, 5000)
    return () => clearInterval(timer)
  }, [step, checkConnection])

  const handleCopy = (text: string) => {
    navigator.clipboard.writeText(text)
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  const handleUrlSave = () => {
    setBackendUrl(customUrl)
    checkConnection()
  }

  const install = DOCKER_INSTALL[os]
  const runCmd = os === 'windows' ? DOCKER_RUN_CMD_WIN : DOCKER_RUN_CMD

  if (step === 'checking' && checking) {
    return (
      <div className="min-h-screen bg-gray-950 flex items-center justify-center">
        <div className="text-center">
          <div className="w-12 h-12 border-4 border-primary-500 border-t-transparent rounded-full animate-spin mx-auto mb-4" />
          <p className="text-gray-400">Connecting to FiberFoam backend...</p>
        </div>
      </div>
    )
  }

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

        {/* Connection status */}
        <div className="card mb-6">
          <div className="flex items-center gap-3 mb-4">
            <div className="w-3 h-3 rounded-full bg-red-500 animate-pulse" />
            <span className="text-sm text-gray-300">Backend not detected</span>
          </div>
          <p className="text-sm text-gray-400">
            FiberFoam runs simulations on your local machine using Docker.
            Follow the steps below to get started.
          </p>
        </div>

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
              onClick={() => handleCopy(runCmd)}
              className="absolute top-2 right-2 px-2 py-1 text-xs bg-gray-700 hover:bg-gray-600 text-gray-300 rounded transition-colors"
            >
              {copied ? 'Copied!' : 'Copy'}
            </button>
          </div>
          <p className="text-xs text-gray-500 mt-2">
            This downloads and starts FiberFoam. First run may take a few minutes to download (~2 GB).
          </p>
        </div>

        {/* Step 3: Waiting for connection */}
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
            This page will automatically proceed once the backend is detected.
          </p>
        </div>

        {/* Advanced: custom URL */}
        <details className="card">
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
      </div>
    </div>
  )
}
