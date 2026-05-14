#ifndef GUI_APPLICATION_H
#define GUI_APPLICATION_H

#include "../core/GeoData.h"
#include "../map/MapWindow.h"
#include <memory>
#include <atomic>
#include <thread>
#include <vector>
#include <string>

class GUIApplication {
public:
    GUIApplication(GeoData* geoInfo);
    ~GUIApplication();
    void run();
    
private:
    GeoData* geoInfo;
    std::unique_ptr<MapWindow> m_mapWindow;
    
    void renderLocationWindow();
    void renderSystemWindow();
    void renderSignalWindow();
    void renderCellTable(const std::vector<CellInfo>& cells);
    void updateMapMarkers();
    
    void renderHeatmapWindow();
    void startHeatmapGeneration();
    void extractAvailableArfcns();

    int m_heatmapCriteria = 2; 
    int m_heatmapArfcnIdx = 0;
    std::vector<int> m_availableArfcns;
    int m_heatmapRadius = 20;
    
    std::atomic<bool> m_isGeneratingHeatmap{false};
    std::atomic<float> m_heatmapProgress{0.0f};
    std::thread m_heatmapThread;
    std::string m_heatmapStatus = "Not generated";
    
    std::atomic<bool> m_heatmapReadyToLoad{false};
    float m_hmLoadMinLat = 0.0f;
    float m_hmLoadMaxLat = 0.0f;
    float m_hmLoadMinLon = 0.0f;
    float m_hmLoadMaxLon = 0.0f;
    std::string m_hmLoadFilepath;
};

#endif