#include "../gui/GUIApplication.h"
#include "../storage/PostgresStorage.h"
#include "../utils/Logger.h"
#include "../utils/NetworkUtils.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include "implot.h"
#include <thread>
#include <chrono>
#include <algorithm>

extern std::shared_ptr<PostgresStorage> g_storage;

GUIApplication::GUIApplication(GeoData* geoInfo) : geoInfo(geoInfo) {
    m_mapWindow = std::make_unique<MapWindow>();
}

GUIApplication::~GUIApplication() {}

void GUIApplication::renderCellTable(const std::vector<CellInfo>& cells) {
    if (cells.empty()) return;
    
    std::vector<CellInfo> sortedCells = cells;
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

void GUIApplication::renderLocationWindow() {
    ImGui::Begin("Location Information", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
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
    
    ImGui::Separator();
    
    if (!geoInfo->currentCells.empty()) {
        ImGui::Text("Current Cells (%zu):", geoInfo->currentCells.size());
        renderCellTable(geoInfo->currentCells);
    }
    
    ImGui::End();
}

void GUIApplication::renderSystemWindow() {
    ImGui::Begin("System Information", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
    
    std::string ipAddress = NetworkUtils::getIPAddress();
    std::string macAddress = NetworkUtils::getMacAddress();
    
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
}

void GUIApplication::renderSignalWindow() {
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
}

void GUIApplication::updateMapMarkers() {
    if (!m_mapWindow) return;
    
    m_mapWindow->clearMarkers();
    
    float lat = geoInfo->latValue.load();
    float lon = geoInfo->lonValue.load();
    if (lat != 0.0f || lon != 0.0f) {
        m_mapWindow->addMarker(lat, lon, "red");
        m_mapWindow->setCenter(lat, lon);
    }
    
    std::lock_guard<std::mutex> lock(geoInfo->historyMutex);
    int step = std::max(1, (int)geoInfo->dataHistory.size() / 200);
    int idx = 0;
    for (const auto& point : geoInfo->dataHistory) {
        if (idx % step == 0 && (point.latitude != 0.0f || point.longitude != 0.0f)) {
            int bestRssi = point.getMaxRssi();
            std::string color;
            if (bestRssi >= -70) color = "green";
            else if (bestRssi >= -85) color = "yellow";
            else color = "red";
            
            m_mapWindow->addMarker(point.latitude, point.longitude, color);
        }
        idx++;
    }
}

void GUIApplication::run() {
    Logger::log("INFO", "Инициализация графического интерфейса");
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        Logger::log("ERROR", "SDL_Init Error: " + std::string(SDL_GetError()));
        return;
    }
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    
    SDL_WindowFlags windowFlags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window* window = SDL_CreateWindow("Cell Signal Monitor", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1600, 900, windowFlags);
    
    if (!window) {
        Logger::log("ERROR", "Не удалось создать окно: " + std::string(SDL_GetError()));
        return;
    }
    
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) {
        Logger::log("ERROR", "Не удалось создать контекст OpenGL: " + std::string(SDL_GetError()));
        return;
    }
    
    SDL_GL_MakeCurrent(window, glContext);
    SDL_GL_SetSwapInterval(1);
    
    glewExperimental = GL_TRUE;
    GLenum glewStatus = glewInit();
    if (glewStatus != GLEW_OK) {
        Logger::log("ERROR", "Failed to initialize GLEW");
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
        Logger::log("ERROR", "Не удалось инициализировать ImGui SDL2");
        return;
    }
    
    if (!ImGui_ImplOpenGL3_Init("#version 330")) {
        Logger::log("ERROR", "Не удалось инициализировать ImGui OpenGL3");
        return;
    }
    
    if (geoInfo->latValue.load() != 0.0f || geoInfo->lonValue.load() != 0.0f) {
        m_mapWindow->setCenter(geoInfo->latValue.load(), geoInfo->lonValue.load());
        m_mapWindow->setZoomLevel(15);
    }
    
    Logger::log("INFO", "Графический интерфейс инициализирован успешно");
    
    bool isRunning = true;
    
    while (isRunning) {
        if (geoInfo->needReloadFromFile.load()) {
            if (g_storage) {
                g_storage->loadAllData(geoInfo);
            }
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
        
        updateMapMarkers();
        
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();
        
        m_mapWindow->draw();
        
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.1f, 0.1f, 0.1f, 0.7f));
        renderLocationWindow();
        ImGui::PopStyleColor();
        
        renderSystemWindow();
        renderSignalWindow();
        
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