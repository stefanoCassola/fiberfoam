import { useState, useEffect } from 'react'
import { NavLink } from 'react-router-dom'
import { submitFeedback, getHealth, getBackendUrl, setBackendUrl, resetBackendUrl } from '../api/client'

interface NavItem {
  path: string
  label: string
  icon: JSX.Element
}

const primaryNavItems: NavItem[] = [
  {
    path: '/',
    label: 'Pipeline',
    icon: (
      <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
        <path strokeLinecap="round" strokeLinejoin="round" d="M5.25 5.653c0-.856.917-1.398 1.667-.986l11.54 6.348a1.125 1.125 0 010 1.971l-11.54 6.347a1.125 1.125 0 01-1.667-.985V5.653z" />
      </svg>
    ),
  },
  {
    path: '/batch',
    label: 'Batch',
    icon: (
      <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
        <path strokeLinecap="round" strokeLinejoin="round" d="M2.25 12.75V12A2.25 2.25 0 014.5 9.75h15A2.25 2.25 0 0121.75 12v.75m-8.69-6.44l-2.12-2.12a1.5 1.5 0 00-1.061-.44H4.5A2.25 2.25 0 002.25 6v12a2.25 2.25 0 002.25 2.25h15A2.25 2.25 0 0021.75 18V9a2.25 2.25 0 00-2.25-2.25h-5.379a1.5 1.5 0 01-1.06-.44z" />
      </svg>
    ),
  },
  {
    path: '/history',
    label: 'History',
    icon: (
      <svg className="w-5 h-5" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
        <path strokeLinecap="round" strokeLinejoin="round" d="M12 6v6h4.5m4.5 0a9 9 0 11-18 0 9 9 0 0118 0z" />
      </svg>
    ),
  },
]


function NavItemLink({ item }: { item: NavItem }) {
  return (
    <NavLink
      to={item.path}
      end={item.path === '/'}
      className={({ isActive }) =>
        `flex items-center gap-3 rounded-lg px-3 py-2.5 text-sm font-medium transition-colors duration-150 ${
          isActive
            ? 'bg-primary-600/20 text-primary-400 shadow-sm'
            : 'text-gray-400 hover:bg-gray-800 hover:text-gray-200'
        }`
      }
    >
      {item.icon}
      {item.label}
    </NavLink>
  )
}

// ---------------------------------------------------------------------------
// Connection status indicator
// ---------------------------------------------------------------------------
function ConnectionStatus() {
  const [status, setStatus] = useState<'checking' | 'connected' | 'disconnected'>('checking')

  useEffect(() => {
    const check = () => {
      getHealth()
        .then(() => setStatus('connected'))
        .catch(() => setStatus('disconnected'))
    }
    check()
    const timer = setInterval(check, 15000)
    return () => clearInterval(timer)
  }, [])

  const color =
    status === 'connected' ? 'bg-green-500' :
    status === 'disconnected' ? 'bg-red-500' : 'bg-yellow-500'
  const label =
    status === 'connected' ? 'Connected' :
    status === 'disconnected' ? 'Disconnected' : 'Checking...'

  return (
    <div className="flex items-center gap-2 px-3 py-1">
      <div className={`w-2 h-2 rounded-full ${color}`} />
      <span className="text-xs text-gray-500">{label}</span>
    </div>
  )
}

// ---------------------------------------------------------------------------
// Settings modal (backend URL)
// ---------------------------------------------------------------------------
function SettingsModal({ onClose }: { onClose: () => void }) {
  const [url, setUrl] = useState(getBackendUrl())
  const [testStatus, setTestStatus] = useState<'idle' | 'testing' | 'ok' | 'fail'>('idle')

  const handleTest = async () => {
    setTestStatus('testing')
    try {
      const cleaned = url.replace(/\/+$/, '')
      const res = await fetch(`${cleaned}/health`, { signal: AbortSignal.timeout(5000) })
      if (res.ok) setTestStatus('ok')
      else setTestStatus('fail')
    } catch {
      setTestStatus('fail')
    }
  }

  const handleSave = () => {
    setBackendUrl(url)
    window.location.reload()
  }

  const handleReset = () => {
    resetBackendUrl()
    window.location.reload()
  }

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60" onClick={onClose}>
      <div className="bg-gray-900 border border-gray-700 rounded-xl shadow-2xl w-full max-w-md mx-4 p-6" onClick={(e) => e.stopPropagation()}>
        <h3 className="text-lg font-semibold text-white mb-4">Backend Connection</h3>

        <label className="block text-xs text-gray-400 mb-1">Backend URL</label>
        <div className="flex gap-2 mb-2">
          <input
            type="text"
            value={url}
            onChange={(e) => { setUrl(e.target.value); setTestStatus('idle') }}
            className="flex-1 bg-gray-800 border border-gray-700 rounded-lg px-3 py-2 text-sm text-gray-200 placeholder-gray-500 focus:outline-none focus:border-primary-500"
            placeholder="http://localhost:3000/api"
          />
          <button onClick={handleTest} className="btn-secondary px-3 text-sm" disabled={testStatus === 'testing'}>
            {testStatus === 'testing' ? '...' : 'Test'}
          </button>
        </div>

        {testStatus === 'ok' && <p className="text-xs text-green-400 mb-3">Connection successful</p>}
        {testStatus === 'fail' && <p className="text-xs text-red-400 mb-3">Cannot reach backend at this URL</p>}

        <p className="text-xs text-gray-500 mb-4">
          To use FiberFoam online, run the Docker backend on your machine and enter its URL here.
          Default: <code className="text-gray-400">http://localhost:3000/api</code>
        </p>

        <div className="flex justify-between">
          <button onClick={handleReset} className="text-xs text-gray-500 hover:text-gray-300">Reset to default</button>
          <div className="flex gap-3">
            <button onClick={onClose} className="btn-secondary px-4">Cancel</button>
            <button onClick={handleSave} className="btn-primary px-4">Save & Reload</button>
          </div>
        </div>
      </div>
    </div>
  )
}

// ---------------------------------------------------------------------------
// Feedback modal
// ---------------------------------------------------------------------------
const CATEGORIES = ['Bug Report', 'Feature Request', 'Question', 'General']

function FeedbackModal({ onClose }: { onClose: () => void }) {
  const [category, setCategory] = useState('General')
  const [message, setMessage] = useState('')
  const [contact, setContact] = useState('')
  const [sending, setSending] = useState(false)
  const [sent, setSent] = useState(false)
  const [error, setError] = useState('')

  const handleSubmit = async () => {
    if (!message.trim()) return
    setSending(true)
    setError('')
    const payload = { category, message: message.trim(), contact: contact.trim() }
    try {
      // Try local backend first
      await submitFeedback(payload)
      setSent(true)
    } catch {
      // Fall back to Vercel serverless function
      try {
        const res = await fetch('/api/feedback', {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify(payload),
        })
        if (res.ok) setSent(true)
        else setError('Failed to submit feedback. Please try again.')
      } catch {
        setError('Failed to submit feedback. Please try again.')
      }
    } finally {
      setSending(false)
    }
  }

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/60" onClick={onClose}>
      <div className="bg-gray-900 border border-gray-700 rounded-xl shadow-2xl w-full max-w-md mx-4 p-6" onClick={(e) => e.stopPropagation()}>
        {sent ? (
          <div className="text-center py-4">
            <svg className="w-12 h-12 text-green-400 mx-auto mb-3" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
              <path strokeLinecap="round" strokeLinejoin="round" d="M9 12.75L11.25 15 15 9.75M21 12a9 9 0 11-18 0 9 9 0 0118 0z" />
            </svg>
            <h3 className="text-lg font-semibold text-white mb-1">Thank you!</h3>
            <p className="text-sm text-gray-400">Your feedback has been submitted.</p>
            <button onClick={onClose} className="mt-4 btn-primary px-6">Close</button>
          </div>
        ) : (
          <>
            <h3 className="text-lg font-semibold text-white mb-4">Send Feedback</h3>

            <label className="block text-xs text-gray-400 mb-1">Category</label>
            <div className="flex flex-wrap gap-2 mb-4">
              {CATEGORIES.map((c) => (
                <button
                  key={c}
                  onClick={() => setCategory(c)}
                  className={`px-3 py-1 rounded-full text-xs font-medium transition-colors ${
                    category === c
                      ? 'bg-primary-600 text-white'
                      : 'bg-gray-800 text-gray-400 hover:bg-gray-700'
                  }`}
                >
                  {c}
                </button>
              ))}
            </div>

            <label className="block text-xs text-gray-400 mb-1">Message *</label>
            <textarea
              value={message}
              onChange={(e) => setMessage(e.target.value)}
              rows={4}
              className="w-full bg-gray-800 border border-gray-700 rounded-lg px-3 py-2 text-sm text-gray-200 placeholder-gray-500 focus:outline-none focus:border-primary-500 mb-4 resize-none"
              placeholder="Describe your feedback, bug, or feature request..."
            />

            <label className="block text-xs text-gray-400 mb-1">Contact (optional)</label>
            <input
              type="text"
              value={contact}
              onChange={(e) => setContact(e.target.value)}
              className="w-full bg-gray-800 border border-gray-700 rounded-lg px-3 py-2 text-sm text-gray-200 placeholder-gray-500 focus:outline-none focus:border-primary-500 mb-4"
              placeholder="Email or name (so we can follow up)"
            />

            {error && <p className="text-sm text-red-400 mb-3">{error}</p>}

            <div className="flex justify-end gap-3">
              <button onClick={onClose} className="btn-secondary px-4">Cancel</button>
              <button
                onClick={handleSubmit}
                disabled={!message.trim() || sending}
                className="btn-primary px-4 disabled:opacity-50"
              >
                {sending ? 'Sending...' : 'Submit'}
              </button>
            </div>
          </>
        )}
      </div>
    </div>
  )
}

// ---------------------------------------------------------------------------
// Sidebar
// ---------------------------------------------------------------------------
export default function Sidebar() {
  const [showFeedback, setShowFeedback] = useState(false)
  const [showSettings, setShowSettings] = useState(false)

  return (
    <aside className="fixed inset-y-0 left-0 z-30 flex w-60 flex-col bg-gray-900 border-r border-gray-800">
      {/* Logo / Brand */}
      <div className="flex h-16 items-center gap-3 px-5 border-b border-gray-800">
        <div className="flex h-9 w-9 items-center justify-center rounded-lg bg-primary-600">
          <svg className="w-5 h-5 text-white" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={2}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M21 7.5l-9-5.25L3 7.5m18 0l-9 5.25m9-5.25v9l-9 5.25M3 7.5l9 5.25M3 7.5v9l9 5.25m0-9v9" />
          </svg>
        </div>
        <div>
          <h1 className="text-base font-bold text-white tracking-tight">FiberFoam</h1>
          <p className="text-[10px] text-gray-500 uppercase tracking-widest">Simulation Suite</p>
        </div>
      </div>

      {/* Navigation */}
      <nav className="flex-1 overflow-y-auto px-3 py-4">
        <div className="space-y-1">
          {primaryNavItems.map((item) => (
            <NavItemLink key={item.path} item={item} />
          ))}
        </div>
      </nav>

      {/* Footer */}
      <div className="border-t border-gray-800 px-4 py-3 space-y-1">
        <ConnectionStatus />
        <button
          onClick={() => setShowSettings(true)}
          className="flex items-center gap-2 rounded-lg px-3 py-2 text-sm text-gray-400 hover:bg-gray-800 hover:text-gray-200 transition-colors duration-150 w-full"
        >
          <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M9.594 3.94c.09-.542.56-.94 1.11-.94h2.593c.55 0 1.02.398 1.11.94l.213 1.281c.063.374.313.686.645.87.074.04.147.083.22.127.324.196.72.257 1.075.124l1.217-.456a1.125 1.125 0 011.37.49l1.296 2.247a1.125 1.125 0 01-.26 1.431l-1.003.827c-.293.24-.438.613-.431.992a6.759 6.759 0 010 .255c-.007.378.138.75.43.99l1.005.828c.424.35.534.954.26 1.43l-1.298 2.247a1.125 1.125 0 01-1.369.491l-1.217-.456c-.355-.133-.75-.072-1.076.124a6.57 6.57 0 01-.22.128c-.331.183-.581.495-.644.869l-.213 1.28c-.09.543-.56.941-1.11.941h-2.594c-.55 0-1.02-.398-1.11-.94l-.213-1.281c-.062-.374-.312-.686-.644-.87a6.52 6.52 0 01-.22-.127c-.325-.196-.72-.257-1.076-.124l-1.217.456a1.125 1.125 0 01-1.369-.49l-1.297-2.247a1.125 1.125 0 01.26-1.431l1.004-.827c.292-.24.437-.613.43-.992a6.932 6.932 0 010-.255c.007-.378-.138-.75-.43-.99l-1.004-.828a1.125 1.125 0 01-.26-1.43l1.297-2.247a1.125 1.125 0 011.37-.491l1.216.456c.356.133.751.072 1.076-.124.072-.044.146-.087.22-.128.332-.183.582-.495.644-.869l.214-1.281z" />
            <path strokeLinecap="round" strokeLinejoin="round" d="M15 12a3 3 0 11-6 0 3 3 0 016 0z" />
          </svg>
          Settings
        </button>
        <button
          onClick={() => setShowFeedback(true)}
          className="flex items-center gap-2 rounded-lg px-3 py-2 text-sm text-gray-400 hover:bg-gray-800 hover:text-gray-200 transition-colors duration-150 w-full"
        >
          <svg className="w-4 h-4" fill="none" viewBox="0 0 24 24" stroke="currentColor" strokeWidth={1.5}>
            <path strokeLinecap="round" strokeLinejoin="round" d="M7.5 8.25h9m-9 3H12m-9.75 1.51c0 1.6 1.123 2.994 2.707 3.227 1.129.166 2.27.293 3.423.379.35.026.67.21.865.501L12 21l2.755-4.133a1.14 1.14 0 01.865-.501 48.172 48.172 0 003.423-.379c1.584-.233 2.707-1.626 2.707-3.228V6.741c0-1.602-1.123-2.995-2.707-3.228A48.394 48.394 0 0012 3c-2.392 0-4.744.175-7.043.513C3.373 3.746 2.25 5.14 2.25 6.741v6.018z" />
          </svg>
          Feedback
        </button>
        <p className="text-xs text-gray-600 px-3">v0.1.0</p>
      </div>

      {showSettings && <SettingsModal onClose={() => setShowSettings(false)} />}
      {showFeedback && <FeedbackModal onClose={() => setShowFeedback(false)} />}
    </aside>
  )
}
