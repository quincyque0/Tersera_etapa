#include "PostgresStorage.h"
#include "../parser/JsonParser.h"
#include "../utils/Logger.h"
#include <thread>
#include <chrono>
#include <sstream>

PostgresStorage::PostgresStorage() : connected(false), monitoring(false) {
    conn_string = "host=localhost dbname=cell_monitor user=postgres password=postgres";
}

PostgresStorage::~PostgresStorage() {
    stopMonitor();
    disconnect();
}

bool PostgresStorage::connect(const std::string& conn_string) {
    if (!conn_string.empty()) {
        this->conn_string = conn_string;
    }
    
    try {
        conn = std::make_unique<pqxx::connection>(this->conn_string);
        if (conn->is_open()) {
            connected = true;
            Logger::log("INFO", "Подключено к PostgreSQL");
            return true;
        }
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка подключения: " + std::string(e.what()));
    }
    return false;
}

void PostgresStorage::disconnect() {
    if (conn && conn->is_open()) {
        conn->close();
        connected = false;
    }
}

void PostgresStorage::saveDataPoint(const DataPoint& point) {
    if (!connected) return;
    
    try {
        pqxx::work txn(*conn);
        
        std::stringstream query;
        query << "INSERT INTO data_points (timestamp, latitude, longitude, altitude, device_id) "
              << "VALUES (" << point.timestamp << ", "
              << point.latitude << ", "
              << point.longitude << ", "
              << point.altitude << ", "
              << txn.quote(point.deviceId) << ") "
              << "RETURNING id";
        
        auto result = txn.exec(query.str());
        int dataPointId = result[0][0].as<int>();
        
        for (const auto& cell : point.cells) {
            std::stringstream cellQuery;
            cellQuery << "INSERT INTO cell_info (data_point_id, rssi, arfcn, cell_id, lac, "
                     << "is_registered, operator_name, timestamp) "
                     << "VALUES (" << dataPointId << ", "
                     << cell.rssi << ", "
                     << cell.arfcn << ", "
                     << cell.cellId << ", "
                     << cell.lac << ", "
                     << (cell.isRegistered ? "TRUE" : "FALSE") << ", "
                     << txn.quote(cell.operator_) << ", "
                     << cell.timestamp << ")";
            
            txn.exec(cellQuery.str());
        }
        
        txn.commit();
        Logger::log("INFO", "Сохранено в БД: точка " + std::to_string(dataPointId));
        
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка сохранения: " + std::string(e.what()));
    }
}

void PostgresStorage::loadAllData(GeoData* geoInfo) {
    if (!connected) return;
    
    try {
        pqxx::nontransaction txn(*conn);
        
        auto result = txn.exec(
            "SELECT id, timestamp, latitude, longitude, altitude, device_id "
            "FROM data_points ORDER BY timestamp DESC LIMIT 500"
        );
        
        std::deque<DataPoint> newHistory;
        std::set<int> newCellIds;
        
        for (const auto& row : result) {
            DataPoint point;
            int pointId = row["id"].as<int>();
            point.timestamp = row["timestamp"].as<long long>();
            point.latitude = row["latitude"].as<float>();
            point.longitude = row["longitude"].as<float>();
            point.altitude = row["altitude"].as<float>();
            point.deviceId = row["device_id"].as<std::string>();
            
            auto cellsResult = txn.exec(
                "SELECT rssi, arfcn, cell_id, lac, is_registered, operator_name, timestamp "
                "FROM cell_info WHERE data_point_id = " + std::to_string(pointId)
            );
            
            for (const auto& cellRow : cellsResult) {
                CellInfo cell;
                cell.rssi = cellRow["rssi"].as<int>();
                cell.arfcn = cellRow["arfcn"].as<int>();
                cell.cellId = cellRow["cell_id"].as<int>();
                cell.lac = cellRow["lac"].as<int>();
                cell.isRegistered = cellRow["is_registered"].as<bool>();
                cell.operator_ = cellRow["operator_name"].as<std::string>();
                cell.timestamp = cellRow["timestamp"].as<long long>();
                point.cells.push_back(cell);
                newCellIds.insert(cell.cellId);
            }
            
            newHistory.push_front(point);
        }
        
        {
            std::lock_guard<std::mutex> lock(geoInfo->historyMutex);
            geoInfo->dataHistory = newHistory;
            geoInfo->knownCellIds = newCellIds;
            geoInfo->totalPoints.store(newHistory.size());
        }
        
        if (!newHistory.empty()) {
            const auto& lastPoint = newHistory.back();
            geoInfo->latValue.store(lastPoint.latitude);
            geoInfo->lonValue.store(lastPoint.longitude);
            geoInfo->altValue.store(lastPoint.altitude);
            geoInfo->timeStamp.store(lastPoint.timestamp);
            geoInfo->deviceId = lastPoint.deviceId;
            geoInfo->currentCells = lastPoint.cells;
            geoInfo->updateSignalStats(lastPoint.cells);
        }
        
        Logger::log("INFO", "Загружено " + std::to_string(newHistory.size()) + " точек");
        
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка загрузки: " + std::string(e.what()));
    }
}

long long PostgresStorage::getLastTimestamp() {
    if (!connected) return 0;
    
    try {
        pqxx::nontransaction txn(*conn);
        auto result = txn.exec("SELECT MAX(timestamp) FROM data_points");
        if (!result.empty() && !result[0][0].is_null()) {
            return result[0][0].as<long long>();
        }
    } catch (const std::exception& e) {}
    return 0;
}

int PostgresStorage::getTotalPointsCount() {
    if (!connected) return 0;
    
    try {
        pqxx::nontransaction txn(*conn);
        auto result = txn.exec("SELECT COUNT(*) FROM data_points");
        if (!result.empty()) {
            return result[0][0].as<int>();
        }
    } catch (const std::exception& e) {}
    return 0;
}

void PostgresStorage::startMonitor(GeoData* geoInfo) {
    if (monitoring) return;
    
    monitoring = true;
    monitorThread = std::thread([this, geoInfo]() {
        int lastCount = getTotalPointsCount();
        
        while (monitoring) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            try {
                int currentCount = getTotalPointsCount();
                if (currentCount > lastCount) {
                    Logger::log("INFO", "Новые данные в БД");
                    lastCount = currentCount;
                    geoInfo->needReloadFromFile.store(true);
                }
            } catch (const std::exception& e) {}
        }
    });
}

void PostgresStorage::stopMonitor() {
    monitoring = false;
    if (monitorThread.joinable()) {
        monitorThread.join();
    }
}

void PostgresStorage::setConnectionParams(const std::string& host, const std::string& dbname,
                                          const std::string& user, const std::string& password) {
    conn_string = "host=" + host + " dbname=" + dbname +  " user=" + user + " password=" + password;
}