export default function MinerStatus({
  minerID,
  helmetLink,
  helmetSOS,
  helmetFall,
  helmetMQ4Change,
  sensorConfidence,
  lastPacketAgeMs,
}) {
  const isHelmetConnected = helmetLink === 'CONNECTED'

  const formatSlope = (val) => {
    if (val === undefined || val === null) return 'Unavailable'
    const sign = val >= 0 ? '+' : ''
    return `${sign}${val.toFixed(1)}% Δ`
  }

  return (
    <div className="dashboard-panel">
      <div className="panel-header">
        <div className="panel-title-wrap">
          <span className="material-symbols-outlined panel-icon">
            sports_motorsports
          </span>
          <h2 className="panel-title">
            MINER STATUS — {minerID || 'MINER-01'}
          </h2>
        </div>
        <span
          className={`status-badge ${isHelmetConnected ? 'online' : 'offline'}`}
          style={{ fontSize: '0.8125rem', padding: '4px 12px' }}
        >
          <span className="pulse-dot"></span>
          {isHelmetConnected ? 'ONLINE' : 'LINK LOST'}
        </span>
      </div>

      <div className="metrics-grid-3">
        {/* SOS State */}
        <div className={`metric-card ${helmetSOS ? 'emergency' : ''}`}>
          <span className="metric-label">SOS Panic State</span>
          <div>
            <span className={`pill-badge ${helmetSOS ? 'danger' : 'normal'}`}>
              <span className="material-symbols-outlined" style={{ fontSize: '18px' }}>
                {helmetSOS ? 'emergency' : 'verified'}
              </span>
              {helmetSOS ? 'CRITICAL EMERGENCY' : 'NORMAL'}
            </span>
          </div>
          <span className="metric-subtext">
            {helmetSOS ? 'Panic Button Triggered' : 'Button: Disengaged'}
          </span>
        </div>

        {/* Fall Detection */}
        <div className={`metric-card ${helmetFall ? 'emergency' : ''}`}>
          <span className="metric-label">Fall Detection</span>
          <div>
            <span className={`pill-badge ${helmetFall ? 'danger' : 'normal'}`}>
              <span className="material-symbols-outlined" style={{ fontSize: '18px' }}>
                {helmetFall ? 'warning' : 'check_circle'}
              </span>
              {helmetFall ? 'CRITICAL EMERGENCY' : 'NORMAL'}
            </span>
          </div>
          <span className="metric-subtext">
            {helmetFall ? 'Man-Down Detected' : 'Motion: Stable'}
          </span>
        </div>

        {/* Helmet Methane Slope */}
        <div className="metric-card">
          <span className="metric-label">Helmet CH4 Slope</span>
          <div
            className="metric-value-lg"
            style={{
              color:
                helmetMQ4Change !== undefined && Math.abs(helmetMQ4Change) > 5
                  ? 'var(--color-warning)'
                  : 'var(--color-safe)',
            }}
          >
            {formatSlope(helmetMQ4Change)}
          </div>
          <span className="metric-subtext">MQ-4 Gas Gradient</span>
        </div>

        {/* Sensor Confidence */}
        <div className="metric-card">
          <span className="metric-label">Sensor Confidence</span>
          <div className="metric-value-lg" style={{ color: '#38BDF8' }}>
            {sensorConfidence !== undefined && sensorConfidence !== null
              ? `${sensorConfidence}%`
              : 'Unavailable'}
          </div>
          <span className="metric-subtext">Cross-Validation State</span>
        </div>

        {/* Last Packet Age */}
        <div className="metric-card">
          <span className="metric-label">Last Packet Age</span>
          <div className="metric-value-lg">
            {lastPacketAgeMs !== undefined && lastPacketAgeMs !== null
              ? `${lastPacketAgeMs} ms`
              : 'Unavailable'}
          </div>
          <span className="metric-subtext">ESP-NOW Telemetry Delay</span>
        </div>

        {/* Biometrics Note (No fake values) */}
        <div className="metric-card">
          <span className="metric-label">Biometric Heart Rate</span>
          <div
            style={{
              fontSize: '0.875rem',
              color: 'var(--text-muted)',
              fontStyle: 'italic',
              marginTop: '4px',
            }}
          >
            Unavailable in Firmware
          </div>
          <span className="metric-subtext">Pulse sensor unprovisioned</span>
        </div>
      </div>
    </div>
  )
}
