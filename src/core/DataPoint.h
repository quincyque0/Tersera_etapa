#ifndef DATAPOINT_H
#define DATAPOINT_H

#include <vector>
#include <string>
#include "CellInfo.h"

struct DataPoint {
    long long timestamp;
    float latitude;
    float longitude;
    float altitude;
    std::string deviceId;
    std::vector<CellInfo> cells;
    
    float getAverageRssi() const;
    int getMaxRssi() const;
    size_t getCellCount() const;
};

#endif