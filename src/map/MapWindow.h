#ifndef MAP_WINDOW_H
#define MAP_WINDOW_H

#include <map>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>
#include <GL/glew.h>
#include <curl/curl.h>

struct TileJob {
    std::string id;
    int zoom;
    int x;
    int y;
};

struct TextureData {
    GLuint id = 0;
    bool isLoading = false;
    std::vector<uint8_t> rgbaBlob;
    int width = 0;
    int height = 0;
};

struct MapPoint {
    float lat;
    float lon;
    int rssi;
    long long timestamp;
    std::string deviceId;
};

class MapWindow {
public:
    MapWindow();
    ~MapWindow();
    
    void draw();
    void setCenter(float lat, float lon);
    void addMarker(float lat, float lon, const std::string& color = "red");
    void addPoint(const MapPoint& point);
    void clearMarkers();
    void clearPoints();
    void setZoomLevel(int zoom);
    void setShowAllPoints(bool show);
    int getZoomLevel() const { return m_currentZoom; }
    void fitToBounds(float minLat, float maxLat, float minLon, float maxLon);
    
    void setHeatmap(const std::string& imagePath, double minLat, double maxLat, double minLon, double maxLon);
    void clearHeatmap();
    
private:
    std::map<std::string, TextureData> m_tileCache;
    std::queue<TileJob> m_jobQueue;
    std::mutex m_jobMutex;
    std::mutex m_cacheMutex;
    std::atomic<bool> m_running;
    std::thread m_workerThread;
    
    struct Marker {
        float lat;
        float lon;
        std::string color;
    };
    std::vector<Marker> m_markers;
    std::mutex m_markersMutex;
    
    std::vector<MapPoint> m_allPoints;
    std::mutex m_pointsMutex;
    bool m_showAllPoints;
    
    float m_centerLat;
    float m_centerLon;
    int m_currentZoom;
    bool m_plotInitialized;
    
    GLuint m_heatmapTextureId = 0;
    double m_hmMinLat = 0.0;
    double m_hmMaxLat = 0.0;
    double m_hmMinLon = 0.0;
    double m_hmMaxLon = 0.0;
    bool m_showHeatmap = false;
    
    double mercatorXToTileX(double mercatorX, int zoom);
    double mercatorYToTileY(double mercatorY, int zoom);
    double tileXToMercatorX(int tileX, int zoom);
    double tileYToMercatorY(int tileY, int zoom);
    double latToMercator(double lat);
    double lonToMercator(double lon);
    
    void fetchWorker();
    static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
};

#endif