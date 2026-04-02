#ifndef CELLINFO_H
#define CELLINFO_H

#include <string>

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

#endif