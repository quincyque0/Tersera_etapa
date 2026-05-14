#include "PostgresStorage.h"
#include "../utils/Logger.h"
#include <iostream>

PostgresStorage::PostgresStorage() : connected(false), monitoring(false) {}

PostgresStorage::~PostgresStorage() {
    disconnect();
}

void PostgresStorage::setConnectionParams(const std::string& host, const std::string& dbname,
                                          const std::string& user, const std::string& password) {
    conn_string = "host=" + host + " dbname=" + dbname + " user=" + user + " password=" + password;
}

bool PostgresStorage::connect(const std::string& conn_string_param) {
    std::string conn_str = conn_string_param.empty() ? conn_string : conn_string_param;
    
    if (conn_str.empty()) {
        Logger::log("ERROR", "Строка подключения пуста");
        return false;
    }
    
    try {
        std::lock_guard<std::recursive_mutex> lock(dbMutex);
        conn = std::make_unique<pqxx::connection>(conn_str);
        if (conn->is_open()) {
            connected = true;
            createTablesIfNotExist();
            Logger::log("INFO", "Подключено к PostgreSQL");
            return true;
        }
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка подключения к PostgreSQL: " + std::string(e.what()));
    }
    
    return false;
}

void PostgresStorage::disconnect() {
    std::lock_guard<std::recursive_mutex> lock(dbMutex);
    if (conn && conn->is_open()) {
        conn->close();
    }
    connected = false;
}

void PostgresStorage::createTablesIfNotExist() {
    if (!connected) return;
    
    try {
        std::lock_guard<std::recursive_mutex> lock(dbMutex);
        pqxx::work txn(*conn);
        
        txn.exec(
            "CREATE TABLE IF NOT EXISTS data_points ("
            "id SERIAL PRIMARY KEY,"
            "timestamp BIGINT NOT NULL,"
            "latitude REAL,"
            "longitude REAL,"
            "altitude REAL,"
            "device_id VARCHAR(50)"
            ")"
        );
        
        txn.exec(
            "CREATE TABLE IF NOT EXISTS cell_info ("
            "id SERIAL PRIMARY KEY,"
            "data_point_id INTEGER REFERENCES data_points(id) ON DELETE CASCADE,"
            "rssi INTEGER,"
            "arfcn INTEGER,"
            "cell_id INTEGER,"
            "lac INTEGER,"
            "is_registered BOOLEAN,"
            "operator_name VARCHAR(100),"
            "timestamp BIGINT,"
            "rsrp INTEGER DEFAULT 0,"
            "rsrq INTEGER DEFAULT 0"
            ")"
        );
        
        txn.exec(
            "CREATE INDEX IF NOT EXISTS idx_cell_info_cell_id ON cell_info(cell_id)"
        );
        
        txn.exec(
            "CREATE INDEX IF NOT EXISTS idx_data_points_timestamp ON data_points(timestamp)"
        );
        
        
        pqxx::result colCheck = txn.exec("SELECT column_name FROM information_schema.columns WHERE table_name='cell_info' AND column_name='rsrp'");
        if (colCheck.empty()) {
            txn.exec("ALTER TABLE cell_info ADD COLUMN rsrp INTEGER DEFAULT 0");
            txn.exec("ALTER TABLE cell_info ADD COLUMN rsrq INTEGER DEFAULT 0");
        }
        
        txn.commit();
        Logger::log("INFO", "Таблицы созданы/проверены");
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка создания таблиц: " + std::string(e.what()));
    }
}

void PostgresStorage::saveDataPoint(const DataPoint& point) {
    if (!connected) return;
    
    try {
        std::lock_guard<std::recursive_mutex> lock(dbMutex);
        pqxx::work txn(*conn);
        
        std::string query = "INSERT INTO data_points (timestamp, latitude, longitude, altitude, device_id) "
                           "VALUES (" + 
                           std::to_string(point.timestamp) + ", " +
                           std::to_string(point.latitude) + ", " +
                           std::to_string(point.longitude) + ", " +
                           std::to_string(point.altitude) + ", " +
                           txn.quote(point.deviceId) + 
                           ") RETURNING id";
        
        pqxx::result res = txn.exec(query);
        int dataPointId = res[0][0].as<int>();
        
        for (const auto& cell : point.cells) {
            std::string cellQuery = "INSERT INTO cell_info (data_point_id, rssi, arfcn, cell_id, lac, is_registered, operator_name, timestamp, rsrp, rsrq) "
                                   "VALUES (" + 
                                   std::to_string(dataPointId) + ", " +
                                   std::to_string(cell.rssi) + ", " +
                                   std::to_string(cell.arfcn) + ", " +
                                   std::to_string(cell.cellId) + ", " +
                                   std::to_string(cell.lac) + ", " +
                                   (cell.isRegistered ? "TRUE" : "FALSE") + ", " +
                                   txn.quote(cell.operator_) + ", " +
                                   std::to_string(cell.timestamp) + ", " +
                                   std::to_string(cell.rsrp) + ", " +
                                   std::to_string(cell.rsrq) + ")";
            txn.exec(cellQuery);
        }
        
        txn.commit();
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка сохранения точки: " + std::string(e.what()));
    }
}

long long PostgresStorage::getLastTimestamp() {
    if (!connected) return 0;
    
    try {
        std::lock_guard<std::recursive_mutex> lock(dbMutex);
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec("SELECT MAX(timestamp) FROM data_points");
        if (!res[0][0].is_null()) {
            return res[0][0].as<long long>();
        }
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка получения последнего timestamp: " + std::string(e.what()));
    }
    return 0;
}

void PostgresStorage::loadAllData(GeoData* geoInfo) {
    if (!connected || !geoInfo) return;
    
    geoInfo->clearHistory();
    
    try {
        std::lock_guard<std::recursive_mutex> lock(dbMutex);
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec(
            "SELECT dp.id, dp.timestamp, dp.latitude, dp.longitude, dp.altitude, dp.device_id, "
            "ci.rssi, ci.arfcn, ci.cell_id, ci.lac, ci.is_registered, ci.operator_name, ci.timestamp as cell_timestamp, ci.rsrp, ci.rsrq "
            "FROM data_points dp "
            "LEFT JOIN cell_info ci ON ci.data_point_id = dp.id "
            "ORDER BY dp.timestamp ASC"
        );
        
        std::map<int, DataPoint> pointsMap;
        
        for (const auto& row : res) {
            int pointId = row["id"].as<int>();
            
            if (pointsMap.find(pointId) == pointsMap.end()) {
                DataPoint point;
                point.timestamp = row["timestamp"].as<long long>();
                point.latitude = row["latitude"].as<float>();
                point.longitude = row["longitude"].as<float>();
                point.altitude = row["altitude"].as<float>();
                point.deviceId = row["device_id"].as<std::string>();
                pointsMap[pointId] = point;
            }
            
            if (!row["cell_id"].is_null()) {
                CellInfo cell;
                cell.rssi = row["rssi"].as<int>();
                cell.arfcn = row["arfcn"].as<int>();
                cell.cellId = row["cell_id"].as<int>();
                cell.lac = row["lac"].as<int>();
                cell.isRegistered = row["is_registered"].as<bool>();
                if (!row["operator_name"].is_null()) cell.operator_ = row["operator_name"].as<std::string>();
                cell.timestamp = row["cell_timestamp"].as<long long>();
                if (!row["rsrp"].is_null()) cell.rsrp = row["rsrp"].as<int>();
                if (!row["rsrq"].is_null()) cell.rsrq = row["rsrq"].as<int>();
                pointsMap[pointId].cells.push_back(cell);
            }
        }
        
        for (auto& pair : pointsMap) {
            geoInfo->addDataPoint(pair.second);
        }
        
        Logger::log("INFO", "Загружено " + std::to_string(pointsMap.size()) + " точек");
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка загрузки данных: " + std::string(e.what()));
    }
}

std::vector<DataPoint> PostgresStorage::loadAllDataForHeatmap() {
    std::vector<DataPoint> result;
    if (!connected) return result;
    
    try {
        std::lock_guard<std::recursive_mutex> lock(dbMutex);
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec(
            "SELECT dp.id, dp.timestamp, dp.latitude, dp.longitude, dp.altitude, dp.device_id, "
            "ci.rssi, ci.arfcn, ci.cell_id, ci.lac, ci.is_registered, ci.operator_name, ci.timestamp as cell_timestamp, ci.rsrp, ci.rsrq "
            "FROM data_points dp "
            "LEFT JOIN cell_info ci ON ci.data_point_id = dp.id "
            "ORDER BY dp.timestamp ASC"
        );
        
        std::map<int, DataPoint> pointsMap;
        for (const auto& row : res) {
            int pointId = row["id"].as<int>();
            if (pointsMap.find(pointId) == pointsMap.end()) {
                DataPoint point;
                point.timestamp = row["timestamp"].as<long long>();
                point.latitude = row["latitude"].as<float>();
                point.longitude = row["longitude"].as<float>();
                point.altitude = row["altitude"].as<float>();
                point.deviceId = row["device_id"].as<std::string>();
                pointsMap[pointId] = point;
            }
            if (!row["cell_id"].is_null()) {
                CellInfo cell;
                cell.rssi = row["rssi"].as<int>();
                cell.arfcn = row["arfcn"].as<int>();
                cell.cellId = row["cell_id"].as<int>();
                cell.lac = row["lac"].as<int>();
                cell.isRegistered = row["is_registered"].as<bool>();
                if (!row["operator_name"].is_null()) cell.operator_ = row["operator_name"].as<std::string>();
                cell.timestamp = row["cell_timestamp"].as<long long>();
                if (!row["rsrp"].is_null()) cell.rsrp = row["rsrp"].as<int>();
                if (!row["rsrq"].is_null()) cell.rsrq = row["rsrq"].as<int>();
                pointsMap[pointId].cells.push_back(cell);
            }
        }
        for (auto& pair : pointsMap) {
            result.push_back(pair.second);
        }
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка загрузки данных для тепловой карты: " + std::string(e.what()));
    }
    return result;
}

void PostgresStorage::startMonitor(GeoData* geoInfo) {
    if (monitoring) return;
    
    monitoring = true;
    monitorThread = std::thread([this, geoInfo]() {
        long long lastTimestamp = getLastTimestamp();
        
        while (monitoring) {
            std::this_thread::sleep_for(std::chrono::seconds(2));
            
            if (!connected) continue;
            
            try {
                std::lock_guard<std::recursive_mutex> lock(dbMutex);
                pqxx::nontransaction txn(*conn);
                std::string query = "SELECT dp.id, dp.timestamp, dp.latitude, dp.longitude, dp.altitude, dp.device_id, "
                                   "ci.rssi, ci.arfcn, ci.cell_id, ci.lac, ci.is_registered, ci.operator_name, ci.timestamp as cell_timestamp, ci.rsrp, ci.rsrq "
                                   "FROM data_points dp "
                                   "LEFT JOIN cell_info ci ON ci.data_point_id = dp.id "
                                   "WHERE dp.timestamp > " + std::to_string(lastTimestamp) + " "
                                   "ORDER BY dp.timestamp ASC";
                
                pqxx::result res = txn.exec(query);
                
                std::map<int, DataPoint> newPoints;
                
                for (const auto& row : res) {
                    int pointId = row["id"].as<int>();
                    
                    if (newPoints.find(pointId) == newPoints.end()) {
                        DataPoint point;
                        point.timestamp = row["timestamp"].as<long long>();
                        point.latitude = row["latitude"].as<float>();
                        point.longitude = row["longitude"].as<float>();
                        point.altitude = row["altitude"].as<float>();
                        point.deviceId = row["device_id"].as<std::string>();
                        newPoints[pointId] = point;
                        
                        if (point.timestamp > lastTimestamp) {
                            lastTimestamp = point.timestamp;
                        }
                    }
                    
                    if (!row["cell_id"].is_null()) {
                        CellInfo cell;
                        cell.rssi = row["rssi"].as<int>();
                        cell.arfcn = row["arfcn"].as<int>();
                        cell.cellId = row["cell_id"].as<int>();
                        cell.lac = row["lac"].as<int>();
                        cell.isRegistered = row["is_registered"].as<bool>();
                        if (!row["operator_name"].is_null()) cell.operator_ = row["operator_name"].as<std::string>();
                        cell.timestamp = row["cell_timestamp"].as<long long>();
                        if (!row["rsrp"].is_null()) cell.rsrp = row["rsrp"].as<int>();
                        if (!row["rsrq"].is_null()) cell.rsrq = row["rsrq"].as<int>();
                        newPoints[pointId].cells.push_back(cell);
                    }
                }
                
                for (auto& pair : newPoints) {
                    geoInfo->addDataPoint(pair.second);
                }
                
                if (!newPoints.empty()) {
                    Logger::log("INFO", "Добавлено " + std::to_string(newPoints.size()) + " новых точек");
                }
            } catch (const std::exception& e) {
                Logger::log("ERROR", "Ошибка мониторинга: " + std::string(e.what()));
            }
        }
    });
}

void PostgresStorage::stopMonitor() {
    monitoring = false;
    if (monitorThread.joinable()) {
        monitorThread.join();
    }
}

int PostgresStorage::getTotalPointsCount() {
    if (!connected) return 0;
    
    try {
        std::lock_guard<std::recursive_mutex> lock(dbMutex);
        pqxx::nontransaction txn(*conn);
        pqxx::result res = txn.exec("SELECT COUNT(*) FROM data_points");
        return res[0][0].as<int>();
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка получения количества точек: " + std::string(e.what()));
    }
    return 0;
}

std::vector<MapPoint> PostgresStorage::loadAllPoints() {
    std::vector<MapPoint> points;
    
    if (!connected) return points;
    
    try {
        std::lock_guard<std::recursive_mutex> lock(dbMutex);
        pqxx::nontransaction txn(*conn);
        auto result = txn.exec(
            "SELECT DISTINCT ON (dp.latitude, dp.longitude) "
            "dp.latitude, dp.longitude, ci.rssi, dp.timestamp, dp.device_id "
            "FROM data_points dp "
            "JOIN cell_info ci ON ci.data_point_id = dp.id "
            "WHERE dp.latitude != 0 AND dp.longitude != 0 "
            "ORDER BY dp.latitude, dp.longitude, dp.timestamp DESC"
        );
        
        for (const auto& row : result) {
            MapPoint point;
            point.lat = row["latitude"].as<float>();
            point.lon = row["longitude"].as<float>();
            point.rssi = row["rssi"].as<int>();
            point.timestamp = row["timestamp"].as<long long>();
            point.deviceId = row["device_id"].as<std::string>();
            points.push_back(point);
        }
        
        Logger::log("INFO", "Загружено " + std::to_string(points.size()) + " уникальных точек для карты");
        
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка загрузки точек: " + std::string(e.what()));
    }
    
    return points;
}

std::vector<int> PostgresStorage::getUniqueArfcns() {
    std::vector<int> arfcns;
    if (!connected) return arfcns;
    
    try {
        std::lock_guard<std::recursive_mutex> lock(dbMutex);
        pqxx::nontransaction txn(*conn);
        auto result = txn.exec("SELECT DISTINCT arfcn FROM cell_info WHERE arfcn > 0 ORDER BY arfcn");
        for (const auto& row : result) {
            arfcns.push_back(row["arfcn"].as<int>());
        }
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка загрузки EARFCN: " + std::string(e.what()));
    }
    return arfcns;
}