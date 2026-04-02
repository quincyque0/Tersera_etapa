#include "FileStorage.h"
#include "../parser/JsonParser.h"
#include "../utils/Logger.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>
#include <chrono>
#include <set>

void FileStorage::loadAllData(GeoData* geoInfo) {
    std::ifstream file(JSON_FILE_PATH);
    if (!file.is_open()) {
        Logger::log("WARNING", "Не удалось открыть файл для загрузки: " + std::string(JSON_FILE_PATH));
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    if (content.empty()) {
        Logger::log("WARNING", "Файл пуст");
        return;
    }
    
    content.erase(0, content.find_first_not_of(" \t\n\r"));
    content.erase(content.find_last_not_of(" \t\n\r") + 1);
    
    std::string arrayContent = content.substr(1, content.length() - 2);
    if (arrayContent.empty()) {
        Logger::log("INFO", "Файл содержит пустой массив");
        return;
    }
    
    std::vector<std::string> jsonObjects;
    int braceLevel = 0;
    size_t startPos = 0;
    
    for (size_t i = 0; i < arrayContent.length(); ++i) {
        if (arrayContent[i] == '{') {
            if (braceLevel == 0) startPos = i;
            braceLevel++;
        } else if (arrayContent[i] == '}') {
            braceLevel--;
            if (braceLevel == 0 && i > startPos) {
                jsonObjects.push_back(arrayContent.substr(startPos, i - startPos + 1));
            }
        }
    }
    
    Logger::log("INFO", "Загружено " + std::to_string(jsonObjects.size()) + " записей из файла");
    
    int validEntries = 0;
    
    for (const auto& jsonObj : jsonObjects) {
        DataPoint point = JsonParser::parseDataPoint(jsonObj);
        
        if (!point.cells.empty()) {
            geoInfo->addDataPoint(point);
            validEntries++;
        }
    }
    
    Logger::log("INFO", "Найдено " + std::to_string(validEntries) + " записей с данными о сотах");
    Logger::log("INFO", "Всего в истории: " + std::to_string(geoInfo->dataHistory.size()) + " точек");
}

void FileStorage::appendData(GeoData* geoInfo) {
    std::ifstream file(JSON_FILE_PATH);
    if (!file.is_open()) {
        Logger::log("WARNING", "Не удалось открыть файл для добавления");
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    if (content.empty()) return;
    
    size_t lastBracePos = content.rfind('}');
    if (lastBracePos == std::string::npos) return;
    
    size_t secondLastBracePos = content.rfind('}', lastBracePos - 1);
    size_t startPos = (secondLastBracePos == std::string::npos) ? content.find('{') : secondLastBracePos;
    if (startPos == std::string::npos) return;
    
    std::string lastJsonObj = content.substr(startPos, lastBracePos - startPos + 1);
    DataPoint newPoint = JsonParser::parseDataPoint(lastJsonObj);
    
    if (!newPoint.cells.empty()) {
        geoInfo->addDataPoint(newPoint);
        Logger::log("INFO", "Добавлена новая точка. Всего: " + std::to_string(geoInfo->dataHistory.size()));
    }
}

void FileStorage::saveDataPoint(const std::string& data, GeoData* geoInfo) {
    std::string filename = JSON_FILE_PATH;
    
    std::ifstream checkFile(filename);
    bool fileExists = checkFile.good();
    checkFile.close();
    
    std::ofstream file(filename, std::ios::app);
    
    if (file.is_open()) {
        if (!fileExists) {
            file << "[\n";
            file << data << "\n";
            file << "]";
            Logger::log("INFO", "Создан новый файл с данными");
        } else {
            std::ifstream readFile(filename);
            std::stringstream content;
            content << readFile.rdbuf();
            readFile.close();
            
            std::string fileContent = content.str();
            
            if (fileContent.length() > 1 && 
                fileContent.substr(fileContent.length() - 2) == "\n]") {
                fileContent = fileContent.substr(0, fileContent.length() - 2);
                
                std::ofstream writeFile(filename);
                writeFile << fileContent;
                
                if (fileContent.length() > 2) {
                    writeFile << ",\n";
                }
                
                writeFile << data << "\n";
                writeFile << "]";
                writeFile.close();
                Logger::log("INFO", "Данные добавлены в существующий файл");
            } else {
                Logger::log("WARNING", "Файл поврежден, создается новый");
                std::ofstream newFile(filename);
                newFile << "[\n";
                newFile << data << "\n";
                newFile << "]";
                newFile.close();
            }
        }
        file.close();
    } else {
        Logger::log("ERROR", "Не удалось открыть файл для записи");
    }
}

void FileStorage::startFileMonitor(GeoData* geoInfo) {
    try {
        auto lastWriteTime = std::filesystem::last_write_time(JSON_FILE_PATH);
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            try {
                auto currentWriteTime = std::filesystem::last_write_time(JSON_FILE_PATH);    
                if (currentWriteTime != lastWriteTime) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    Logger::log("INFO", "Обнаружено изменение файла");
                    lastWriteTime = currentWriteTime;
                    geoInfo->needReloadFromFile.store(true);
                }
            } catch (const std::exception& e) {
            }
        }
    } catch (const std::exception& e) {
        Logger::log("WARNING", "Файл еще не существует, мониторинг не запущен");
    }
}