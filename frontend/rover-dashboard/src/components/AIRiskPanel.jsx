export default function AIRiskPanel({
  riskScore,
  riskLevel,
  riskReason,
  sensorConfidence,
  smoke,
  water,
  roverMQ4Change,
  distanceCm,
  distanceValid,
}) {
  const getRiskColor = (lvl) => {
    const l = String(lvl || '').toUpperCase()
    if (l.includes('CRITICAL')) return 'var(--color-critical)'
    if (l.includes('HIGH')) return 'var(--color-high)'
    if (l.includes('WARN')) return 'var(--color-warning)'
    return 'var(--color-safe)'
  }

  const confidenceText =
    sensorConfidence !== undefined && sensorConfidence !== null
      ? `${sensorConfidence}% — Cross-Validated`
      : 'Unavailable'

  const isSmokeTriggered = Boolean(smoke)
  const isWaterTriggered = Boolean(water)
  const isGasElevated = roverMQ4Change !== undefined && roverMQ4Change > 5
  const isObstacleClose = distanceValid && distanceCm !== undefined && distanceCm > 0 && distanceCm < 50

  return (
    <div className="dashboard-panel" style={{ flex: 1, justifyContent: 'space-between' }}>
      <div className="panel-header">
        <div className="panel-title-wrap">
          <span className="material-symbols-outlined panel-icon">psychology</span>
          <h2 className="panel-title">AI RISK ASSESSMENT ENGINE</h2>
        </div>
        {riskScore !== undefined && riskScore !== null && (
          <span
            style={{
              fontSize: '0.8125rem',
              fontFamily: 'var(--font-mono)',
              fontWeight: 700,
              color: getRiskColor(riskLevel),
            }}
          >
            SCORE: {riskScore}/100
          </span>
        )}
      </div>

      {/* Confidence & Primary Factor */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        <div
          style={{
            display: 'flex',
            justifyContent: 'space-between',
            alignItems: 'center',
            fontSize: '0.9375rem',
          }}
        >
          <span style={{ color: 'var(--text-secondary)', fontWeight: 500 }}>
            Sensor Confidence:
          </span>
          <span
            style={{
              color: 'var(--color-safe)',
              fontWeight: 700,
              fontFamily: 'var(--font-mono)',
            }}
          >
            {confidenceText}
          </span>
        </div>

        <div
          style={{
            backgroundColor: 'var(--bg-card)',
            border: '1px solid rgba(245, 158, 11, 0.4)',
            padding: '12px 14px',
            borderRadius: '8px',
          }}
        >
          <span
            style={{
              fontSize: '0.75rem',
              textTransform: 'uppercase',
              letterSpacing: '0.05em',
              color: 'var(--text-secondary)',
              fontWeight: 700,
              display: 'block',
              marginBottom: '2px',
            }}
          >
            Primary Risk Factor
          </span>
          <span
            style={{
              color: '#FCD34D',
              fontWeight: 600,
              fontSize: '1rem',
            }}
          >
            {riskReason || 'All telemetry normal — no active hazards detected'}
          </span>
        </div>
      </div>

      {/* Active Diagnostics Overview */}
      <div style={{ display: 'flex', flexDirection: 'column', gap: '8px' }}>
        <span
          style={{
            fontSize: '0.75rem',
            textTransform: 'uppercase',
            letterSpacing: '0.05em',
            color: 'var(--text-secondary)',
            fontWeight: 700,
          }}
        >
          Active Subsystem Evaluators
        </span>

        <ul style={{ display: 'flex', flexDirection: 'column', gap: '8px', listStyle: 'none' }}>
          <li className="ai-factor-item">
            <span style={{ display: 'flex', alignItems: 'center', gap: '8px', color: '#E2E8F0' }}>
              <span
                style={{
                  width: '6px',
                  height: '6px',
                  borderRadius: '50%',
                  backgroundColor: isSmokeTriggered ? 'var(--color-critical)' : 'var(--color-safe)',
                }}
              ></span>
              Smoke Ingress Check
            </span>
            <span
              style={{
                fontFamily: 'var(--font-mono)',
                fontWeight: 700,
                color: isSmokeTriggered ? 'var(--color-critical)' : 'var(--color-safe)',
              }}
            >
              {isSmokeTriggered ? 'TRIGGERED' : 'CLEAR'}
            </span>
          </li>

          <li className="ai-factor-item">
            <span style={{ display: 'flex', alignItems: 'center', gap: '8px', color: '#E2E8F0' }}>
              <span
                style={{
                  width: '6px',
                  height: '6px',
                  borderRadius: '50%',
                  backgroundColor: isGasElevated ? 'var(--color-warning)' : 'var(--color-safe)',
                }}
              ></span>
              Methane Gradient (MQ-4)
            </span>
            <span
              style={{
                fontFamily: 'var(--font-mono)',
                fontWeight: 700,
                color: isGasElevated ? 'var(--color-warning)' : 'var(--color-safe)',
              }}
            >
              {isGasElevated ? 'ELEVATED' : 'NOMINAL'}
            </span>
          </li>

          <li className="ai-factor-item">
            <span style={{ display: 'flex', alignItems: 'center', gap: '8px', color: '#E2E8F0' }}>
              <span
                style={{
                  width: '6px',
                  height: '6px',
                  borderRadius: '50%',
                  backgroundColor: isObstacleClose ? 'var(--color-warning)' : 'var(--color-safe)',
                }}
              ></span>
              Obstacle Standoff Distance
            </span>
            <span
              style={{
                fontFamily: 'var(--font-mono)',
                fontWeight: 700,
                color: isObstacleClose ? 'var(--color-warning)' : 'var(--color-safe)',
              }}
            >
              {isObstacleClose ? '< 50 CM' : 'CLEAR'}
            </span>
          </li>

          {isWaterTriggered && (
            <li className="ai-factor-item">
              <span style={{ display: 'flex', alignItems: 'center', gap: '8px', color: '#E2E8F0' }}>
                <span
                  style={{
                    width: '6px',
                    height: '6px',
                    borderRadius: '50%',
                    backgroundColor: 'var(--color-critical)',
                  }}
                ></span>
                Water Ingress Sensor
              </span>
              <span
                style={{
                  fontFamily: 'var(--font-mono)',
                  fontWeight: 700,
                  color: 'var(--color-critical)',
                }}
              >
                DETECTED
              </span>
            </li>
          )}
        </ul>
      </div>

      {/* Operational Directive Banner */}
      <div className="directive-banner">
        <span
          className="material-symbols-outlined"
          style={{ color: getRiskColor(riskLevel), fontSize: '20px', flexShrink: 0, marginTop: '2px' }}
        >
          report_problem
        </span>
        <p style={{ lineHeight: '1.4' }}>
          <strong>OPERATIONAL DIRECTIVE:</strong>{' '}
          {riskLevel === 'CRITICAL' || riskLevel === 'HIGH'
            ? 'Maintain standoff posture. Alert rescue dispatch and verify ventilation controls immediately.'
            : riskLevel === 'WARNING'
            ? 'Caution advised. Monitor rising sensor gradients and verify rover positioning.'
            : 'Conditions within standard safe operational boundaries. Continue exploration.'}
        </p>
      </div>
    </div>
  )
}
