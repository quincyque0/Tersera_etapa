#ifndef GUI_APPLICATION_H
#define GUI_APPLICATION_H

#include "../core/GeoData.h"

class GUIApplication {
public:
    GUIApplication(GeoData* geoInfo);
    ~GUIApplication();
    void run();
    
private:
    GeoData* geoInfo;
    void renderLocationWindow();
    void renderSystemWindow();
    void renderSignalWindow();
    void renderCellTable(const std::vector<CellInfo>& cells);
};

#endif 