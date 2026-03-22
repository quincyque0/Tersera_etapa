export const parseCellInfoPoints = (cellInfoString) => {
    if (!cellInfoString) return [];
    
    const points = [];
    const rssiRegex = /rssi=(-?\d+)/g;
    let match;
    
    while ((match = rssiRegex.exec(cellInfoString)) !== null) {
      const rssi = parseInt(match[1], 10);
      if (!isNaN(rssi)) {
        points.push({ rssi });
      }
    }
    return points;
  };
  
  export const normalizeIntensity = (rssi) => {
    const minRssi = -110;
    const maxRssi = -50;
    let intensity = (rssi - minRssi) / (maxRssi - minRssi);
    return Math.min(Math.max(intensity, 0.2), 1);
  };
  
  export const getPointColor = (rssi) => {
    if (rssi >= -65) return '#00ff00';
    if (rssi >= -75) return '#adff2f';
    if (rssi >= -85) return '#ffff00';
    if (rssi >= -95) return '#ffa500';
    return '#ff0000';
  };
  

  export const getSignalLevel = (rssi) => {
    if (rssi >= -65) return 'Отлично';
    if (rssi >= -75) return 'Хорошо';
    if (rssi >= -85) return 'Средне';
    if (rssi >= -95) return 'Плохо';
    return 'Очень плохо';
  };
  
  export const groupPointsByQuality = (points) => {
    return {
      excellent: points.filter(p => p.rssi >= -65).map(p => [p.lat, p.lng, 0.9]),
      good: points.filter(p => p.rssi >= -75 && p.rssi < -65).map(p => [p.lat, p.lng, 0.7]),
      medium: points.filter(p => p.rssi >= -85 && p.rssi < -75).map(p => [p.lat, p.lng, 0.5]),
      poor: points.filter(p => p.rssi >= -95 && p.rssi < -85).map(p => [p.lat, p.lng, 0.3]),
      veryPoor: points.filter(p => p.rssi < -95).map(p => [p.lat, p.lng, 0.2]),
    };
  };
  

  export const getGradientConfig = () => ({
    excellent: { gradient: { 0.0: '#00ff00', 0.5: '#00cc00', 1.0: '#00ff00' }, name: 'Отличный' },
    good: { gradient: { 0.0: '#adff2f', 0.5: '#8bc34a', 1.0: '#adff2f' }, name: 'Хороший' },
    medium: { gradient: { 0.0: '#ffff00', 0.5: '#ffcc00', 1.0: '#ffff00' }, name: 'Средний' },
    poor: { gradient: { 0.0: '#ffa500', 0.5: '#ff8800', 1.0: '#ffa500' }, name: 'Плохой' },
    veryPoor: { gradient: { 0.0: '#ff0000', 0.5: '#cc0000', 1.0: '#ff0000' }, name: 'Очень плохой' },
  });
  
  export const calculateStats = (points) => {
    if (!points || points.length === 0) return null;
    
    const rssiValues = points.map(p => p.rssi);
    const avgRssi = rssiValues.reduce((a, b) => a + b, 0) / rssiValues.length;
    const minRssi = Math.min(...rssiValues);
    const maxRssi = Math.max(...rssiValues);
    const excellentCount = rssiValues.filter(r => r >= -65).length;
    const goodCount = rssiValues.filter(r => r >= -75 && r < -65).length;
    const mediumCount = rssiValues.filter(r => r >= -85 && r < -75).length;
    const poorCount = rssiValues.filter(r => r < -85).length;
    
    return {
      totalPoints: points.length,
      avgRssi: avgRssi.toFixed(1),
      minRssi,
      maxRssi,
      excellent: ((excellentCount / points.length) * 100).toFixed(1),
      good: ((goodCount / points.length) * 100).toFixed(1),
      medium: ((mediumCount / points.length) * 100).toFixed(1),
      poor: ((poorCount / points.length) * 100).toFixed(1),
    };
  };