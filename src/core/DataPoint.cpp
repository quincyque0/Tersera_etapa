#include "DataPoint.h"

float DataPoint::getAverageRssi() const {
    if (cells.empty()) return 0;
    float sum = 0;
    for (const auto& cell : cells) {
        sum += cell.rssi;
    }
    return sum / cells.size();
}

int DataPoint::getMaxRssi() const {
    if (cells.empty()) return 0;
    int maxRssi = -1000;
    for (const auto& cell : cells) {
        if (cell.rssi > maxRssi) maxRssi = cell.rssi;
    }
    return maxRssi;
}

size_t DataPoint::getCellCount() const {
    return cells.size();
}