import { useEffect } from 'react';
import { useMap } from 'react-leaflet';
import L from 'leaflet';

const FitBounds = ({ points }) => {
  const map = useMap();
  
  useEffect(() => {
    if (points && points.length > 0) {
      const bounds = L.latLngBounds(points.map(p => [p.lat, p.lng]));
      map.fitBounds(bounds, { padding: [50, 50] });
    }
  }, [map, points]);
  
  return null;
};

export default FitBounds;