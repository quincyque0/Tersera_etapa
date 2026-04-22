#ifndef GUI_APPLICATION_H
#define GUI_APPLICATION_H

#include "../core/GeoData.h"
#include "../map/MapWindow.h"
#include <memory>

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
};

#endif