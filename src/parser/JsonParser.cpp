#include "JsonParser.h"
#include "../utils/Logger.h"
#include <sstream>
#include <vector>

std::string JsonParser::jsonString(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.length(); ++i) {
        if (str[i] == '\\' && i + 1 < str.length()) {
            switch (str[i + 1]) {
                case 'n': result += '\n'; i++; break;
                case 't': result += '\t'; i++; break;
                case 'r': result += '\r'; i++; break;
                case '"': result += '"'; i++; break;
                case '\\': result += '\\'; i++; break;
                default: result += str[i];
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string JsonParser::parseJsonString(const std::string& json, const std::string& key) {
    size_t keyPos = json.find("\"" + key + "\"");
    if (keyPos == std::string::npos) return "";
    
    size_t colonPos = json.find(":", keyPos);
    if (colonPos == std::string::npos) return "";
    
    size_t valueStart = json.find_first_not_of(" \t", colonPos + 1);
    if (valueStart == std::string::npos) return "";
    
    if (json[valueStart] == '"') {
        size_t valueEnd = json.find("\"", valueStart + 1);
        if (valueEnd == std::string::npos) return "";
        return json.substr(valueStart + 1, valueEnd - valueStart - 1);
    } else {
        size_t valueEnd = json.find_first_of(",}] \t\n", valueStart);
        if (valueEnd == std::string::npos) return "";
        return json.substr(valueStart, valueEnd - valueStart);
    }
}

DataPoint JsonParser::parseDataPoint(const std::string& jsonStr) {
    DataPoint point;
    
    point.latitude = 0;
    point.longitude = 0;
    point.altitude = 0;
    point.timestamp = 0;
    
    std::string latStr = parseJsonString(jsonStr, "latitude");
    std::string lonStr = parseJsonString(jsonStr, "longitude");
    std::string altStr = parseJsonString(jsonStr, "altitude");
    std::string tsStr = parseJsonString(jsonStr, "timestamp");
    std::string imeiStr = parseJsonString(jsonStr, "imei");
    std::string cellInfoStr = parseJsonString(jsonStr, "cellInfo");
    
    if (!latStr.empty()) point.latitude = std::stof(latStr);
    if (!lonStr.empty()) point.longitude = std::stof(lonStr);
    if (!altStr.empty()) point.altitude = std::stof(altStr);
    if (!tsStr.empty()) point.timestamp = std::stoll(tsStr);
    if (!imeiStr.empty()) point.deviceId = imeiStr;
    
    if (!cellInfoStr.empty()) {
        std::string unescaped = jsonString(cellInfoStr);
        point.cells = parseCellInfo(unescaped, point.timestamp);
    }
    
    return point;
}

std::vector<CellInfo> JsonParser::parseCellInfo(const std::string& cellInfoStr, long timestamp) {
    std::vector<CellInfo> cells;
    
    try {
        std::stringstream ss(cellInfoStr);
        std::string cellEntry;
        while (std::getline(ss, cellEntry, '\n')) {
            if (cellEntry.empty()) continue;
            
            CellInfo cell;
            cell.timestamp = timestamp;
            size_t rssiPos = cellEntry.find("rssi=");
            if (rssiPos != std::string::npos) {
                size_t valueStart = rssiPos + 5;
                size_t valueEnd = cellEntry.find_first_of(" \n\r", valueStart);
                if (valueEnd != std::string::npos) {
                    std::string rssiStr = cellEntry.substr(valueStart, valueEnd - valueStart);
                    cell.rssi = std::stoi(rssiStr);
                }
            }
            size_t arfcnPos = cellEntry.find("mArfcn=");
            if (arfcnPos != std::string::npos) {
                size_t valueStart = arfcnPos + 7;
                size_t valueEnd = cellEntry.find_first_of(" \n\r", valueStart);
                if (valueEnd != std::string::npos) {
                    std::string arfcnStr = cellEntry.substr(valueStart, valueEnd - valueStart);
                    cell.arfcn = std::stoi(arfcnStr);
                }
            }
            
            size_t cidPos = cellEntry.find("mCid=");
            if (cidPos != std::string::npos) {
                size_t valueStart = cidPos + 5;
                size_t valueEnd = cellEntry.find_first_of(" \n\r", valueStart);
                if (valueEnd != std::string::npos) {
                    std::string cidStr = cellEntry.substr(valueStart, valueEnd - valueStart);
                    cell.cellId = std::stoi(cidStr);
                }
            }
            
            size_t lacPos = cellEntry.find("mLac=");
            if (lacPos != std::string::npos) {
                size_t valueStart = lacPos + 5;
                size_t valueEnd = cellEntry.find_first_of(" \n\r", valueStart);
                if (valueEnd != std::string::npos) {
                    std::string lacStr = cellEntry.substr(valueStart, valueEnd - valueStart);
                    cell.lac = std::stoi(lacStr);
                }
            }
            cell.isRegistered = (cellEntry.find("mRegistered=YES") != std::string::npos);
            
            size_t operatorPos = cellEntry.find("mAlphaLong=");
            if (operatorPos != std::string::npos) {
                size_t valueStart = operatorPos + 11;
                size_t valueEnd = cellEntry.find_first_of(" \n\r", valueStart);
                if (valueEnd != std::string::npos) {
                    cell.operator_ = cellEntry.substr(valueStart, valueEnd - valueStart);
                    if (!cell.operator_.empty() && cell.operator_.front() == '"') {
                        cell.operator_ = cell.operator_.substr(1);
                    }
                    if (!cell.operator_.empty() && cell.operator_.back() == '"') {
                        cell.operator_.pop_back();
                    }
                }
            }
            
            if (cell.rssi != 0) {
                cells.push_back(cell);
            }
        }
    } catch (const std::exception& e) {
        Logger::log("ERROR", "Ошибка при парсинге cell info: " + std::string(e.what()));
    }
    
    return cells;
}