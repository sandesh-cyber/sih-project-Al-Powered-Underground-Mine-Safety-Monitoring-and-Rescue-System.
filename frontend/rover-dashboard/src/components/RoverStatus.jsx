export default function RoverStatus({ isConnected, roverVersion }) {
  return (
    <div className="dashboard-panel" style={{ padding: '16px 18px' }}>
      <div
        style={{
          fontSize: '0.75rem',
          textTransform: 'uppercase',
          letterSpacing: '0.05em',
          color: 'var(--text-secondary)',
          fontWeight: 700,
          marginBottom: '10px',
          display: 'flex',
          alignItems: 'center',
          gap: '6px',
        }}
      >
        <span
          className="material-symbols-outlined"
          style={{ fontSize: '18px', color: 'var(--accent-sky)' }}
        >
          developer_board
        </span>
        ROVER HARDWARE CORE TELEMETRY
      </div>

      <div className="rover-core-grid">
        <div className="rover-core-cell">
          <span className="rover-core-label">ROVER STATUS</span>
          <span
            className="rover-core-val"
            style={{
              color: isConnected ? 'var(--color-safe)' : 'var(--color-critical)',
            }}
          >
            {isConnected ? 'ONLINE' : 'OFFLINE'}
          </span>
        </div>

        <div className="rover-core-cell">
          <span className="rover-core-label">WI-FI AP</span>
          <span className="rover-core-val" style={{ color: '#38BDF8' }}>
            MINE-ROVER
          </span>
        </div>

        <div className="rover-core-cell">
          <span className="rover-core-label">API ENDPOINT</span>
          <span className="rover-core-val" style={{ color: 'var(--color-safe)' }}>
            192.168.4.1/data
          </span>
        </div>

        <div className="rover-core-cell">
          <span className="rover-core-label">CHASSIS STATE</span>
          <span className="rover-core-val" style={{ color: '#E2E8F0' }}>
            {roverVersion || 'ROVER-12.1'}
          </span>
        </div>
      </div>
    </div>
  )
}
