const Legend = () => {
    return (
      <div className="heatmap-legend">
        <h4>Качество сигнала (RSSI)</h4>
        <div className="gradient-bar">
          <div className="gradient-colors"></div>
          <div className="gradient-labels">
            <span>Плохо<br/>(-110 dBm)</span>
            <span>Средне<br/>(-80 dBm)</span>
            <span>Отлично<br/>(-50 dBm)</span>
          </div>
        </div>
        <div className="legend-info">
          <p><strong style={{color: '#00ff00'}}>Зеленый</strong> → хороший сигнал (≥ -65)</p>
          <p><strong style={{color: '#ffff00'}}>Желтый</strong> → средний сигнал (-85...-66)</p>
          <p><strong style={{color: '#ff0000'}}>Красный</strong> → плохой сигнал (≤ -86)</p>
          <p><strong>Слияние цветов</strong> = множество точек в зоне</p>
          <p><strong>Цвет</strong> = качество сигнала (чем зеленее, тем лучше)</p>
        </div>
      </div>
    );
  };
  
  export default Legend;