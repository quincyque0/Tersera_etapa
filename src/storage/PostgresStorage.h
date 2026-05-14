#ifndef POSTGRES_STORAGE_H
#define POSTGRES_STORAGE_H

#include <string>
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <pqxx/pqxx>
#include "../core/GeoData.h"
#include "../map/MapWindow.h"

class PostgresStorage {
public:
    PostgresStorage();
    ~PostgresStorage();
    
    bool connect(const std::string& conn_string = "");
    void disconnect();
    bool isConnected() const { return connected; }
    
    void loadAllData(GeoData* geoInfo);
    std::vector<DataPoint> loadAllDataForHeatmap();
    void saveDataPoint(const DataPoint& point);
    void startMonitor(GeoData* geoInfo);
    void stopMonitor();
    int getTotalPointsCount();
    std::vector<MapPoint> loadAllPoints();
    std::vector<int> getUniqueArfcns();
    
    void setConnectionParams(const std::string& host, const std::string& dbname, 
                            const std::string& user, const std::string& password);
    
private:
    std::unique_ptr<pqxx::connection> conn;
    std::string conn_string;
    bool connected;
    std::recursive_mutex dbMutex;
    std::atomic<bool> monitoring;
    std::thread monitorThread;
    
    void createTablesIfNotExist();
    long long getLastTimestamp();
};

#endif