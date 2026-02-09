#pragma once
#include <Arduino.h>
#include <vector>

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
    String resolveBookUrl(String bookOpdsUrl); // New helper
    bool downloadBook(String url, String targetPath);
    String getLastError() { return errorMsg; }
};
