#include <SDL2/SDL.h>
#include <GL/glew.h>

#include <iostream>
#include <thread>
#include <atomic>
#include <chrono>
#include <random>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <sys/stat.h>
#include <sys/types.h>
#include <zmq.hpp>
#include <deque>
#include <mutex>
#include <vector>
#include <algorithm>
#include <filesystem>
#include <set>
#include <map>
#include "implot.h"

#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <net/if_dl.h>


#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"

#define NETWORK_PORT 5555
#define MAX_HISTORY_POINTS 500
#define JSON_FILE_PATH "../database/locations.json"

std::string getAddress(){
    struct ifaddrs *ifaddr, *ifa;
    std::string ipAddress;
    getifaddrs(&ifaddr);
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        if (strcmp(ifa->ifa_name, "en0") == 0 && 
            ifa->ifa_addr->sa_family == AF_INET) {
            
            struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
            char buffer[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &(addr->sin_addr), buffer, INET_ADDRSTRLEN);
            ipAddress = std::string(buffer);
            break;
        }
    }
    
    freeifaddrs(ifaddr);
    return ipAddress;
}
std::string getMacAddress() {
    struct ifaddrs *ifaddr, *ifa;
    std::string macAddress;
    
    getifaddrs(&ifaddr);
    
    for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == nullptr) continue;
        
        if (strcmp(ifa->ifa_name, "en0") == 0 && 
            ifa->ifa_addr->sa_family == AF_LINK) {
            
            struct sockaddr_dl* sdl = (struct sockaddr_dl*)ifa->ifa_addr;
            unsigned char* mac = (unsigned char*)LLADDR(sdl);
            
            char macBuffer[18];
            snprintf(macBuffer, sizeof(macBuffer), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            macAddress = std::string(macBuffer);
            break;
        }
    }
    
    freeifaddrs(ifaddr);
    return macAddress;
}
void logMessage(const std::string& level, const std::string& message) {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::tm now_tm = *std::localtime(&now_time_t);
    
    std::cout << "[" << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S") << "." << std::setfill('0') << std::setw(3) << now_ms.count() << "] [" << level << "] " << message << std::endl;
}

struct CellInfo {
    int rssi;                    
    int arfcn;                  
    int cellId;                 
    int lac;                  
    bool isRegistered;          
    std::string operator_;   
    long long timestamp;       
    
    CellInfo() : rssi(0), arfcn(0), cellId(0), lac(0), isRegistered(false), timestamp(0) {}
};


struct DataPoint {
    long long timestamp;
    float latitude;
    float longitude;
    float altitude;
    std::string deviceId;
    std::vector<CellInfo> cells;
    
    float getAverageRssi() const {
        if (cells.empty()) return 0;
        float sum = 0;
        for (const auto& cell : cells) {
            sum += cell.rssi;
        }
        return sum / cells.size();
    }
    
    int getMaxRssi() const {
        if (cells.empty()) return 0;
        int maxRssi = -1000;
        for (const auto& cell : cells) {
            if (cell.rssi > maxRssi) maxRssi = cell.rssi;
        }
        return maxRssi;
    }
    size_t getCellCount() const {
        return cells.size();
    }
};

struct GeoData {
    std::atomic<float> latValue;
    std::atomic<float> lonValue;
    std::atomic<float> altValue;
    std::atomic<long long> timeStamp;
    std::string deviceId;
    std::deque<DataPoint> dataHistory;
    std::mutex historyMutex;
    std::vector<CellInfo> currentCells;
    std::atomic<int> bestRssi{0};
    std::atomic<int> avgRssi{0};
    std::atomic<int> worstRssi{0};
    std::atomic<bool> needReloadFromFile{false};
    std::set<int> knownCellIds;
    std::atomic<int> totalPoints{0};
};

std::vector<CellInfo> parseCellInfo(const std::string& cellInfoStr, long timestamp);
void calculateSignalStats(const std::vector<CellInfo>& cells, int& best, int& avg, int& worst);
std::string jsonString(const std::string& str);
DataPoint parseDataPoint(const std::string& jsonStr);


std::string jsonString(const std::string& str) {
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

std::string parseJsonString(const std::string& json, const std::string& key) {
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
DataPoint parseDataPoint(const std::string& jsonStr) {
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
void loadAllDataFromJsonFile(GeoData* geoInfo) {
    std::ifstream file(JSON_FILE_PATH);
    if (!file.is_open()) {
        logMessage("WARNING", "Не удалось открыть файл для загрузки: " + std::string(JSON_FILE_PATH));
        return;
    }
    
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();
    
    if (content.empty()) {
        logMessage("WARNING", "Файл пуст");
        return;
    }
    content.erase(0, content.find_first_not_of(" \t\n\r"));
    content.erase(content.find_last_not_of(" \t\n\r") + 1);
    
    std::string arrayContent = content.substr(1, content.length() - 2);
    if (arrayContent.empty()) {
        logMessage("INFO", "Файл содержит пустой массив");
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
    
    logMessage("INFO", "Загружено " + std::to_string(jsonObjects.size()) + " записей из файла");
    
    std::deque<DataPoint> newHistory;
    std::set<int> newCellIds;
    
    int validEntries = 0;
    
    for (const auto& jsonObj : jsonObjects) {
        DataPoint point = parseDataPoint(jsonObj);
        
        if (!point.cells.empty()) {
            newHistory.push_back(point);
            

            for (const auto& cell : point.cells) {
                newCellIds.insert(cell.cellId);
            }
            
            validEntries++;
        }
    }
    
    logMessage("INFO", "Найдено " + std::to_string(validEntries) + " записей с данными о сотах");
    
    {
        std::lock_guard<std::mutex> lock(geoInfo->historyMutex);
        geoInfo->dataHistory.insert(geoInfo->dataHistory.end(), newHistory.begin(), newHistory.end());
        geoInfo->knownCellIds.insert(newCellIds.begin(), newCellIds.end());
        while (geoInfo->dataHistory.size() > MAX_HISTORY_POINTS) {
            geoInfo->dataHistory.pop_front();
        }
        geoInfo->totalPoints.store(geoInfo->dataHistory.size());
    }
    
    if (!geoInfo->dataHistory.empty()) {
        const DataPoint& lastPoint = geoInfo->dataHistory.back();
        geoInfo->latValue.store(lastPoint.latitude);
        geoInfo->lonValue.store(lastPoint.longitude);
        geoInfo->altValue.store(lastPoint.altitude);
        geoInfo->timeStamp.store(lastPoint.timestamp);
        geoInfo->deviceId = lastPoint.deviceId;
        geoInfo->currentCells = lastPoint.cells;
        int best, avg, worst;
        calculateSignalStats(lastPoint.cells, best, avg, worst);
        geoInfo->bestRssi.store(best);
        geoInfo->avgRssi.store(avg);
        geoInfo->worstRssi.store(worst);
    }
    
    logMessage("INFO", "Всего в истории: " + std::to_string(geoInfo->dataHistory.size()) + " точек");
}

void appendDataFromJsonFile(GeoData* geoInfo) {
    std::ifstream file(JSON_FILE_PATH);
    if (!file.is_open()) {
        logMessage("WARNING", "Не удалось открыть файл для добавления");
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
    DataPoint newPoint = parseDataPoint(lastJsonObj);
    if (!newPoint.cells.empty()) {
        std::lock_guard<std::mutex> lock(geoInfo->historyMutex);
        geoInfo->dataHistory.push_back(newPoint);

        for (const auto& cell : newPoint.cells) {
            geoInfo->knownCellIds.insert(cell.cellId);
        }
        geoInfo->currentCells = newPoint.cells;
        geoInfo->latValue.store(newPoint.latitude);
        geoInfo->lonValue.store(newPoint.longitude);
        geoInfo->altValue.store(newPoint.altitude);
        geoInfo->timeStamp.store(newPoint.timestamp);
        geoInfo->deviceId = newPoint.deviceId;
        int best, avg, worst;
        calculateSignalStats(newPoint.cells, best, avg, worst);
        geoInfo->bestRssi.store(best);
        geoInfo->avgRssi.store(avg);
        geoInfo->worstRssi.store(worst);
        while (geoInfo->dataHistory.size() > MAX_HISTORY_POINTS) {
            geoInfo->dataHistory.pop_front();
        }
        geoInfo->totalPoints.store(geoInfo->dataHistory.size());
        logMessage("INFO", "Добавлена новая точка. Всего: " + std::to_string(geoInfo->dataHistory.size()));
    }
}

void fileMonitorThread(GeoData* geoInfo) {
    try {
        auto lastWriteTime = std::filesystem::last_write_time(JSON_FILE_PATH);
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            
            try {
                auto currentWriteTime = std::filesystem::last_write_time(JSON_FILE_PATH);    
                if (currentWriteTime != lastWriteTime) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    logMessage("INFO", "Обнаружено изменение файла");
                    lastWriteTime = currentWriteTime;
                    geoInfo->needReloadFromFile.store(true);
                }
            } catch (const std::exception& e) {
            }
        }
    } catch (const std::exception& e) {
        logMessage("WARNING", "Файл еще не существует, мониторинг не запущен");
    }
}

std::vector<CellInfo> parseCellInfo(const std::string& cellInfoStr, long timestamp) {
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
        logMessage("ERROR", "Ошибка при парсинге cell info: " + std::string(e.what()));
    }
    
    return cells;
}

void calculateSignalStats(const std::vector<CellInfo>& cells, int& best, int& avg, int& worst) {
    if (cells.empty()) {
        best = avg = worst = 0;
        return;
    }
    
    best = -1000;
    worst = 1000;
    int sum = 0;
    
    for (const auto& cell : cells) {
        if (cell.rssi > best) best = cell.rssi;
        if (cell.rssi < worst) worst = cell.rssi;
        sum += cell.rssi;
    }
    
    avg = sum / cells.size();
}

void parseIncomingData(const std::string& rawData, GeoData* geoInfo) {
    try {
        logMessage("DEBUG", "Парсинг полученных данных: " + rawData);
        DataPoint newPoint = parseDataPoint(rawData);
        
        if (!newPoint.cells.empty()) {
            geoInfo->currentCells = newPoint.cells;
            geoInfo->latValue.store(newPoint.latitude);
            geoInfo->lonValue.store(newPoint.longitude);
            geoInfo->altValue.store(newPoint.altitude);
            geoInfo->timeStamp.store(newPoint.timestamp);
            geoInfo->deviceId = newPoint.deviceId;
            
            int best, avg, worst;
            calculateSignalStats(newPoint.cells, best, avg, worst);
            geoInfo->bestRssi.store(best);
            geoInfo->avgRssi.store(avg);
            geoInfo->worstRssi.store(worst);
            {
                std::lock_guard<std::mutex> lock(geoInfo->historyMutex);
                geoInfo->dataHistory.push_back(newPoint);

                for (const auto& cell : newPoint.cells) {
                    geoInfo->knownCellIds.insert(cell.cellId);
                }
                                while (geoInfo->dataHistory.size() > MAX_HISTORY_POINTS) {
                    geoInfo->dataHistory.pop_front();
                }
                
                geoInfo->totalPoints.store(geoInfo->dataHistory.size());
            }
            
            logMessage("INFO", "Получены данные о " + std::to_string(newPoint.cells.size()) + 
                      " сотах. Всего точек: " + std::to_string(geoInfo->totalPoints.load()));
        }
        
        logMessage("INFO", "Данные успешно распознаны");
    } catch (const std::exception& e) {
        logMessage("ERROR", "Ошибка при парсинге данных: " + std::string(e.what()));
    }
}

void displayInterface(GeoData* geoInfo) {
    logMessage("INFO", "Инициализация графического интерфейса");
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        logMessage("ERROR", "SDL_Init Error: " + std::string(SDL_GetError()));
        return;
    }
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Cell Signal Monitor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1400, 900, windowFlags);
    
    if (!window) {
        logMessage("ERROR", "Не удалось создать окно: " + std::string(SDL_GetError()));
        return;
    }
    
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        logMessage("ERROR", "Не удалось создать контекст OpenGL: " + std::string(SDL_GetError()));
        return;
    }
    
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);

    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        logMessage("ERROR", "Failed to initialize GLEW: " + std::string(reinterpret_cast<const char*>(glewGetErrorString(glewStatus))));
        return;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImPlot::CreateContext();

    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 10.0f;
    style.ChildRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.WindowPadding = ImVec2(15, 15);
    
    ImVec4 clear_color = ImVec4(0.05f, 0.63f, 0.31f, 1.00f);

    if (!ImGui_ImplSDL2_InitForOpenGL(window, glContext)) {
        logMessage("ERROR", "Не удалось инициализировать ImGui SDL2");
        return;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        logMessage("ERROR", "Не удалось инициализировать ImGui OpenGL3");
        return;
    }
    
    logMessage("INFO", "Графический интерфейс инициализирован успешно");

    loadAllDataFromJsonFile(geoInfo);
    std::string ipAddress = getAddress();
    std::string macAddress = getMacAddress(); 
    
    try {
        std::thread monitorThread(fileMonitorThread, geoInfo);
        monitorThread.detach();
    } catch (const std::exception& e) {
        logMessage("WARNING", "Не удалось запустить мониторинг файла: " + std::string(e.what()));
    }

    bool isRunning = true;
    
    while (isRunning) {
        if (geoInfo->needReloadFromFile.load()) {
            appendDataFromJsonFile(geoInfo);
            geoInfo->needReloadFromFile.store(false);
        }
        
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                isRunning = false;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && 
                event.window.windowID == SDL_GetWindowID(window))
                isRunning = false;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.7f));
        
        ImGui::Begin("Location Information", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        try {
            ImGui::Text("Latitude: %.6f°", geoInfo->latValue.load());
            ImGui::Text("Longitude: %.6f°", geoInfo->lonValue.load());
            ImGui::Text("Altitude: %.2f meters", geoInfo->altValue.load());
            
            time_t rawTime = geoInfo->timeStamp.load() / 1000;
            struct tm* timeInfo = localtime(&rawTime);
            char timeBuffer[80];
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeInfo);
            ImGui::Text("Timestamp: %s", timeBuffer);
            
            ImGui::Text("Device IMEI: %s", geoInfo->deviceId.c_str());
            
            ImGui::Separator();
            
            int bestRssi = geoInfo->bestRssi.load();
            int avgRssi = geoInfo->avgRssi.load();
            int worstRssi = geoInfo->worstRssi.load();
            
            if (bestRssi != 0) {
                ImGui::TextColored(ImVec4(0,1,0,1), "Best Signal: %d dBm", bestRssi);
                ImGui::TextColored(ImVec4(1,1,0,1), "Average Signal: %d dBm", avgRssi);
                ImGui::TextColored(ImVec4(1,0,0,1), "Worst Signal: %d dBm", worstRssi);
            }
            
            ImGui::Separator();
            ImGui::Text("History size: %d/%d points", geoInfo->totalPoints.load(), MAX_HISTORY_POINTS);
            ImGui::Text("Unique cells: %zu", geoInfo->knownCellIds.size());
            if (ImGui::Button("Reload all from file")) {
                loadAllDataFromJsonFile(geoInfo);
            }
            ImGui::SameLine();
            if (ImGui::Button("Clear history")) {
                std::lock_guard<std::mutex> lock(geoInfo->historyMutex);
                geoInfo->dataHistory.clear();
                geoInfo->knownCellIds.clear();
                geoInfo->currentCells.clear();
                geoInfo->bestRssi.store(0);
                geoInfo->avgRssi.store(0);
                geoInfo->worstRssi.store(0);
                geoInfo->totalPoints.store(0);
            }
            
            ImGui::Separator();
            
            if (!geoInfo->currentCells.empty()) {
                ImGui::Text("Current Cells (%zu):", geoInfo->currentCells.size());
                std::vector<CellInfo> sortedCells = geoInfo->currentCells;
                std::sort(sortedCells.begin(), sortedCells.end(), 
                    [](const CellInfo& a, const CellInfo& b) { return a.rssi > b.rssi; });
                
                if (ImGui::BeginTable("Cells", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                    ImGui::TableSetupColumn("Registered");
                    ImGui::TableSetupColumn("RSSI (dBm)");
                    ImGui::TableSetupColumn("Cell ID");
                    ImGui::TableSetupColumn("ARFCN");
                    ImGui::TableSetupColumn("Operator");
                    ImGui::TableHeadersRow();
                    
                    for (const auto& cell : sortedCells) {
                        ImGui::TableNextRow();
                        
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%s", cell.isRegistered ? "YES" : "NO");
                        
                        ImGui::TableSetColumnIndex(1);
                        if (cell.rssi >= -70) ImGui::TextColored(ImVec4(0,1,0,1), "%d", cell.rssi);
                        else if (cell.rssi >= -85) ImGui::TextColored(ImVec4(1,1,0,1), "%d", cell.rssi);
                        else ImGui::TextColored(ImVec4(1,0,0,1), "%d", cell.rssi);
                        
                        ImGui::TableSetColumnIndex(2);
                        ImGui::Text("%d", cell.cellId);
                        
                        ImGui::TableSetColumnIndex(3);
                        ImGui::Text("%d", cell.arfcn);
                        
                        ImGui::TableSetColumnIndex(4);
                        ImGui::Text("%s", cell.operator_.c_str());
                    }
                    
                    ImGui::EndTable();
                }
            }
            
        } catch (const std::exception& e) {
            logMessage("ERROR", "Ошибка при отображении данных: " + std::string(e.what()));
        }
        
        ImGui::Separator();
        ImGui::Text("Application average %.3f ms/frame (%.1f FPS)",
                    1000.0f / io.Framerate, io.Framerate);
        
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::Begin("System Information", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        
        long long currentTime = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        long long lastUpdateTime = geoInfo->timeStamp.load();
        long long secondsAgo = 0;
        if (lastUpdateTime > 0) {
            secondsAgo = (currentTime - lastUpdateTime) / 1000;
        }
        
        std::string lastUpdateStr;
        if (secondsAgo < 60) {
            lastUpdateStr = std::to_string(secondsAgo) + " seconds ago";
        } else if (secondsAgo < 3600) {
            lastUpdateStr = std::to_string(secondsAgo / 60) + " minutes " + 
                           std::to_string(secondsAgo % 60) + " seconds ago";
        } else {
            lastUpdateStr = std::to_string(secondsAgo / 3600) + " hours " + 
                           std::to_string((secondsAgo % 3600) / 60) + " minutes ago";
        }
        
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 1.0f, 1.0f), "Network Interface: en0");
        ImGui::Separator();
        
        if (ImGui::BeginTable("SystemInfo", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Parameter");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("IP Address");
            ImGui::TableSetColumnIndex(1);
            if (!ipAddress.empty()) {
                ImGui::TextColored(ImVec4(0,1,0,1), "%s", ipAddress.c_str());
            } else {
                ImGui::TextColored(ImVec4(1,0.5f,0,1), "Not connected");
            }
            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("MAC Address");
            ImGui::TableSetColumnIndex(1);
            if (!macAddress.empty()) {
                ImGui::Text("%s", macAddress.c_str());
            } else {
                ImGui::TextColored(ImVec4(1,0.5f,0,1), "Not available");
            }
            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Last Data Update");
            ImGui::TableSetColumnIndex(1);
            if (lastUpdateTime > 0) {
                if (secondsAgo < 5) {
                    ImGui::TextColored(ImVec4(0,1,0,1), "%s", lastUpdateStr.c_str());
                } else if (secondsAgo < 30) {
                    ImGui::TextColored(ImVec4(1,1,0,1), "%s", lastUpdateStr.c_str());
                } else {
                    ImGui::TextColored(ImVec4(1,0,0,1), "%s", lastUpdateStr.c_str());
                }
            } else {
                ImGui::TextColored(ImVec4(1,0.5f,0,1), "No data received yet");
            }
            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Total Data Points");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%d", geoInfo->totalPoints.load());
            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Last Timestamp");
            ImGui::TableSetColumnIndex(1);
            if (lastUpdateTime > 0) {
                char lastTimeBuffer[80];
                time_t lastRawTime = lastUpdateTime / 1000;
                struct tm* lastTimeInfo = localtime(&lastRawTime);
                strftime(lastTimeBuffer, sizeof(lastTimeBuffer), "%Y-%m-%d %H:%M:%S", lastTimeInfo);
                ImGui::Text("%s", lastTimeBuffer);
            } else {
                ImGui::Text("---");
            }
            
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Device Status");
            ImGui::TableSetColumnIndex(1);
            if (lastUpdateTime > 0 && secondsAgo < 60) {
                ImGui::TextColored(ImVec4(0,1,0,1), "Active");
            } else if (lastUpdateTime > 0) {
                ImGui::TextColored(ImVec4(1,1,0,1), "Inactive");
            } else {
                ImGui::TextColored(ImVec4(1,0.5f,0,1), "Unknown");
            }
            
            ImGui::EndTable();
        }
        
        ImGui::End();
        
        ImGui::Begin("Signal Analysis", nullptr, ImGuiWindowFlags_None);
        
        ImVec2 available_size = ImGui::GetContentRegionAvail();
        if (ImPlot::BeginPlot("Signal Strength Over Time", ImVec2(available_size.x, available_size.y * 0.6f))) {
            ImPlot::SetupAxes("Time (samples)", "RSSI (dBm)");
            ImPlot::SetupAxisLimits(ImAxis_X1, 0, MAX_HISTORY_POINTS, ImGuiCond_Always);
            ImPlot::SetupAxisLimits(ImAxis_Y1, -120, -50, ImGuiCond_Once);
            
            std::lock_guard<std::mutex> lock(geoInfo->historyMutex);
            
            if (!geoInfo->dataHistory.empty()) {
                std::vector<float> times;
                std::vector<float> avgSignals;
                std::vector<float> maxSignals;
                
                int sampleIdx = 0;
                for (const auto& point : geoInfo->dataHistory) {
                    times.push_back(sampleIdx++);
                    avgSignals.push_back(point.getAverageRssi());
                    maxSignals.push_back(point.getMaxRssi());
                }
                
                if (!times.empty()) {
                    ImPlot::SetNextLineStyle(ImVec4(1,1,0,1), 2.0f); 
                    ImPlot::PlotLine("Average Signal", times.data(), avgSignals.data(), times.size());
                    ImPlot::SetNextLineStyle(ImVec4(0,1,0,1), 1.5f);
                    ImPlot::PlotLine("Best Signal", times.data(), maxSignals.data(), times.size());
                }
            }
            
            ImPlot::EndPlot();
        }
        
        ImGui::End();

        ImGui::Render();
        
        int displayWidth, displayHeight;
        SDL_GetWindowSize(window, &displayWidth, &displayHeight);
        glViewport(0, 0, displayWidth, displayHeight);
        glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void startNetworkServer(GeoData* geoInfo) {
    logMessage("INFO", "Запуск сетевого сервера на порту " + std::to_string(NETWORK_PORT));
    
    try {
        zmq::context_t context(1);
        zmq::socket_t socket(context, ZMQ_REP);
        
        std::string bindAddress = "tcp://*:" + std::to_string(NETWORK_PORT);
        socket.bind(bindAddress);
        logMessage("INFO", "Сервер успешно запущен");

        struct stat dirStat = {0};
        if (stat("../database", &dirStat) == -1) {
            mkdir("../database", 0700);
        }

        while (true) {
            try {
                zmq::message_t request;
                auto receiveResult = socket.recv(request, zmq::recv_flags::none);
                
                if (receiveResult) {
                    std::string receivedData(static_cast<char*>(request.data()), request.size());
                    logMessage("INFO", "Получены данные от клиента");
                    
                    parseIncomingData(receivedData, geoInfo);
                    std::string filename = JSON_FILE_PATH;
                    
                    std::ifstream checkFile(filename);
                    bool fileExists = checkFile.good();
                    checkFile.close();

                    std::ofstream file(filename, std::ios::app);
                    
                    if (file.is_open()) {
                        if (!fileExists) {
                            file << "[\n";
                            file << receivedData << "\n";
                            file << "]";
                            logMessage("INFO", "Создан новый файл с данными");
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

                                writeFile << receivedData << "\n";
                                writeFile << "]";
                                writeFile.close();
                                logMessage("INFO", "Данные добавлены в существующий файл");
                            } else {
                                logMessage("WARNING", "Файл поврежден, создается новый");
                                std::ofstream newFile(filename);
                                newFile << "[\n";
                                newFile << receivedData << "\n";
                                newFile << "]";
                                newFile.close();
                            }
                        }
                        file.close();
                        
                        std::string response = "Successful sending";
                        socket.send(zmq::buffer(response), zmq::send_flags::none);
                        
                    } else {
                        logMessage("ERROR", "Не удалось открыть файл для записи");
                        std::string response = "Unexpected error";
                        socket.send(zmq::buffer(response), zmq::send_flags::none);
                    }
                }
                
            } catch (const zmq::error_t& e) {
                logMessage("ERROR", "Ошибка ZeroMQ: " + std::string(e.what()));
            }
        }
    } catch (const std::exception& e) {
        logMessage("ERROR", "Критическая ошибка: " + std::string(e.what()));
    }
}

int main() {
    logMessage("INFO", "Запуск приложения");
    
    GeoData geoInfo;
    
    try {
        geoInfo.latValue.store(0.0f);
        geoInfo.lonValue.store(0.0f);
        geoInfo.altValue.store(0.0f);
        geoInfo.timeStamp.store(0);
        geoInfo.deviceId = "None";
        geoInfo.bestRssi.store(0);
        geoInfo.avgRssi.store(0);
        geoInfo.worstRssi.store(0);
        geoInfo.needReloadFromFile.store(false);
        geoInfo.totalPoints.store(0);
        
        logMessage("INFO", "Запуск серверного потока");
        std::thread serverThread(startNetworkServer, &geoInfo);
        
        logMessage("INFO", "Запуск графического интерфейса");
        displayInterface(&geoInfo);
        
        serverThread.join();
        
    } catch (const std::exception& e) {
        logMessage("ERROR", "Ошибка в main: " + std::string(e.what()));
        return 1;
    }
    
    logMessage("INFO", "Приложение завершено");
    return 0;
}