#ifndef SD_AU_H
#define SD_AU_H

#include "pinos.h"
#include <SD.h>
#include <SPI.h>

class SDAU {
private:
    bool sdInitialized;
    
public:
    SDAU();
    bool begin();
    bool isInitialized();
    bool fileExists(const char* filename);
    bool readFile(const char* filename, String& content);
    bool writeFile(const char* filename, const String& content);
    bool deleteFile(const char* filename);
    void listFiles();
};

#endif
