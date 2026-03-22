const ControlPanel = ({ 
    heatmapRadius, setHeatmapRadius, 
    heatmapBlur, setHeatmapBlur, 
    showPoints, setShowPoints 
  }) => {
    return (
      <div className="control-panel">
        <h4>Настройки отображения</h4>
        
        <div className="control-group">
          <label>Радиус влияния: {heatmapRadius}px</label>
          <input
            type="range"
            min="1"
            max="60"
            step="1"
            value={heatmapRadius}
            onChange={(e) => setHeatmapRadius(parseInt(e.target.value))}
          />
          <small>Больше радиус = сильнее слияние точек</small>
        </div>
        
        <div className="control-group">
          <label>Размытие: {heatmapBlur}px</label>
          <input
            type="range"
            min="5"
            max="99"
            step="5"
            value={heatmapBlur}
            onChange={(e) => setHeatmapBlur(parseInt(e.target.value))}
          />
          <small>Больше размытие = плавнее переходы</small>
        </div>
        
        <div className="control-group">
          <label>
            <input
              type="checkbox"
              checked={showPoints}
              onChange={(e) => setShowPoints(e.target.checked)}
            />
            Показывать отдельные точки
          </label>
        </div>
        
        <div className="stats-info">
          <p><strong>Как читать тепловую карту:</strong></p>
          <ul>
            <li><strong style={{color: '#00ff00'}}>Зеленые зоны</strong> = хороший сигнал</li>
            <li><strong style={{color: '#ffff00'}}>Желтые зоны</strong> = средний сигнал</li>
            <li><strong style={{color: '#ff0000'}}>Красные зоны</strong> = плохой сигнал</li>
          </ul>
        </div>
      </div>
    );
  };
  
  export default ControlPanel;