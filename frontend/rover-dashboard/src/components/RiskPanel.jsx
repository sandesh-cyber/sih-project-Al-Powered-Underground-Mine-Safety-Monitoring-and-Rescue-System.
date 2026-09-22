export default function RiskPanel({
  riskScore,
  riskLevel,
  riskReason,
  sensorConfidence,
}) {
  const getRiskClass = (level) => {
    const l = String(level || '').toUpperCase()
    if (l.includes('CRITICAL')) return 'critical'
    if (l.includes('HIGH')) return 'high'
    if (l.includes('WARN')) return 'warning'
    return 'safe'
  }

  const riskClass = getRiskClass(riskLevel)
  const score = typeof riskScore === 'number' ? Math.max(0, Math.min(100, riskScore)) : 0
  const scoreDisplay = typeof riskScore === 'number' ? riskScore : '--'
  const confidenceDisplay =
    sensorConfidence !== undefined && sensorConfidence !== null
      ? `${sensorConfidence}%`
      : 'Unavailable'

  return (
    <section className={`risk-panel ${riskClass}`}>
      <div className="risk-panel-inner">
        {/* Score and Level Badge */}
        <div className="risk-score-group">
          <div>
            <div className="risk-heading-label">
              <span className="material-symbols-outlined" style={{ fontSize: '20px' }}>
                warning
              </span>
              OVERALL SYSTEM RISK
            </div>
            <div className="risk-numeric-display">
              <span className={`risk-number ${riskClass}`}>{scoreDisplay}</span>
              <span className="risk-max">/ 100</span>
            </div>
          </div>

          <div style={{ display: 'flex', flexDirection: 'column', gap: '6px' }}>
            <span className={`risk-badge ${riskClass}`}>
              <span className="pulse-dot"></span>
              {riskLevel || 'SAFE'}
            </span>
            <span
              style={{
                fontSize: '0.75rem',
                color: '#94A3B8',
                fontFamily: 'var(--font-mono)',
                textTransform: 'uppercase',
              }}
            >
              Real-time ESP32 Assessment
            </span>
          </div>
        </div>

        {/* Center & Right: Diagnostic Reason & Threshold Gauge */}
        <div className="risk-details-group">
          <div>
            <span className="diagnostic-label">Incident Diagnostic</span>
            <p className="diagnostic-text">
              {riskReason || 'System operational — all environmental parameters normal'}
            </p>
          </div>

          <div className="gauge-track-container">
            <div className="gauge-threshold-labels">
              <span style={{ color: 'var(--color-safe)' }}>SAFE (0 - 30)</span>
              <span style={{ color: 'var(--color-warning)', fontWeight: 800 }}>
                WARN (31 - 60)
              </span>
              <span style={{ color: 'var(--color-high)' }}>HIGH (61 - 80)</span>
              <span style={{ color: 'var(--color-critical)' }}>
                CRITICAL (81 - 100)
              </span>
            </div>

            <div className="gauge-bar-frame">
              <div className="gauge-segment safe"></div>
              <div className="gauge-segment warning"></div>
              <div className="gauge-segment high"></div>
              <div className="gauge-segment critical"></div>
              <div
                className="gauge-pointer"
                style={{ left: `${score}%` }}
                title={`Risk Score: ${score}/100`}
              ></div>
            </div>

            <div className="gauge-footer">
              <span>
                Assessment: <strong className="mono-metric" style={{ color: '#F8FAFC' }}>{riskLevel || 'SAFE'}</strong>
              </span>
              <span>
                Sensor Confidence: <strong className="mono-metric" style={{ color: '#6EE7B7' }}>{confidenceDisplay}</strong>
              </span>
            </div>
          </div>
        </div>
      </div>
    </section>
  )
}
