#ifndef FILE_STORAGE_H
#define FILE_STORAGE_H

#include <string>
#include "../core/GeoData.h"

#define JSON_FILE_PATH "../database/locations.json"

class FileStorage {
public:
    static void loadAllData(GeoData* geoInfo);
    static void appendData(GeoData* geoInfo);
    static void saveDataPoint(const std::string& data, GeoData* geoInfo);
    static void startFileMonitor(GeoData* geoInfo);
};

#endif 