import { useMemo } from 'react';
import { calculateStats } from './funtions';

const StatsPanel = ({ points }) => {
  const stats = useMemo(() => calculateStats(points), [points]);
  
  if (!stats) return null;
  
  return (
    <div className="stats-panel">
      <h4>Статистика сигнала</h4>
      <div className="stats-grid">
        <div className="stat-item full-width">
          <span>Всего точек:</span>
          <strong>{stats.totalPoints}</strong>
        </div>
        <div className="stat-item full-width">
          <span>Средний RSSI:</span>
          <strong style={{color: stats.avgRssi > -65 ? '#4caf50' : stats.avgRssi > -85 ? '#ff9800' : '#f44336'}}>
            {stats.avgRssi} dBm
          </strong>
        </div>
        <div className="stat-item">
          <span>Отлично (≥-65):</span>
          <strong style={{ color: '#4caf50' }}>{stats.excellent}%</strong>
        </div>
        <div className="stat-item">
          <span>Хорошо (-75...-66):</span>
          <strong style={{ color: '#8bc34a' }}>{stats.good}%</strong>
        </div>
        <div className="stat-item">
          <span>Средне (-85...-76):</span>
          <strong style={{ color: '#ff9800' }}>{stats.medium}%</strong>
        </div>
        <div className="stat-item">
          <span>Плохо (≤-86):</span>
          <strong style={{ color: '#f44336' }}>{stats.poor}%</strong>
        </div>
      </div>
    </div>
  );
};

export default StatsPanel;