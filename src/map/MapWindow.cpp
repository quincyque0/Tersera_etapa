#include "MapWindow.h"
#include "imgui.h"
#include <implot.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stb_image.h>


double MapWindow::latToMercator(double lat) {
    double y = std::log(std::tan((90.0 + lat) * M_PI / 360.0)) * 180.0 / M_PI;
    return y;
}

double MapWindow::lonToMercator(double lon) {
    return lon;
}

double MapWindow::mercatorXToTileX(double mercatorX, int zoom) {
    return (0.5 + mercatorX / 360.0) * (1 << zoom);
}

double MapWindow::mercatorYToTileY(double mercatorY, int zoom) {
    return (0.5 - mercatorY / 360.0) * (1 << zoom);
}

double MapWindow::tileXToMercatorX(int tileX, int zoom) {
    return (tileX / static_cast<double>(1 << zoom) - 0.5) * 360.0;
}

double MapWindow::tileYToMercatorY(int tileY, int zoom) {
    return (0.5 - tileY / static_cast<double>(1 << zoom)) * 360.0;
}

size_t MapWindow::curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    auto* vec = static_cast<std::vector<uint8_t>*>(userp);
    size_t total = size * nmemb;
    vec->insert(vec->end(), static_cast<uint8_t*>(contents), static_cast<uint8_t*>(contents) + total);
    return total;
}

void MapWindow::fetchWorker() {
    CURL* curl = curl_easy_init();
    
    while (m_running) {
        TileJob job;
        bool hasJob = false;
        
        {
            std::lock_guard<std::mutex> lock(m_jobMutex);
            if (!m_jobQueue.empty()) {
                job = m_jobQueue.front();
                m_jobQueue.pop();
                hasJob = true;
            }
        }

        if (!hasJob) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        std::string dir = "cache/" + std::to_string(job.zoom) + "/" + std::to_string(job.x);
        std::string file_path = dir + "/" + std::to_string(job.y) + ".png";

        std::vector<uint8_t> file_data;
        
        if (std::filesystem::exists(file_path)) {
            std::ifstream file(file_path, std::ios::binary | std::ios::ate);
            if (file.is_open()) {
                std::streamsize size = file.tellg();
                file.seekg(0, std::ios::beg);
                file_data.resize(size);
                file.read(reinterpret_cast<char*>(file_data.data()), size);
            }
        } else {
            std::string url = "https://tile.openstreetmap.org/" + std::to_string(job.zoom) + "/" +
                            std::to_string(job.x) + "/" + std::to_string(job.y) + ".png";
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_USERAGENT, "CellMonitor/1.0");
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &file_data);
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
            
            if (curl_easy_perform(curl) == CURLE_OK && !file_data.empty()) {
                std::error_code ec;
                std::filesystem::create_directories(dir, ec);
                std::ofstream file(file_path, std::ios::binary);
                if (file.is_open()) {
                    file.write(reinterpret_cast<const char*>(file_data.data()), file_data.size());
                }
            }
        }

        if (!file_data.empty()) {
            int w, h, c;
            stbi_set_flip_vertically_on_load(false);
            unsigned char* ptr = stbi_load_from_memory(file_data.data(), file_data.size(), &w, &h, &c, 4);
            
            if (ptr) {
                std::lock_guard<std::mutex> lock(m_cacheMutex);
                auto& tex = m_tileCache[job.id];
                tex.width = w;
                tex.height = h;
                tex.rgbaBlob.assign(ptr, ptr + w * h * 4);
                tex.isLoading = false;
                stbi_image_free(ptr);
                continue;
            }
        }
        
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        m_tileCache[job.id].isLoading = false;
    }
    
    curl_easy_cleanup(curl);
}

MapWindow::MapWindow() 
    : m_running(true)
    , m_centerLat(55.7558)
    , m_centerLon(37.6173)
    , m_currentZoom(10)
    , m_plotInitialized(false)
    , m_showAllPoints(false) {
    m_workerThread = std::thread(&MapWindow::fetchWorker, this);
}

MapWindow::~MapWindow() {
    m_running = false;
    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }
}

void MapWindow::setCenter(float lat, float lon) {
    m_centerLat = lat;
    m_centerLon = lon;
}

void MapWindow::setZoomLevel(int zoom) {
    m_currentZoom = std::clamp(zoom, 1, 18);
}

void MapWindow::setShowAllPoints(bool show) {
    m_showAllPoints = show;
}

void MapWindow::fitToBounds(float minLat, float maxLat, float minLon, float maxLon) {
    m_centerLat = (minLat + maxLat) / 2.0f;
    m_centerLon = (minLon + maxLon) / 2.0f;
    
    
    m_plotInitialized = false; 
    
    double mercYMin = latToMercator(minLat);
    double mercYMax = latToMercator(maxLat);
    
    ImPlot::SetNextAxesLimits(
        minLon - 0.05, 
        maxLon + 0.05,
        mercYMin - 0.05,
        mercYMax + 0.05,
        ImPlotCond_Always
    );
}

void MapWindow::setHeatmap(const std::string& imagePath, double minLat, double maxLat, double minLon, double maxLon) {
    clearHeatmap();
    
    int w, h, c;
    stbi_set_flip_vertically_on_load(false);
    unsigned char* data = stbi_load(imagePath.c_str(), &w, &h, &c, 4);
    if (data) {
        glGenTextures(1, &m_heatmapTextureId);
        glBindTexture(GL_TEXTURE_2D, m_heatmapTextureId);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        stbi_image_free(data);
        
        m_hmMinLat = minLat;
        m_hmMaxLat = maxLat;
        m_hmMinLon = minLon;
        m_hmMaxLon = maxLon;
        m_showHeatmap = true;
    }
}

void MapWindow::clearHeatmap() {
    if (m_heatmapTextureId != 0) {
        glDeleteTextures(1, &m_heatmapTextureId);
        m_heatmapTextureId = 0;
    }
    m_showHeatmap = false;
}

void MapWindow::addMarker(float lat, float lon, const std::string& color) {
    std::lock_guard<std::mutex> lock(m_markersMutex);
    m_markers.push_back({lat, lon, color});
}

void MapWindow::addPoint(const MapPoint& point) {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    m_allPoints.push_back(point);
}

void MapWindow::clearMarkers() {
    std::lock_guard<std::mutex> lock(m_markersMutex);
    m_markers.clear();
}

void MapWindow::clearPoints() {
    std::lock_guard<std::mutex> lock(m_pointsMutex);
    m_allPoints.clear();
}


void MapWindow::draw() {
    ImGui::Begin("Map Window");
    
    if (ImGui::Checkbox("Show Points", &m_showAllPoints)) {
        if (m_showAllPoints) {
            setZoomLevel(12);
        }
    }
    

    
    ImGui::Separator();
    
    ImVec2 mapSize = ImGui::GetContentRegionAvail();
    if (mapSize.x < 100 || mapSize.y < 100) {
        ImGui::End();
        return;
    }
    
    if (!m_plotInitialized) {
        double mercY = latToMercator(m_centerLat);
        ImPlot::SetNextAxesLimits(
            m_centerLon - 0.1, 
            m_centerLon + 0.1,
            mercY - 0.1,
            mercY + 0.1,
            ImPlotCond_Once
        );
        m_plotInitialized = true;
    }

    if (ImPlot::BeginPlot("##Map", mapSize, ImPlotFlags_NoLegend | ImPlotFlags_Equal)) {
        ImPlotRect limits = ImPlot::GetPlotLimits();
        
        int zoom = m_currentZoom;
        double lonRange = limits.X.Max - limits.X.Min;
        
        if (lonRange > 90.0) zoom = 4;
        else if (lonRange > 45.0) zoom = 5;
        else if (lonRange > 22.5) zoom = 6;
        else if (lonRange > 11.25) zoom = 7;
        else if (lonRange > 5.62) zoom = 8;
        else if (lonRange > 2.81) zoom = 9;
        else if (lonRange > 1.40) zoom = 10;
        else if (lonRange > 0.70) zoom = 11;
        else if (lonRange > 0.35) zoom = 12;
        else if (lonRange > 0.17) zoom = 13;
        else if (lonRange > 0.08) zoom = 14;
        else if (lonRange > 0.04) zoom = 15;
        else if (lonRange > 0.02) zoom = 16;
        else if (lonRange > 0.01) zoom = 17;
        else zoom = 18;
        
        m_currentZoom = zoom;
        
        int minX = static_cast<int>(std::floor(mercatorXToTileX(limits.X.Min, zoom)));
        int minY = static_cast<int>(std::floor(mercatorYToTileY(limits.Y.Max, zoom)));
        int maxX = static_cast<int>(std::floor(mercatorXToTileX(limits.X.Max, zoom)));
        int maxY = static_cast<int>(std::floor(mercatorYToTileY(limits.Y.Min, zoom)));

        int maxTileCount = (1 << zoom) - 1;
        minX = std::max(0, minX);
        maxX = std::min(maxTileCount, maxX);
        minY = std::max(0, minY);
        maxY = std::min(maxTileCount, maxY);

        for (int tileX = minX; tileX <= maxX; tileX++) {
            for (int tileY = minY; tileY <= maxY; tileY++) {
                std::string tileId = std::to_string(zoom) + "/" + std::to_string(tileX) + "/" + std::to_string(tileY);
                
                bool needToLoad = false;
                {
                    std::lock_guard<std::mutex> lock(m_cacheMutex);
                    auto& tex = m_tileCache[tileId];
                    
                    if (tex.id == 0 && !tex.isLoading && tex.rgbaBlob.empty()) {
                        tex.isLoading = true;
                        needToLoad = true;
                    }
                }

                if (needToLoad) {
                    std::lock_guard<std::mutex> lock(m_jobMutex);
                    m_jobQueue.push({tileId, zoom, tileX, tileY});
                }

                GLuint gpuId = 0;
                {
                    std::lock_guard<std::mutex> lock(m_cacheMutex);
                    auto& tex = m_tileCache[tileId];
                    
                    if (!tex.rgbaBlob.empty() && tex.id == 0) {
                        glGenTextures(1, &tex.id);
                        glBindTexture(GL_TEXTURE_2D, tex.id);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex.width, tex.height, 0, 
                                    GL_RGBA, GL_UNSIGNED_BYTE, tex.rgbaBlob.data());
                        tex.rgbaBlob.clear();
                    }
                    gpuId = tex.id;
                }

                if (gpuId != 0) {
                    ImPlotPoint minPoint{ tileXToMercatorX(tileX, zoom), tileYToMercatorY(tileY + 1, zoom) };
                    ImPlotPoint maxPoint{ tileXToMercatorX(tileX + 1, zoom), tileYToMercatorY(tileY, zoom) };
                    ImPlot::PlotImage(("##tile_" + tileId).c_str(), (ImTextureID)(intptr_t)gpuId, minPoint, maxPoint);
                }
            }
        }
        
        if (m_showHeatmap && m_heatmapTextureId != 0) {
            ImPlotPoint minPoint{ m_hmMinLon, latToMercator(m_hmMinLat) };
            ImPlotPoint maxPoint{ m_hmMaxLon, latToMercator(m_hmMaxLat) };
            
            ImVec4 tint = ImVec4(1, 1, 1, 0.7f); 
            ImPlot::PlotImage("##heatmap", (ImTextureID)(intptr_t)m_heatmapTextureId, minPoint, maxPoint, ImVec2(0,0), ImVec2(1,1), tint);
        }
        
        if (m_showAllPoints) {
            std::lock_guard<std::mutex> lock(m_pointsMutex);
            for (const auto& point : m_allPoints) {
                double mercY = latToMercator(point.lat);
                ImVec4 color;
                if (point.rssi >= -70) color = ImVec4(0, 1, 0, 0.6f);
                else if (point.rssi >= -85) color = ImVec4(1, 1, 0, 0.6f);
                else color = ImVec4(1, 0, 0, 0.6f);
                
                float lonVal = static_cast<float>(point.lon);
                float mercYVal = static_cast<float>(mercY);
                
                ImPlot::PushStyleColor(ImPlotCol_MarkerFill, color);
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 4.0f);
                ImPlot::PlotScatter(("##point_" + std::to_string(point.timestamp)).c_str(), &lonVal, &mercYVal, 1);
                ImPlot::PopStyleColor();
            }
        }
        
        if (m_showAllPoints) {
            std::lock_guard<std::mutex> lock(m_markersMutex);
            for (const auto& marker : m_markers) {
                double mercY = latToMercator(marker.lat);
                ImVec4 color;
                if (marker.color == "red") color = ImVec4(1, 0, 0, 1);
                else if (marker.color == "green") color = ImVec4(0, 1, 0, 1);
                else if (marker.color == "yellow") color = ImVec4(1, 1, 0, 1);
                else color = ImVec4(1, 0, 0, 1);
                
                float lonVal = static_cast<float>(marker.lon);
                float mercYVal = static_cast<float>(mercY);
                
                ImPlot::PushStyleColor(ImPlotCol_MarkerFill, color);
                ImPlot::SetNextMarkerStyle(ImPlotMarker_Circle, 8.0f);
                ImPlot::PlotScatter(("##marker_" + std::to_string(marker.lat) + "_" + std::to_string(marker.lon)).c_str(), 
                                    &lonVal, &mercYVal, 1);
                ImPlot::PopStyleColor();
            }
        }
        
        ImPlot::EndPlot();
    }
    
    ImGui::SliderInt("Zoom Level", &m_currentZoom, 1, 18);
    ImGui::Text("Center: %.6f, %.6f", m_centerLat, m_centerLon);
    ImGui::Text("Points on map: %zu", m_allPoints.size());
    
    ImGui::End();
}