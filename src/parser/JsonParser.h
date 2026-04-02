#ifndef JSON_PARSER_H
#define JSON_PARSER_H

#include <string>
#include "../core/DataPoint.h"
#include "../core/CellInfo.h"

class JsonParser {
public:
    static DataPoint parseDataPoint(const std::string& jsonStr);
    static std::vector<CellInfo> parseCellInfo(const std::string& cellInfoStr, long timestamp);
    static std::string jsonString(const std::string& str);
    static std::string parseJsonString(const std::string& json, const std::string& key);
};

#endif 