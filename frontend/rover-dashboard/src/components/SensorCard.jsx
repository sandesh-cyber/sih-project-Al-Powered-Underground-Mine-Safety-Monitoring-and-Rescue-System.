export default function SensorCard({
  title,
  badgeText,
  badgeType = 'normal',
  mainValue,
  valueColor,
  icon,
  footerLeft,
  footerRight,
  cardClass = '',
}) {
  const getBadgeStyle = () => {
    switch (badgeType) {
      case 'danger':
        return {
          background: 'rgba(127, 29, 29, 0.7)',
          border: '1px solid rgba(239, 68, 68, 0.7)',
          color: '#FCA5A5',
        }
      case 'warning':
        return {
          background: 'rgba(245, 158, 11, 0.2)',
          border: '1px solid rgba(245, 158, 11, 0.5)',
          color: '#FCD34D',
        }
      case 'normal':
      default:
        return {
          background: 'rgba(6, 78, 59, 0.6)',
          border: '1px solid rgba(16, 185, 129, 0.4)',
          color: '#6EE7B7',
        }
    }
  }

  return (
    <div className={`sensor-card ${cardClass}`}>
      <div className="sensor-card-header">
        <span className="sensor-name">{title}</span>
        {badgeText && (
          <span
            style={{
              padding: '4px 10px',
              borderRadius: '9999px',
              fontSize: '0.75rem',
              fontWeight: 700,
              textTransform: 'uppercase',
              letterSpacing: '0.03em',
              ...getBadgeStyle(),
            }}
          >
            {badgeText}
          </span>
        )}
      </div>

      <div className="sensor-value-main" style={{ color: valueColor || '#FFFFFF' }}>
        {icon && (
          <span className="material-symbols-outlined" style={{ fontSize: '28px' }}>
            {icon}
          </span>
        )}
        <span>{mainValue}</span>
      </div>

      <div className="sensor-footer">
        <span style={{ color: 'var(--text-secondary)' }}>{footerLeft}</span>
        <span
          style={{
            color: badgeType === 'danger' ? 'var(--color-critical)' : badgeType === 'warning' ? 'var(--color-warning)' : 'var(--text-muted)',
            fontFamily: 'var(--font-mono)',
            fontSize: '0.75rem',
            fontWeight: 600,
          }}
        >
          {footerRight}
        </span>
      </div>
    </div>
  )
}
