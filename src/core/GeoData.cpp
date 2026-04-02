#include "GeoData.h"
#include "../utils/Logger.h"

void GeoData::init() {
    latValue.store(0.0f);
    lonValue.store(0.0f);
    altValue.store(0.0f);
    timeStamp.store(0);
    deviceId = "None";
    bestRssi.store(0);
    avgRssi.store(0);
    worstRssi.store(0);
    needReloadFromFile.store(false);
    totalPoints.store(0);
}

void GeoData::updateSignalStats(const std::vector<CellInfo>& cells) {
    if (cells.empty()) {
        bestRssi.store(0);
        avgRssi.store(0);
        worstRssi.store(0);
        return;
    }
    
    int best = -1000;
    int worst = 1000;
    int sum = 0;
    
    for (const auto& cell : cells) {
        if (cell.rssi > best) best = cell.rssi;
        if (cell.rssi < worst) worst = cell.rssi;
        sum += cell.rssi;
    }
    
    bestRssi.store(best);
    worstRssi.store(worst);
    avgRssi.store(sum / cells.size());
}

void GeoData::addDataPoint(const DataPoint& point) {
    std::lock_guard<std::mutex> lock(historyMutex);
    dataHistory.push_back(point);
    
    for (const auto& cell : point.cells) {
        knownCellIds.insert(cell.cellId);
    }
    
    currentCells = point.cells;
    latValue.store(point.latitude);
    lonValue.store(point.longitude);
    altValue.store(point.altitude);
    timeStamp.store(point.timestamp);
    deviceId = point.deviceId;
    
    updateSignalStats(point.cells);
    
    while (dataHistory.size() > MAX_HISTORY_POINTS) {
        dataHistory.pop_front();
    }
    
    totalPoints.store(dataHistory.size());
}

void GeoData::clearHistory() {
    std::lock_guard<std::mutex> lock(historyMutex);
    dataHistory.clear();
    knownCellIds.clear();
    currentCells.clear();
    bestRssi.store(0);
    avgRssi.store(0);
    worstRssi.store(0);
    totalPoints.store(0);
}