import Header from '../components/Header'
import RiskPanel from '../components/RiskPanel'
import MinerStatus from '../components/MinerStatus'
import CommunicationPanel from '../components/CommunicationPanel'
import EnvironmentalSensors from '../components/EnvironmentalSensors'
import CameraPanel from '../components/CameraPanel'
import AIRiskPanel from '../components/AIRiskPanel'
import RoverStatus from '../components/RoverStatus'

export default function Dashboard({ roverData, isConnected }) {
  const isSOS = Boolean(roverData?.helmetSOS)
  const isFall = Boolean(roverData?.helmetFall)
  const hasEmergency = isSOS || isFall

  return (
    <div className="dashboard-root">
      {/* Top Header */}
      <Header
        isConnected={isConnected}
        minerID={roverData?.minerID}
        helmetLink={roverData?.helmetLink}
      />

      {/* Main Operational Workspace */}
      <main className="dashboard-main">
        {/* Critical Emergency Banner if SOS or Fall is triggered */}
        {hasEmergency && (
          <div className="emergency-banner">
            <div className="emergency-banner-content">
              <span
                className="material-symbols-outlined"
                style={{ fontSize: '32px', color: '#EF4444' }}
              >
                emergency
              </span>
              <div>
                <h2 className="emergency-banner-title">
                  CRITICAL RESCUE ALERT —{' '}
                  {isSOS && isFall
                    ? 'SOS PANIC + MAN-DOWN FALL DETECTED'
                    : isSOS
                    ? 'SOS PANIC BUTTON ACTIVATED'
                    : 'MAN-DOWN FALL DETECTED'}
                </h2>
                <p className="emergency-banner-desc">
                  Immediate emergency response protocol active for {roverData?.minerID || 'Miner'}.
                </p>
              </div>
            </div>
            <div className="status-badge offline" style={{ animation: 'pulse-animation 1s infinite' }}>
              CRITICAL EMERGENCY
            </div>
          </div>
        )}

        {/* 1. Dominant System Risk Panel */}
        <RiskPanel
          riskScore={roverData?.riskScore}
          riskLevel={roverData?.riskLevel}
          riskReason={roverData?.riskReason}
          sensorConfidence={roverData?.sensorConfidence}
        />

        {/* 2. Top Row: Miner Status & RF Transport */}
        <section className="grid-2-col">
          <MinerStatus
            minerID={roverData?.minerID}
            helmetLink={roverData?.helmetLink}
            helmetSOS={roverData?.helmetSOS}
            helmetFall={roverData?.helmetFall}
            helmetMQ4Change={roverData?.helmetMQ4Change}
            sensorConfidence={roverData?.sensorConfidence}
            lastPacketAgeMs={roverData?.lastPacketAgeMs}
          />

          <CommunicationPanel
            helmetLink={roverData?.helmetLink}
            packetsReceived={roverData?.packetsReceived}
            missedPackets={roverData?.missedPackets}
            packetDeliveryPercent={roverData?.packetDeliveryPercent}
            lastPacketAgeMs={roverData?.lastPacketAgeMs}
          />
        </section>

        {/* 3. Middle Row: Environmental Hazard Sensors */}
        <EnvironmentalSensors
          roverMQ4Change={roverData?.roverMQ4Change}
          helmetMQ4Change={roverData?.helmetMQ4Change}
          smoke={roverData?.smoke}
          water={roverData?.water}
          distanceCm={roverData?.distanceCm}
          distanceValid={roverData?.distanceValid}
        />

        {/* 4. Lower Section: Camera + AI Engine + Rover Telemetry */}
        <section className="lower-grid">
          {/* Left Column: Live Rover Camera Placeholder */}
          <CameraPanel
            distanceCm={roverData?.distanceCm}
            distanceValid={roverData?.distanceValid}
          />

          {/* Right Column: AI Risk Engine & Hardware Core */}
          <div style={{ display: 'flex', flexDirection: 'column', gap: '16px' }}>
            <AIRiskPanel
              riskScore={roverData?.riskScore}
              riskLevel={roverData?.riskLevel}
              riskReason={roverData?.riskReason}
              sensorConfidence={roverData?.sensorConfidence}
              smoke={roverData?.smoke}
              water={roverData?.water}
              roverMQ4Change={roverData?.roverMQ4Change}
              distanceCm={roverData?.distanceCm}
              distanceValid={roverData?.distanceValid}
            />

            <RoverStatus
              isConnected={isConnected}
              roverVersion={roverData?.rover}
            />
          </div>
        </section>
      </main>
    </div>
  )
}
