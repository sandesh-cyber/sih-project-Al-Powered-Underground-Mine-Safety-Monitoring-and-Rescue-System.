import { useState, useEffect } from 'react'

export default function Header({ isConnected, minerID, helmetLink }) {
  const [clocks, setClocks] = useState({ utc: '--:--:--', local: '--:--:--' })

  useEffect(() => {
    const updateTime = () => {
      const now = new Date()
      const utc = now.toUTCString().split(' ')[4] || ''
      const local = now.toTimeString().split(' ')[0] || ''
      setClocks({ utc, local })
    }

    updateTime()
    const timer = setInterval(updateTime, 1000)
    return () => clearInterval(timer)
  }, [])

  const isHelmetConnected = helmetLink === 'CONNECTED'

  return (
    <header className="top-header">
      <div className="header-container">
        {/* Title & Subtitle */}
        <div className="header-brand">
          <div className="brand-icon-box">
            <span className="material-symbols-outlined">emergency</span>
          </div>
          <div>
            <h1 className="brand-title">SMART MINE RESCUE SYSTEM</h1>
            <p className="brand-subtitle">
              AI-Powered Mine Safety &amp; Rescue Control Center
            </p>
          </div>
        </div>

        {/* Live Status Indicators & Clocks */}
        <div className="header-badges">
          {/* Rover Connection Badge */}
          <div
            className={`status-badge ${isConnected ? 'online' : 'offline'}`}
          >
            <span className="pulse-dot"></span>
            <span>{isConnected ? 'ROVER ONLINE' : 'ROVER OFFLINE'}</span>
          </div>

          {/* Miner ID Badge */}
          <div className="status-badge neutral">
            <span
              className="material-symbols-outlined"
              style={{ fontSize: '18px', color: '#38BDF8' }}
            >
              person
            </span>
            <span>{minerID || 'MINER-01'}</span>
          </div>

          {/* Helmet Connected Badge */}
          <div
            className={`status-badge ${isHelmetConnected ? 'online' : 'offline'}`}
          >
            <span className="pulse-dot"></span>
            <span>
              {isHelmetConnected ? 'HELMET CONNECTED' : 'HELMET LINK LOST'}
            </span>
          </div>

          {/* UTC & Local Clocks */}
          <div className="clocks-box">
            <span className="clock-label">UTC</span>
            <span>{clocks.utc}</span>
            <span className="clock-divider">|</span>
            <span className="clock-label">LOCAL</span>
            <span className="clock-val-local">{clocks.local}</span>
          </div>
        </div>
      </div>
    </header>
  )
}
