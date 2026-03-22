import { useEffect } from 'react';
import { useMap } from 'react-leaflet';
import L from 'leaflet';
import { groupPointsByQuality, getGradientConfig } from './funtions';

const HeatmapLayer = ({ points, radius, blur, maxZoom }) => {
  const map = useMap();
  
  useEffect(() => {
    if (!map || !points || points.length === 0) return;
    
    const groups = groupPointsByQuality(points);
    const gradients = getGradientConfig();
    
    const baseOptions = {
      radius: radius || 20,
      blur: blur || 40,
      maxZoom: maxZoom || 18,
      minOpacity: 0.3,
      maxOpacity: 0.8,
    };
    
    const layers = [];
    const groupNames = ['excellent', 'good', 'medium', 'poor', 'veryPoor'];
    
    groupNames.forEach(groupName => {
      const groupPoints = groups[groupName];
      if (groupPoints.length > 0) {
        const layer = L.heatLayer(groupPoints, {
          ...baseOptions,
          gradient: gradients[groupName].gradient
        });
        layers.push(layer);
        layer.addTo(map);
      }
    });
    
    return () => {
      layers.forEach(layer => {
        if (layer) map.removeLayer(layer);
      });
    };
  }, [map, points, radius, blur, maxZoom]);
  
  return null;
};

export default HeatmapLayer;