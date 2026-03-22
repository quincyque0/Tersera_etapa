import { CircleMarker, Popup } from 'react-leaflet';
import { getPointColor, getSignalLevel } from './funtions';

const PointsLayer = ({ points, showPoints }) => {
  if (!showPoints) return null;
  
  return (
    <>
      {points.map((point, idx) => (
        <CircleMarker
          key={`point-${idx}`}
          center={[point.lat, point.lng]}
          radius={5}
          fillColor={getPointColor(point.rssi)}
          color="#333"
          weight={1}
          fillOpacity={0.8}
        >
          <Popup>
            <div style={{ minWidth: '180px' }}>
              <strong>Информация о сигнале</strong><br/>
              <hr/>
              <strong>RSSI:</strong> {point.rssi} dBm<br/>
              <strong>Качество:</strong> {getSignalLevel(point.rssi)}<br/>
              {point.imei && <><strong>IMEI:</strong> {point.imei}<br/></>}
              {point.timestamp && <><strong>Время:</strong> {new Date(point.timestamp).toLocaleString()}</>}
              <hr/>
            </div>
          </Popup>
        </CircleMarker>
      ))}
    </>
  );
};

export default PointsLayer;