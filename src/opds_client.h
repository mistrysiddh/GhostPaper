#pragma once
#include <Arduino.h>
#include <vector>

#include <functional>

struct OpdsEntry {
    String title;
    String author;
    String downloadUrl;
    String format; // "txt"
};

class OpdsClient {
private:
    String errorMsg;
public:
    void begin();
    std::vector<OpdsEntry> fetchCatalog(String url);
    String resolveBookUrl(String startUrl);
    bool downloadBook(String url, String targetPath, std::function<void(int)> progressCallback = nullptr);
    String getLastError() { return errorMsg; }

    // Progress tracking
    int downloadProgress = -1; 
    int totalBytes = 0;
    int currentBytes = 0;
};