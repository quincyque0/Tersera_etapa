#include "../gui/GUIApplication.h"
#include "../storage/PostgresStorage.h"
#include "../utils/Logger.h"
#include "../utils/NetworkUtils.h"
#include "backends/imgui_impl_opengl3.h"
#include "backends/imgui_impl_sdl2.h"
#include "imgui.h"
#include "implot.h"
#include "stb_image_write.h"
#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <set>
#include <thread>

extern std::shared_ptr<PostgresStorage> g_storage;

GUIApplication::GUIApplication(GeoData *geoInfo) : geoInfo(geoInfo) {
  m_mapWindow = std::make_unique<MapWindow>();
}

GUIApplication::~GUIApplication() {
  if (m_isGeneratingHeatmap) {
    m_isGeneratingHeatmap = false;
  }
  if (m_heatmapThread.joinable()) {
    m_heatmapThread.join();
  }
}

void GUIApplication::renderCellTable(const std::vector<CellInfo> &cells) {
  if (cells.empty())
    return;

  std::vector<CellInfo> sortedCells = cells;
  std::sort(
      sortedCells.begin(), sortedCells.end(),
      [](const CellInfo &a, const CellInfo &b) { return a.rssi > b.rssi; });

  if (ImGui::BeginTable("Cells", 5,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Registered");
    ImGui::TableSetupColumn("RSSI (dBm)");
    ImGui::TableSetupColumn("Cell ID");
    ImGui::TableSetupColumn("ARFCN");
    ImGui::TableSetupColumn("Operator");
    ImGui::TableHeadersRow();

    for (const auto &cell : sortedCells) {
      ImGui::TableNextRow();

      ImGui::TableSetColumnIndex(0);
      ImGui::Text("%s", cell.isRegistered ? "YES" : "NO");

      ImGui::TableSetColumnIndex(1);
      if (cell.rssi >= -70)
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "%d", cell.rssi);
      else if (cell.rssi >= -85)
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "%d", cell.rssi);
      else
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%d", cell.rssi);

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
  ImGui::Begin("Location Information", nullptr,
               ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::Text("Latitude: %.6f°", geoInfo->latValue.load());
  ImGui::Text("Longitude: %.6f°", geoInfo->lonValue.load());
  ImGui::Text("Altitude: %.2f meters", geoInfo->altValue.load());

  time_t rawTime = geoInfo->timeStamp.load() / 1000;
  struct tm *timeInfo = localtime(&rawTime);
  char timeBuffer[80];
  strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", timeInfo);
  ImGui::Text("Timestamp: %s", timeBuffer);

  ImGui::Text("Device IMEI: %s", geoInfo->deviceId.c_str());

  ImGui::Separator();

  int bestRssi = geoInfo->bestRssi.load();
  int avgRssi = geoInfo->avgRssi.load();
  int worstRssi = geoInfo->worstRssi.load();

  if (bestRssi != 0) {
    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Best Signal: %d dBm", bestRssi);
    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Average Signal: %d dBm", avgRssi);
    ImGui::TextColored(ImVec4(1, 0, 0, 1), "Worst Signal: %d dBm", worstRssi);
  }

  ImGui::Separator();
  ImGui::Text("History size: %d/%d points", geoInfo->totalPoints.load(),
              MAX_HISTORY_POINTS);
  ImGui::Text("Unique cells: %zu", geoInfo->knownCellIds.size());

  ImGui::Separator();

  if (!geoInfo->currentCells.empty()) {
    ImGui::Text("Current Cells (%zu):", geoInfo->currentCells.size());
    renderCellTable(geoInfo->currentCells);
  }

  ImGui::End();
}

void GUIApplication::renderSystemWindow() {
  ImGui::Begin("System Information", nullptr,
               ImGuiWindowFlags_AlwaysAutoResize);

  std::string ipAddress = NetworkUtils::getIPAddress();
  std::string macAddress = NetworkUtils::getMacAddress();

  long long currentTime =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::system_clock::now().time_since_epoch())
          .count();
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

  if (ImGui::BeginTable("SystemInfo", 2,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
    ImGui::TableSetupColumn("Parameter");
    ImGui::TableSetupColumn("Value");
    ImGui::TableHeadersRow();
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("IP Address");
    ImGui::TableSetColumnIndex(1);
    if (!ipAddress.empty()) {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", ipAddress.c_str());
    } else {
      ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Not connected");
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("MAC Address");
    ImGui::TableSetColumnIndex(1);
    if (!macAddress.empty()) {
      ImGui::Text("%s", macAddress.c_str());
    } else {
      ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Not available");
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Last Data Update");
    ImGui::TableSetColumnIndex(1);
    if (lastUpdateTime > 0) {
      if (secondsAgo < 5) {
        ImGui::TextColored(ImVec4(0, 1, 0, 1), "%s", lastUpdateStr.c_str());
      } else if (secondsAgo < 30) {
        ImGui::TextColored(ImVec4(1, 1, 0, 1), "%s", lastUpdateStr.c_str());
      } else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", lastUpdateStr.c_str());
      }
    } else {
      ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "No data received yet");
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
      struct tm *lastTimeInfo = localtime(&lastRawTime);
      strftime(lastTimeBuffer, sizeof(lastTimeBuffer), "%Y-%m-%d %H:%M:%S",
               lastTimeInfo);
      ImGui::Text("%s", lastTimeBuffer);
    } else {
      ImGui::Text("---");
    }

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("Device Status");
    ImGui::TableSetColumnIndex(1);
    if (lastUpdateTime > 0 && secondsAgo < 60) {
      ImGui::TextColored(ImVec4(0, 1, 0, 1), "Active");
    } else if (lastUpdateTime > 0) {
      ImGui::TextColored(ImVec4(1, 1, 0, 1), "Inactive");
    } else {
      ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Unknown");
    }

    ImGui::EndTable();
  }

  ImGui::End();
}

void GUIApplication::renderSignalWindow() {
  ImGui::Begin("Signal Analysis", nullptr, ImGuiWindowFlags_None);

  ImVec2 available_size = ImGui::GetContentRegionAvail();
  if (ImPlot::BeginPlot("Signal Strength Over Time",
                        ImVec2(available_size.x, available_size.y * 0.6f))) {
    ImPlot::SetupAxes("Time (samples)", "RSSI (dBm)");
    ImPlot::SetupAxisLimits(ImAxis_X1, 0, MAX_HISTORY_POINTS, ImGuiCond_Always);
    ImPlot::SetupAxisLimits(ImAxis_Y1, -120, -50, ImGuiCond_Once);

    std::lock_guard<std::mutex> lock(geoInfo->historyMutex);

    if (!geoInfo->dataHistory.empty()) {
      std::vector<float> times;
      std::vector<float> avgSignals;
      std::vector<float> maxSignals;

      int sampleIdx = 0;
      for (const auto &point : geoInfo->dataHistory) {
        times.push_back(sampleIdx++);
        avgSignals.push_back(point.getAverageRssi());
        maxSignals.push_back(point.getMaxRssi());
      }

      if (!times.empty()) {
        ImPlot::SetNextLineStyle(ImVec4(1, 1, 0, 1), 2.0f);
        ImPlot::PlotLine("Average Signal", times.data(), avgSignals.data(),
                         times.size());
        ImPlot::SetNextLineStyle(ImVec4(0, 1, 0, 1), 1.5f);
        ImPlot::PlotLine("Best Signal", times.data(), maxSignals.data(),
                         times.size());
      }
    }

    ImPlot::EndPlot();
  }

  ImGui::End();
}

void GUIApplication::extractAvailableArfcns() {
  std::set<int> arfcns;

  if (g_storage) {
    std::vector<int> dbArfcns = g_storage->getUniqueArfcns();
    for (int a : dbArfcns)
      arfcns.insert(a);
  } else {
    std::lock_guard<std::mutex> lock(geoInfo->historyMutex);
    for (const auto &point : geoInfo->dataHistory) {
      for (const auto &cell : point.cells) {
        if (cell.arfcn > 0)
          arfcns.insert(cell.arfcn);
      }
    }
  }

  m_availableArfcns.clear();
  m_availableArfcns.push_back(-1);
  for (int a : arfcns) {
    m_availableArfcns.push_back(a);
  }
}

void GUIApplication::renderHeatmapWindow() {
  ImGui::Begin("Heatmap Generator", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  if (m_availableArfcns.empty()) {
    extractAvailableArfcns();
  }

  const char *criteria[] = {"RSRP", "RSRQ", "RSSI", "Altitude"};
  ImGui::Combo("Criteria", &m_heatmapCriteria, criteria,
               IM_ARRAYSIZE(criteria));

  std::vector<std::string> arfcnLabels;
  for (int a : m_availableArfcns) {
    if (a == -1)
      arfcnLabels.push_back("All");
    else
      arfcnLabels.push_back(std::to_string(a));
  }

  std::vector<const char *> arfcnCStrs;
  for (const auto &s : arfcnLabels)
    arfcnCStrs.push_back(s.c_str());

  ImGui::Combo("EARFCN", &m_heatmapArfcnIdx, arfcnCStrs.data(),
               arfcnCStrs.size());
  ImGui::SliderInt("Radius (m)", &m_heatmapRadius, 10, 200);

  if (m_isGeneratingHeatmap.load()) {
    float progress = m_heatmapProgress.load();
    char progressText[64];
    snprintf(progressText, sizeof(progressText), "%.2f%%", progress * 100.0f);
    ImGui::ProgressBar(progress, ImVec2(-1, 0), progressText);
    ImGui::Text("%s", m_heatmapStatus.c_str());
  } else {
    ImGui::Text("Status: %s", m_heatmapStatus.c_str());
  }

  bool isDisabled = m_isGeneratingHeatmap.load();
  if (isDisabled) {
    ImGui::BeginDisabled();
  }
  if (ImGui::Button("Generate Heatmap")) {
    startHeatmapGeneration();
  }
  if (isDisabled) {
    ImGui::EndDisabled();
  }

  if (ImGui::Button("Clear Heatmap")) {
    m_mapWindow->clearHeatmap();
  }

  if (m_heatmapReadyToLoad) {
    m_mapWindow->setHeatmap(m_hmLoadFilepath, m_hmLoadMinLat, m_hmLoadMaxLat,
                            m_hmLoadMinLon, m_hmLoadMaxLon);
    m_heatmapReadyToLoad = false;
  }

  ImGui::End();
}

void GUIApplication::startHeatmapGeneration() {
  if (m_isGeneratingHeatmap)
    return;

  m_isGeneratingHeatmap = true;
  m_heatmapProgress.store(0.0f);
  m_heatmapStatus = "Loading data";

  if (m_heatmapThread.joinable()) {
    m_heatmapThread.join();
  }

  int criteria = m_heatmapCriteria;
  int targetArfcn = m_availableArfcns[m_heatmapArfcnIdx];
  int radius = m_heatmapRadius;

  std::vector<DataPoint> dataCopy;
  if (g_storage) {
    dataCopy = g_storage->loadAllDataForHeatmap();
  } else {
    std::lock_guard<std::mutex> lock(geoInfo->historyMutex);
    dataCopy = std::vector<DataPoint>(geoInfo->dataHistory.begin(),
                                      geoInfo->dataHistory.end());
  }

  m_heatmapThread = std::thread([this, dataCopy, criteria, targetArfcn,
                                 radius]() {
    if (dataCopy.empty()) {
      m_heatmapStatus = "No data points";
      m_isGeneratingHeatmap = false;
      return;
    }

    struct Pt {
      float lat, lon, val;
    };
    std::vector<Pt> pts;

    float minLat = 90.0f, maxLat = -90.0f;
    float minLon = 180.0f, maxLon = -180.0f;

    for (const auto &dp : dataCopy) {
      if (dp.latitude == 0.0f || dp.longitude == 0.0f)
        continue;

      float val = -999.0f;
      bool found = false;

      if (criteria == 3) {
        val = dp.altitude;
        found = true;
      } else {
        for (const auto &cell : dp.cells) {
          if (targetArfcn == -1 || cell.arfcn == targetArfcn) {
            if (criteria == 0)
              val = std::max(val, (float)cell.rsrp);
            else if (criteria == 1)
              val = std::max(val, (float)cell.rsrq);
            else if (criteria == 2)
              val = std::max(val, (float)cell.rssi);
            found = true;
          }
        }
      }

      if (found && val != -999.0f) {
        if (criteria != 3 && val == 0.0f)
          continue;
        pts.push_back({dp.latitude, dp.longitude, val});
        minLat = std::min(minLat, dp.latitude);
        maxLat = std::max(maxLat, dp.latitude);
        minLon = std::min(minLon, dp.longitude);
        maxLon = std::max(maxLon, dp.longitude);
      }
    }

    if (pts.empty()) {
      m_heatmapStatus = "No matching points";
      m_isGeneratingHeatmap = false;
      return;
    }

    m_heatmapStatus =
        "Found " + std::to_string(pts.size()) + " pts, computing IDW";
    Logger::log("INFO", "Heatmap: " + std::to_string(pts.size()) +
                            " points, radius=" + std::to_string(radius) + "m");

    auto haversineMeters = [](double lat1, double lon1, double lat2,
                              double lon2) -> double {
      const double R = 6371000.0;
      double phi1 = lat1 * M_PI / 180.0;
      double phi2 = lat2 * M_PI / 180.0;
      double dPhi = (lat2 - lat1) * M_PI / 180.0;
      double dLam = (lon2 - lon1) * M_PI / 180.0;
      double a = std::sin(dPhi / 2) * std::sin(dPhi / 2) +
                 std::cos(phi1) * std::cos(phi2) * std::sin(dLam / 2) *
                     std::sin(dLam / 2);
      double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
      return R * c;
    };

    float padding = 0.0005f;
    minLat -= padding;
    maxLat += padding;
    minLon -= padding;
    maxLon += padding;

    double latDistMeters = haversineMeters(minLat, minLon, maxLat, minLon);
    double lonDistMeters = haversineMeters(minLat, minLon, minLat, maxLon);

    const double metersPerPixel = 2.0;
    const int maxResolution = 4096;
    const int minResolution = 256;

    int width = std::clamp((int)(lonDistMeters / metersPerPixel), minResolution,
                           maxResolution);
    int height = std::clamp((int)(latDistMeters / metersPerPixel),
                            minResolution, maxResolution);

    Logger::log("INFO", "Heatmap resolution: " + std::to_string(width) + "x" +
                            std::to_string(height) +
                            " (area: " + std::to_string((int)lonDistMeters) +
                            "x" + std::to_string((int)latDistMeters) + "m)");

    std::vector<uint8_t> image(width * height * 4, 0);

    auto latToMercY = [](double lat) -> double {
      return std::log(std::tan((90.0 + lat) * M_PI / 360.0)) * 180.0 / M_PI;
    };
    auto mercYToLat = [](double mercY) -> double {
      return (std::atan(std::exp(mercY * M_PI / 180.0)) * 360.0 / M_PI) - 90.0;
    };

    double mercYMin = latToMercY(minLat);
    double mercYMax = latToMercY(maxLat);

    struct Color {
      int r, g, b;
    };
    auto gradientColor = [](Color c1, Color c2, double ratio) -> Color {
      return {static_cast<int>(c1.r + (c2.r - c1.r) * ratio),
              static_cast<int>(c1.g + (c2.g - c1.g) * ratio),
              static_cast<int>(c1.b + (c2.b - c1.b) * ratio)};
    };

    Color colorExcellent = {255, 0, 0};
    Color colorPoor = {0, 0, 139};

    int filledPixels = 0;

    for (int y = 0; y < height; ++y) {
      {
        float progress = (float)y / (float)height;
        m_heatmapProgress.store(progress);
        char buf[64];
        snprintf(buf, sizeof(buf), "Computing... %.2f%%", progress * 100.0f);
        m_heatmapStatus = buf;
      }

      double mercY = mercYMax - ((double)y / height) * (mercYMax - mercYMin);
      double plat = mercYToLat(mercY);
      for (int x = 0; x < width; ++x) {
        double plon = minLon + ((double)x / width) * (maxLon - minLon);

        double sumWeight = 0.0;
        double sumVal = 0.0;

        for (const auto &p : pts) {
          double dist = haversineMeters(plat, plon, p.lat, p.lon);
          if (dist <= (double)radius) {
            double w = 1.0 / (dist * dist + 1e-6);
            sumWeight += w;
            sumVal += (double)p.val * w;
          }
        }

        if (sumWeight > 0.0) {
          double val = sumVal / sumWeight;

          double ratio = 0.0;
          bool noSignal = false;

          if (criteria == 0) {
            if (val < -110.0)
              noSignal = true;
            else
              ratio = (val - (-110.0)) / (-80.0 - (-110.0));
          } else if (criteria == 1) {
            if (val < -20.0)
              noSignal = true;
            else
              ratio = (val - (-20.0)) / (-10.0 - (-20.0));
          } else if (criteria == 2) {
            if (val < -110.0)
              noSignal = true;
            else
              ratio = (val - (-110.0)) / (-50.0 - (-110.0));
          } else {
            ratio = val / 200.0;
          }

          if (noSignal)
            continue;

          ratio = std::max(0.0, std::min(1.0, ratio));
          Color c = gradientColor(colorPoor, colorExcellent, ratio);

          int idx = (y * width + x) * 4;
          image[idx + 0] = (uint8_t)std::clamp(c.r, 0, 255);
          image[idx + 1] = (uint8_t)std::clamp(c.g, 0, 255);
          image[idx + 2] = (uint8_t)std::clamp(c.b, 0, 255);
          image[idx + 3] = 200;
          filledPixels++;
        }
      }
    }

    Logger::log("INFO", "Heatmap: filled " + std::to_string(filledPixels) +
                            " pixels out of " + std::to_string(width * height));

    std::string filepath = "heatmap.png";
    stbi_write_png(filepath.c_str(), width, height, 4, image.data(), width * 4);
    Logger::log("INFO", "Heatmap saved to " + filepath);

    m_hmLoadFilepath = filepath;
    m_hmLoadMinLat = minLat;
    m_hmLoadMaxLat = maxLat;
    m_hmLoadMinLon = minLon;
    m_hmLoadMaxLon = maxLon;
    m_heatmapReadyToLoad = true;

    m_heatmapProgress.store(1.0f);
    m_heatmapStatus = "Done (" + std::to_string(filledPixels) + " px filled)";
    m_isGeneratingHeatmap = false;
  });
}

void GUIApplication::updateMapMarkers() {
  if (!m_mapWindow)
    return;

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
  for (const auto &point : geoInfo->dataHistory) {
    if (idx % step == 0 &&
        (point.latitude != 0.0f || point.longitude != 0.0f)) {
      int bestRssi = point.getMaxRssi();
      std::string color;
      if (bestRssi >= -70)
        color = "green";
      else if (bestRssi >= -85)
        color = "yellow";
      else
        color = "red";

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

  SDL_WindowFlags windowFlags =
      (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                        SDL_WINDOW_ALLOW_HIGHDPI);
  SDL_Window *window =
      SDL_CreateWindow("Cell Signal Monitor", SDL_WINDOWPOS_CENTERED,
                       SDL_WINDOWPOS_CENTERED, 1600, 900, windowFlags);

  if (!window) {
    Logger::log("ERROR",
                "Не удалось создать окно: " + std::string(SDL_GetError()));
    return;
  }

  SDL_GLContext glContext = SDL_GL_CreateContext(window);
  if (!glContext) {
    Logger::log("ERROR", "Не удалось создать контекст OpenGL: " +
                             std::string(SDL_GetError()));
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
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  ImPlot::CreateContext();

  ImGui::StyleColorsDark();
  ImGuiStyle &style = ImGui::GetStyle();
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

  if (g_storage) {
    auto allPoints = g_storage->loadAllPoints();
    for (const auto &point : allPoints) {
      m_mapWindow->addPoint(point);
    }
    Logger::log("INFO", "Загружено " + std::to_string(allPoints.size()) +
                            " точек на карту");

    if (!allPoints.empty()) {
      float minLat = allPoints[0].lat, maxLat = allPoints[0].lat;
      float minLon = allPoints[0].lon, maxLon = allPoints[0].lon;
      for (const auto &p : allPoints) {
        minLat = std::min(minLat, p.lat);
        maxLat = std::max(maxLat, p.lat);
        minLon = std::min(minLon, p.lon);
        maxLon = std::max(maxLon, p.lon);
      }
      m_mapWindow->fitToBounds(minLat, maxLat, minLon, maxLon);
      m_mapWindow->setShowAllPoints(true);

      if (std::filesystem::exists("heatmap.png")) {
        float pad = 0.0005f;
        m_mapWindow->setHeatmap("heatmap.png", minLat - pad, maxLat + pad,
                                minLon - pad, maxLon + pad);
        m_heatmapStatus = "Loaded from cache";
        Logger::log("INFO",
                    "Загружена существующая тепловая карта heatmap.png");
      }
    }
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
      if (event.type == SDL_WINDOWEVENT &&
          event.window.event == SDL_WINDOWEVENT_CLOSE &&
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
    renderHeatmapWindow();

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