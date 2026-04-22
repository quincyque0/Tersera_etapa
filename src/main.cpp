#include <thread>
#include <memory>
#include "gui/GUIApplication.h"
#include "network/NetworkServer.h"
#include "storage/PostgresStorage.h"
#include "core/GeoData.h"
#include "utils/Logger.h"

std::shared_ptr<PostgresStorage> g_storage;

int main() {
    Logger::log("INFO", "Cell Signal Monitor с PostgreSQL и картой");
    
    g_storage = std::make_shared<PostgresStorage>();
    g_storage->setConnectionParams("localhost", "cell_monitor", "postgres", "postgres");
    
    if (!g_storage->connect()) {
        Logger::log("ERROR", "Не удалось подключиться к PostgreSQL");
        return 1;
    }
    
    GeoData geoInfo;
    geoInfo.init();
    
    g_storage->loadAllData(&geoInfo);
    
    NetworkServer server(&geoInfo);
    server.setStorage(g_storage);
    std::thread serverThread(&NetworkServer::start, &server);
    
    g_storage->startMonitor(&geoInfo);
    
    GUIApplication app(&geoInfo);
    app.run();
    
    server.stop();
    g_storage->stopMonitor();
    serverThread.join();
    
    return 0;
}