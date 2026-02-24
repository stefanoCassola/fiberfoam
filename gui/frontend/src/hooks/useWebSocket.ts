import { useEffect, useRef, useState, useCallback } from 'react'

export interface LogLine {
  timestamp: string
  level: 'info' | 'warning' | 'error' | 'success'
  message: string
}

interface UseWebSocketOptions {
  /** WebSocket endpoint path, e.g. "/ws/logs" */
  url: string
  /** Whether to auto-connect on mount. Default: false */
  autoConnect?: boolean
  /** Max lines to keep in the buffer. Default: 2000 */
  maxLines?: number
  /** Reconnect delay in ms after unexpected close. Default: 3000 */
  reconnectDelay?: number
}

interface UseWebSocketReturn {
  /** The accumulated log lines */
  lines: LogLine[]
  /** Whether the socket is currently open */
  connected: boolean
  /** Manually open the connection */
  connect: () => void
  /** Manually close the connection */
  disconnect: () => void
  /** Clear the line buffer */
  clear: () => void
}

function parseLogLevel(raw: string): LogLine['level'] {
  const lower = raw.toLowerCase()
  if (lower.includes('error') || lower.includes('fatal')) return 'error'
  if (lower.includes('warn')) return 'warning'
  if (lower.includes('success') || lower.includes('done') || lower.includes('finished'))
    return 'success'
  return 'info'
}

function parseMessage(data: string): LogLine {
  try {
    const parsed = JSON.parse(data)
    return {
      timestamp: parsed.timestamp ?? new Date().toISOString(),
      level: parsed.level ?? parseLogLevel(parsed.message ?? ''),
      message: parsed.message ?? data,
    }
  } catch {
    // Plain-text message
    return {
      timestamp: new Date().toISOString(),
      level: parseLogLevel(data),
      message: data,
    }
  }
}

export function useWebSocket({
  url,
  autoConnect = false,
  maxLines = 2000,
  reconnectDelay = 3000,
}: UseWebSocketOptions): UseWebSocketReturn {
  const [lines, setLines] = useState<LogLine[]>([])
  const [connected, setConnected] = useState(false)
  const wsRef = useRef<WebSocket | null>(null)
  const reconnectTimer = useRef<ReturnType<typeof setTimeout> | null>(null)
  const intentionalClose = useRef(false)

  const cleanup = useCallback(() => {
    if (reconnectTimer.current) {
      clearTimeout(reconnectTimer.current)
      reconnectTimer.current = null
    }
    if (wsRef.current) {
      wsRef.current.close()
      wsRef.current = null
    }
  }, [])

  const connect = useCallback(() => {
    cleanup()
    intentionalClose.current = false

    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:'
    const wsUrl = `${protocol}//${window.location.host}${url}`
    const ws = new WebSocket(wsUrl)

    ws.onopen = () => {
      setConnected(true)
    }

    ws.onmessage = (event: MessageEvent) => {
      const logLine = parseMessage(event.data)
      setLines((prev) => {
        const next = [...prev, logLine]
        return next.length > maxLines ? next.slice(next.length - maxLines) : next
      })
    }

    ws.onclose = () => {
      setConnected(false)
      wsRef.current = null
      if (!intentionalClose.current && reconnectDelay > 0) {
        reconnectTimer.current = setTimeout(() => {
          connect()
        }, reconnectDelay)
      }
    }

    ws.onerror = () => {
      // onclose will fire after onerror, triggering reconnect
    }

    wsRef.current = ws
  }, [url, maxLines, reconnectDelay, cleanup])

  const disconnect = useCallback(() => {
    intentionalClose.current = true
    cleanup()
    setConnected(false)
  }, [cleanup])

  const clear = useCallback(() => {
    setLines([])
  }, [])

  useEffect(() => {
    if (autoConnect) {
      connect()
    }
    return () => {
      intentionalClose.current = true
      cleanup()
    }
  }, [autoConnect, connect, cleanup])

  return { lines, connected, connect, disconnect, clear }
}
