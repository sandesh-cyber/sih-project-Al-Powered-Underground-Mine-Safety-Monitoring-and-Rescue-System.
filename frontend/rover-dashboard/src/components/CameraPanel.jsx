import { useState, useEffect, useRef } from 'react'

const CAMERA_URL = 'http://192.168.4.2/frame'
const WIDTH = 320
const HEIGHT = 240
const FRAME_SIZE = 153600 // 320 * 240 * 2 bytes
const ZOOM_LEVELS = [1.0, 1.5, 2.0, 3.0]

export default function CameraPanel({ distanceCm, distanceValid }) {
  const [cameraStatus, setCameraStatus] = useState('CONNECTING') // 'CONNECTING' | 'LIVE' | 'OFFLINE'
  const [latencyMs, setLatencyMs] = useState(null)
  const [zoomIndex, setZoomIndex] = useState(0) // Default 1.0x (index 0)

  const canvasRef = useRef(null)
  const offscreenCanvasRef = useRef(null)
  const lastImageDataRef = useRef(null)
  const zoomRef = useRef(1.0)

  const distText =
    distanceValid && distanceCm !== undefined && distanceCm !== null
      ? `${distanceCm.toFixed(1)} cm`
      : '--'

  const drawFrameWithZoom = (imageData, zoom) => {
    const canvas = canvasRef.current
    if (!canvas) return
    const ctx = canvas.getContext('2d')
    if (!ctx) return

    if (!offscreenCanvasRef.current) {
      offscreenCanvasRef.current = document.createElement('canvas')
      offscreenCanvasRef.current.width = WIDTH
      offscreenCanvasRef.current.height = HEIGHT
    }

    const offscreen = offscreenCanvasRef.current
    const offCtx = offscreen.getContext('2d')
    if (offCtx) {
      offCtx.putImageData(imageData, 0, 0)

      if (zoom === 1.0) {
        ctx.drawImage(offscreen, 0, 0, WIDTH, HEIGHT)
      } else {
        const cropWidth = WIDTH / zoom
        const cropHeight = HEIGHT / zoom
        const cropX = (WIDTH - cropWidth) / 2
        const cropY = (HEIGHT - cropHeight) / 2
        ctx.imageSmoothingEnabled = true
        ctx.imageSmoothingQuality = 'high'
        ctx.drawImage(offscreen, cropX, cropY, cropWidth, cropHeight, 0, 0, WIDTH, HEIGHT)
      }
    }
  }

  const handleZoomIn = () => {
    setZoomIndex((prev) => {
      if (prev < ZOOM_LEVELS.length - 1) {
        const next = prev + 1
        const newZoom = ZOOM_LEVELS[next]
        zoomRef.current = newZoom
        if (lastImageDataRef.current) {
          drawFrameWithZoom(lastImageDataRef.current, newZoom)
        }
        return next
      }
      return prev
    })
  }

  const handleZoomOut = () => {
    setZoomIndex((prev) => {
      if (prev > 0) {
        const next = prev - 1
        const newZoom = ZOOM_LEVELS[next]
        zoomRef.current = newZoom
        if (lastImageDataRef.current) {
          drawFrameWithZoom(lastImageDataRef.current, newZoom)
        }
        return next
      }
      return prev
    })
  }

  useEffect(() => {
    let isMounted = true
    let timeoutId = null
    let abortController = null

    const fetchFrame = async () => {
      if (!isMounted) return

      const startTime = performance.now()
      abortController = new AbortController()

      try {
        const response = await fetch(`${CAMERA_URL}?t=${Date.now()}`, {
          cache: 'no-store',
          signal: abortController.signal,
        })

        if (!response.ok) {
          throw new Error(`HTTP ${response.status}`)
        }

        const buffer = await response.arrayBuffer()
        if (!isMounted) return

        if (buffer.byteLength === FRAME_SIZE) {
          const view = new Uint8Array(buffer)
          const rgba = new Uint8ClampedArray(WIDTH * HEIGHT * 4)

          for (let i = 0, p = 0; i < view.length; i += 2, p++) {
            const pixel = (view[i] << 8) | view[i + 1]

            const r = (((pixel >> 11) & 0x1f) * 255) / 31
            const g = (((pixel >> 5) & 0x3f) * 255) / 63
            const b = ((pixel & 0x1f) * 255) / 31

            rgba[p * 4] = r
            rgba[p * 4 + 1] = g
            rgba[p * 4 + 2] = b
            rgba[p * 4 + 3] = 255
          }

          const imageData = new ImageData(rgba, WIDTH, HEIGHT)
          lastImageDataRef.current = imageData

          drawFrameWithZoom(imageData, zoomRef.current)

          const elapsed = Math.round(performance.now() - startTime)
          setLatencyMs(elapsed)
          setCameraStatus('LIVE')

          // Request next frame continuously
          if (isMounted) {
            timeoutId = setTimeout(fetchFrame, 30)
          }
        } else {
          throw new Error(`Invalid frame size: ${buffer.byteLength}`)
        }
      } catch (err) {
        if (err.name === 'AbortError') return
        if (!isMounted) return

        setCameraStatus('OFFLINE')
        // Automatically retry after 1 second if offline
        timeoutId = setTimeout(fetchFrame, 1000)
      }
    }

    fetchFrame()

    return () => {
      isMounted = false
      if (abortController) {
        abortController.abort()
      }
      if (timeoutId) {
        clearTimeout(timeoutId)
      }
    }
  }, [])

  const currentZoom = ZOOM_LEVELS[zoomIndex]

  return (
    <div className="dashboard-panel" style={{ height: '100%' }}>
      {/* Header with Title, Digital Zoom Controls, and Status Badge */}
      <div className="panel-header">
        <div className="panel-title-wrap">
          <span className="material-symbols-outlined panel-icon">
            videocam
          </span>
          <h2 className="panel-title">LIVE ROVER CAMERA</h2>
        </div>

        <div style={{ display: 'flex', alignItems: 'center', gap: '10px', flexWrap: 'wrap' }}>
          {/* Digital Zoom Controls */}
          <div className="zoom-controls-wrapper">
            <span className="zoom-label">DIGITAL ZOOM</span>
            <div className="zoom-btn-group">
              <button
                type="button"
                className="zoom-btn"
                onClick={handleZoomOut}
                disabled={zoomIndex === 0}
                aria-label="Zoom out"
              >
                −
              </button>
              <span className="zoom-value">{currentZoom.toFixed(1)}x</span>
              <button
                type="button"
                className="zoom-btn"
                onClick={handleZoomIn}
                disabled={zoomIndex === ZOOM_LEVELS.length - 1}
                aria-label="Zoom in"
              >
                +
              </button>
            </div>
          </div>

          {/* Real Dynamic Camera Status Badge */}
          {cameraStatus === 'LIVE' ? (
            <span
              className="status-badge online"
              style={{
                fontSize: '0.75rem',
                padding: '4px 10px',
              }}
            >
              <span className="pulse-dot"></span>
              LIVE
            </span>
          ) : cameraStatus === 'CONNECTING' ? (
            <span
              className="status-badge"
              style={{
                background: 'rgba(245, 158, 11, 0.2)',
                border: '1px solid rgba(245, 158, 11, 0.5)',
                color: '#FCD34D',
                fontSize: '0.75rem',
                padding: '4px 10px',
              }}
            >
              <span className="pulse-dot"></span>
              CONNECTING
            </span>
          ) : (
            <span
              className="status-badge offline"
              style={{
                fontSize: '0.75rem',
                padding: '4px 10px',
              }}
            >
              <span className="pulse-dot"></span>
              OFFLINE
            </span>
          )}
        </div>
      </div>

      {/* Video Viewport with Canvas */}
      <div className="camera-viewport">
        {/* Canvas for Raw RGB565 frame rendering */}
        <canvas
          ref={canvasRef}
          width={WIDTH}
          height={HEIGHT}
          className="camera-canvas"
          style={{
            display: cameraStatus === 'LIVE' ? 'block' : 'none',
          }}
        />

        {/* Grid lines background when offline/connecting */}
        {cameraStatus !== 'LIVE' && <div className="camera-grid-lines"></div>}

        {/* HUD Elements */}
        <div className="camera-hud-badge camera-hud-tl">
          GC2145 ESP32-CAM | 320x240 RGB565
        </div>
        <div className="camera-hud-badge camera-hud-tr">
          DIST PROX: {distText}
        </div>
        <div className="camera-hud-badge camera-hud-bl">
          http://192.168.4.2/frame
        </div>
        <div className="camera-hud-badge camera-hud-br">
          {cameraStatus === 'LIVE'
            ? `LATENCY: ${latencyMs !== null ? `${latencyMs} ms` : '--'}`
            : `STATUS: ${cameraStatus}`}
        </div>

        {/* Overlay when connecting or offline */}
        {cameraStatus === 'CONNECTING' && (
          <div className="camera-pending-overlay">
            <span className="material-symbols-outlined" style={{ color: 'var(--color-warning)' }}>
              hourglass_top
            </span>
            <span>CONNECTING CAMERA...</span>
          </div>
        )}

        {cameraStatus === 'OFFLINE' && (
          <div
            className="camera-pending-overlay"
            style={{ borderColor: 'var(--color-critical)' }}
          >
            <span className="material-symbols-outlined" style={{ color: 'var(--color-critical)' }}>
              videocam_off
            </span>
            <span>CAMERA OFFLINE — RETRYING</span>
          </div>
        )}
      </div>

      {/* Footer Details */}
      <div
        style={{
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          fontSize: '0.75rem',
          color: 'var(--text-secondary)',
          paddingTop: '4px',
        }}
      >
        <span>Sensor: GC2145 (RGB565)</span>
        <span>
          Zoom: <strong style={{ color: 'var(--accent-sky)' }}>{currentZoom.toFixed(1)}x</strong> | Stream: <strong style={{ color: 'var(--accent-sky)' }}>Latest-Frame Web (320x240)</strong>
        </span>
      </div>
    </div>
  )
}
