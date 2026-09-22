import SensorCard from './SensorCard'

export default function EnvironmentalSensors({
  roverMQ4Change,
  helmetMQ4Change,
  smoke,
  water,
  distanceCm,
  distanceValid,
}) {
  const formatSlope = (val) => {
    if (val === undefined || val === null) return 'Unavailable'
    const sign = val >= 0 ? '+' : ''
    return `${sign}${val.toFixed(1)}% Δ`
  }

  const isRoverMQ4Elevated = roverMQ4Change !== undefined && roverMQ4Change > 5
  const isHelmetMQ4Elevated = helmetMQ4Change !== undefined && helmetMQ4Change > 5
  const isSmokeActive = Boolean(smoke)
  const isWaterActive = Boolean(water)
  const isNearObstacle = distanceValid && distanceCm !== undefined && distanceCm > 0 && distanceCm < 50

  return (
    <section style={{ display: 'flex', flexDirection: 'column', gap: '14px' }}>
      <div style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }}>
        <h2
          style={{
            fontSize: '1.125rem',
            fontWeight: 800,
            color: '#FFFFFF',
            textTransform: 'uppercase',
            letterSpacing: '0.04em',
            display: 'flex',
            alignItems: 'center',
            gap: '8px',
          }}
        >
          <span className="material-symbols-outlined" style={{ color: 'var(--accent-sky)' }}>
            sensors
          </span>
          ENVIRONMENTAL HAZARD SENSORS
        </h2>
        <span
          style={{
            fontSize: '0.75rem',
            fontFamily: 'var(--font-mono)',
            color: 'var(--text-secondary)',
            backgroundColor: 'var(--bg-panel)',
            padding: '4px 12px',
            borderRadius: '6px',
            border: '1px solid var(--border-color)',
          }}
        >
          REFRESH CYCLE: 1000MS
        </span>
      </div>

      <div className="grid-3-col">
        {/* Card 1: ROVER METHANE (MQ-4) */}
        <SensorCard
          title="ROVER METHANE (MQ-4)"
          badgeText={isRoverMQ4Elevated ? 'ELEVATED' : 'NORMAL'}
          badgeType={isRoverMQ4Elevated ? 'warning' : 'normal'}
          mainValue={formatSlope(roverMQ4Change)}
          valueColor={isRoverMQ4Elevated ? 'var(--color-warning)' : 'var(--color-safe)'}
          footerLeft="Sensor Delta Rate"
          footerRight={isRoverMQ4Elevated ? 'Slope Rising' : 'Gradient Stable'}
          cardClass={isRoverMQ4Elevated ? 'elevated' : ''}
        />

        {/* Card 2: HELMET METHANE (MQ-4) */}
        <SensorCard
          title="HELMET METHANE (MQ-4)"
          badgeText={isHelmetMQ4Elevated ? 'ELEVATED' : 'NORMAL'}
          badgeType={isHelmetMQ4Elevated ? 'warning' : 'normal'}
          mainValue={formatSlope(helmetMQ4Change)}
          valueColor={isHelmetMQ4Elevated ? 'var(--color-warning)' : 'var(--color-safe)'}
          footerLeft="Helmet Gas Delta"
          footerRight={isHelmetMQ4Elevated ? 'Elevated Δ' : 'Safe Margin'}
          cardClass={isHelmetMQ4Elevated ? 'elevated' : ''}
        />

        {/* Card 3: CO / MQ-7 (Carbon Monoxide) */}
        <SensorCard
          title="CO / MQ-7 (CARBON MONOXIDE)"
          badgeText="ACTIVE"
          badgeType="normal"
          mainValue="MONITORING"
          valueColor="#38BDF8"
          footerLeft="Carbon Monoxide Sensor"
          footerRight="Analog Channel Active"
        />

        {/* Card 4: OPTICAL SMOKE SENSOR */}
        <SensorCard
          title="OPTICAL SMOKE SENSOR"
          badgeText={isSmokeActive ? 'ALERT' : 'CLEAR'}
          badgeType={isSmokeActive ? 'danger' : 'normal'}
          mainValue={isSmokeActive ? 'SMOKE DETECTED' : 'CLEAR / NORMAL'}
          valueColor={isSmokeActive ? 'var(--color-warning)' : 'var(--color-safe)'}
          icon={isSmokeActive ? 'local_fire_department' : 'check_circle'}
          footerLeft="Optical Sensing Probe"
          footerRight={isSmokeActive ? 'Critical Trigger' : 'No Smoke Ingress'}
          cardClass={isSmokeActive ? 'alert' : ''}
        />

        {/* Card 5: WATER INGRESS */}
        <SensorCard
          title="WATER INGRESS"
          badgeText={isWaterActive ? 'ALERT' : 'CLEAR'}
          badgeType={isWaterActive ? 'danger' : 'normal'}
          mainValue={isWaterActive ? 'WATER DETECTED' : 'DRY / CLEAR'}
          valueColor={isWaterActive ? 'var(--color-critical)' : 'var(--color-safe)'}
          icon={isWaterActive ? 'water_drop' : 'water_drop'}
          footerLeft="Conductivity Probe"
          footerRight={isWaterActive ? 'Water Warning' : 'Dry Chassis'}
          cardClass={isWaterActive ? 'alert' : ''}
        />

        {/* Card 6: OBSTACLE DISTANCE */}
        <SensorCard
          title="OBSTACLE DISTANCE"
          badgeText={
            !distanceValid
              ? 'INVALID ECHO'
              : isNearObstacle
              ? 'OBJECT NEAR'
              : 'CLEAR PATH'
          }
          badgeType={
            !distanceValid
              ? 'warning'
              : isNearObstacle
              ? 'warning'
              : 'normal'
          }
          mainValue={
            distanceCm !== undefined && distanceCm !== null && distanceValid
              ? `${distanceCm.toFixed(1)} cm`
              : 'Out of Range'
          }
          valueColor={
            !distanceValid
              ? 'var(--text-muted)'
              : isNearObstacle
              ? 'var(--color-warning)'
              : '#F8FAFC'
          }
          footerLeft={distanceValid ? 'Ultrasonic Echo Valid' : 'Sensor Blind / Out of range'}
          footerRight="Ultrasonic Ranger"
          cardClass={isNearObstacle ? 'elevated' : ''}
        />
      </div>
    </section>
  )
}
