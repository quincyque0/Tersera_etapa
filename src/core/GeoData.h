#ifndef GEODATA_H
#define GEODATA_H

#include <atomic>
#include <deque>
#include <mutex>
#include <set>
#include <string>
#include <vector>
#include "CellInfo.h"
#include "DataPoint.h"

#define MAX_HISTORY_POINTS 500

struct GeoData {
    std::atomic<float> latValue;
    std::atomic<float> lonValue;
    std::atomic<float> altValue;
    std::atomic<long long> timeStamp;
    std::string deviceId;
    std::deque<DataPoint> dataHistory;
    std::mutex historyMutex;
    std::vector<CellInfo> currentCells;
    std::atomic<int> bestRssi{0};
    std::atomic<int> avgRssi{0};
    std::atomic<int> worstRssi{0};
    std::atomic<bool> needReloadFromFile{false};
    std::set<int> knownCellIds;
    std::atomic<int> totalPoints{0};
    
    void init();
    void updateSignalStats(const std::vector<CellInfo>& cells);
    void addDataPoint(const DataPoint& point);
    void clearHistory();
};

#endif