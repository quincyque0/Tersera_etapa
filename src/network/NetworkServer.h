#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

#include "../core/GeoData.h"
#include "../storage/PostgresStorage.h"
#include <atomic>
#include <memory>

#define NETWORK_PORT 5555

class NetworkServer {
public:
    NetworkServer(GeoData* geoInfo);
    ~NetworkServer();
    
    void start();
    void stop();
    void setStorage(std::shared_ptr<PostgresStorage> storage);
    
private:
    GeoData* geoInfo;
    std::shared_ptr<PostgresStorage> storage;
    std::atomic<bool> running;
    
    void parseIncomingData(const std::string& rawData);
    void processDataPoint(const DataPoint& point);
};

#endif