export default function CommunicationPanel({
  helmetLink,
  packetsReceived,
  missedPackets,
  packetDeliveryPercent,
  lastPacketAgeMs,
}) {
  const isConnected = helmetLink === 'CONNECTED'
  const deliveryRate =
    packetDeliveryPercent !== undefined && packetDeliveryPercent !== null
      ? packetDeliveryPercent
      : null

  return (
    <div className="dashboard-panel">
      <div className="panel-header">
        <div className="panel-title-wrap">
          <span className="material-symbols-outlined panel-icon">
            wifi_tethering
          </span>
          <h2 className="panel-title">HELMET RF TRANSPORT</h2>
        </div>
        <span
          className={`status-badge ${isConnected ? 'online' : 'offline'}`}
          style={{ fontSize: '0.8125rem', padding: '4px 12px' }}
        >
          <span className="pulse-dot"></span>
          {isConnected ? 'GOOD - STABLE LINK' : 'HELMET LINK LOST'}
        </span>
      </div>

      <div className="metrics-grid-4">
        {/* Delivery Rate */}
        <div className="metric-card">
          <span className="metric-label">Delivery Rate</span>
          <div
            className="metric-value-lg"
            style={{
              color:
                deliveryRate !== null && deliveryRate >= 95
                  ? 'var(--color-safe)'
                  : deliveryRate !== null && deliveryRate >= 80
                  ? 'var(--color-warning)'
                  : 'var(--color-critical)',
            }}
          >
            {deliveryRate !== null ? `${deliveryRate.toFixed(1)}%` : 'Unavailable'}
          </div>
          <div
            style={{
              width: '100%',
              height: '4px',
              backgroundColor: 'var(--bg-nested)',
              borderRadius: '9999px',
              overflow: 'hidden',
              marginTop: '4px',
            }}
          >
            <div
              style={{
                width: `${deliveryRate !== null ? Math.max(0, Math.min(100, deliveryRate)) : 0}%`,
                height: '100%',
                backgroundColor:
                  deliveryRate !== null && deliveryRate >= 95
                    ? 'var(--color-safe)'
                    : 'var(--color-warning)',
              }}
            ></div>
          </div>
        </div>

        {/* Packets Received */}
        <div className="metric-card">
          <span className="metric-label">Packets Rx</span>
          <div className="metric-value-lg">
            {packetsReceived !== undefined && packetsReceived !== null
              ? packetsReceived.toLocaleString()
              : 'Unavailable'}
          </div>
          <span className="metric-subtext">Telemetry Packets</span>
        </div>

        {/* Missed Packets */}
        <div className="metric-card">
          <span className="metric-label">Missed Pkts</span>
          <div
            className="metric-value-lg"
            style={{
              color:
                missedPackets !== undefined && missedPackets > 10
                  ? 'var(--color-warning)'
                  : '#E2E8F0',
            }}
          >
            {missedPackets !== undefined && missedPackets !== null
              ? missedPackets.toLocaleString()
              : 'Unavailable'}
          </div>
          <span className="metric-subtext">Dropped Frames</span>
        </div>

        {/* Latency / Packet Age */}
        <div className="metric-card">
          <span className="metric-label">Last Latency</span>
          <div className="metric-value-lg">
            {lastPacketAgeMs !== undefined && lastPacketAgeMs !== null
              ? `${lastPacketAgeMs} ms`
              : 'Unavailable'}
          </div>
          <span className="metric-subtext">ESP-NOW Link Age</span>
        </div>
      </div>

      <div
        style={{
          backgroundColor: 'var(--bg-nested)',
          padding: '8px 14px',
          borderRadius: '6px',
          border: '1px solid var(--border-color)',
          display: 'flex',
          justifyContent: 'space-between',
          alignItems: 'center',
          fontSize: '0.75rem',
          color: 'var(--text-secondary)',
          fontFamily: 'var(--font-mono)',
        }}
      >
        <span>PROTOCOL: ESP-NOW (2.4 GHz)</span>
        <span>NODE: HELMET_TRANSMITTER</span>
      </div>
    </div>
  )
}
