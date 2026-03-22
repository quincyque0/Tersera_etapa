import React, { useState, useEffect } from 'react';
import { MapContainer, TileLayer } from 'react-leaflet';
import 'leaflet/dist/leaflet.css';
import 'leaflet.heat';

import locationsData from '../../locations.json';
import { parseCellInfoPoints, getSignalLevel } from './funtions';
import HeatmapLayer from './HeatMap';
import PointsLayer from './Points';
import ControlPanel from './ControlPanel';
import StatsPanel from './StatsPanel';
import Legend from './Legend';
import FitBounds from './FitBounds';

const HeatmapSignalQuality = () => {
  const [points, setPoints] = useState([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState(null);
  const [heatmapRadius, setHeatmapRadius] = useState(20);
  const [heatmapBlur, setHeatmapBlur] = useState(40);
  const [showPoints, setShowPoints] = useState( false);
  
  useEffect(() => {
    try {
      if (!locationsData || locationsData.length === 0) {
        setError('Данные не найдены');
        setLoading(false);
        return;
      }
      
      const processedPoints = [];
      
      locationsData.forEach((item) => {
        if (item.latitude && item.longitude) {
          const cellPoints = parseCellInfoPoints(item.cellInfo);
          
          cellPoints.forEach((point) => {
            processedPoints.push({
              lat: item.latitude,
              lng: item.longitude,
              rssi: point.rssi,
              timestamp: item.timestamp,
              imei: item.imei,
            });
          });
        }
      });
      
      setPoints(processedPoints);
      setLoading(false);
    } catch (err) {
      setError('Ошибка: ' + err.message);
      setLoading(false);
    }
  }, []);
  
  if (loading) {
    return (
      <div className="heatmap-loading">
        <div className="loader"></div>
        <p>Загрузка данных...</p>
      </div>
    );
  }
  
  if (error) {
    return (
      <div className="heatmap-error">
        <p>{error}</p>
      </div>
    );
  }
  
  if (points.length === 0) {
    return (
      <div className="heatmap-empty">
        <p>Нет данных для отображения</p>
      </div>
    );
  }
  
  return (
    <div className="heatmap-container">
      <MapContainer
        center={[55.0425, 83.0154]}
        zoom={15}
        style={{ height: '100%', width: '100%' }}
        zoomControl={true}
      >
        <TileLayer
          url="https://{s}.basemaps.cartocdn.com/light_all/{z}/{x}/{y}{r}.png"
          attribution='&copy; OSM & CartoDB'
        />
        
        <HeatmapLayer 
          points={points} 
          radius={heatmapRadius}
          blur={heatmapBlur}
          maxZoom={18}
        />
        
        <PointsLayer points={points} showPoints={showPoints} />
        
        <FitBounds points={points} />
      </MapContainer>
      
      <ControlPanel
        heatmapRadius={heatmapRadius}
        setHeatmapRadius={setHeatmapRadius}
        heatmapBlur={heatmapBlur}
        setHeatmapBlur={setHeatmapBlur}
        showPoints={showPoints}
        setShowPoints={setShowPoints}
      />
      
      <StatsPanel points={points} />
      <Legend />
      
      <style>{`
        .heatmap-container {
          position: relative;
          width: 100%;
          height: 100vh;
          font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
        }
        
        .control-panel, .stats-panel, .heatmap-legend {
          position: absolute;
          background: rgba(0, 0, 0, 0.85);
          color: white;
          padding: 12px 15px;
          border-radius: 8px;
          box-shadow: 0 2px 8px rgba(0,0,0,0.3);
          z-index: 1000;
          backdrop-filter: blur(5px);
          border: 1px solid rgba(255,255,255,0.2);
          font-size: 12px;
        }
        
        .control-panel {
          top: 10px;
          left: 10px;
          min-width: 260px;
          max-height: 80vh;
          overflow-y: auto;
        }
        
        .stats-panel {
          top: 10px;
          right: 10px;
          min-width: 260px;
        }
        
        .heatmap-legend {
          bottom: 20px;
          right: 10px;
          min-width: 240px;
        }
        
        .control-panel h4, .stats-panel h4, .heatmap-legend h4 {
          margin: 0 0 10px 0;
          font-size: 14px;
          color: #fff;
        }
        
        .control-group {
          margin-bottom: 12px;
        }
        
        .control-group label {
          display: block;
          margin-bottom: 5px;
          color: #ddd;
          cursor: pointer;
        }
        
        .control-group input[type="range"] {
          width: 100%;
          cursor: pointer;
          margin-top: 5px;
        }
        
        .control-group small {
          display: block;
          margin-top: 3px;
          color: #aaa;
          font-size: 10px;
        }
        
        .gradient-bar {
          margin-bottom: 10px;
        }
        
        .gradient-colors {
          height: 20px;
          background: linear-gradient(to right, #ff0000, #ffa500, #ffff00, #adff2f, #00ff00);
          border-radius: 4px;
          margin: 10px 0;
        }
        
        .gradient-labels {
          display: flex;
          justify-content: space-between;
          font-size: 10px;
          color: #ccc;
        }
        
        .stats-grid {
          display: grid;
          grid-template-columns: 1fr 1fr;
          gap: 6px 10px;
          font-size: 11px;
        }
        
        .full-width {
          grid-column: span 2;
        }
        
        .stat-item {
          display: flex;
          justify-content: space-between;
          gap: 8px;
        }
        
        .stat-item span {
          color: #ccc;
        }
        
        .legend-info {
          font-size: 11px;
          color: #ccc;
          margin-top: 10px;
          padding-top: 8px;
          border-top: 1px solid #444;
        }
        
        .legend-info p {
          margin: 4px 0;
        }
        
        .stats-info {
          margin-top: 12px;
          padding-top: 8px;
          border-top: 1px solid #444;
          font-size: 11px;
          color: #ccc;
        }
        
        .stats-info ul {
          margin: 5px 0 0 0;
          padding-left: 20px;
        }
        
        .stats-info li {
          margin: 3px 0;
        }
        
        .heatmap-loading, .heatmap-error, .heatmap-empty {
          display: flex;
          flex-direction: column;
          justify-content: center;
          align-items: center;
          height: 100vh;
          background: #1a1a1a;
          color: white;
        }
        
        .loader {
          border: 4px solid #f3f3f3;
          border-top: 4px solid #3498db;
          border-radius: 50%;
          width: 40px;
          height: 40px;
          animation: spin 1s linear infinite;
          margin-bottom: 20px;
        }
        
        @keyframes spin {
          0% { transform: rotate(0deg); }
          100% { transform: rotate(360deg); }
        }
        
        @media (max-width: 768px) {
          .control-panel, .stats-panel, .heatmap-legend {
            font-size: 10px;
            padding: 8px 12px;
            min-width: 200px;
          }
          .control-panel {
            max-height: 60vh;
          }
        }
      `}</style>
    </div>
  );
};

export default HeatmapSignalQuality;