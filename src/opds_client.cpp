#include "opds_client.h"
#include "common.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <tinyxml2.h>
#include <SD.h>

using namespace tinyxml2;

void OpdsClient::begin() {
    // WiFi assumed connected
}

// Internal helper to find first child matching name regardless of namespace
XMLElement* findTag(XMLElement* parent, const char* name) {
    if (!parent) return NULL;
    XMLElement* child = parent->FirstChildElement();
    while (child) {
        const char* cName = child->Name();
        if (cName && strstr(cName, name)) return child;
        child = child->NextSiblingElement();
    }
    return NULL;
}

std::vector<OpdsEntry> OpdsClient::fetchCatalog(String url) {
    std::vector<OpdsEntry> entries;
    errorMsg = ""; 

    if (WiFi.status() != WL_CONNECTED) {
        errorMsg = "WiFi Not Connected";
        return entries;
    }

    Serial.printf("OPDS: Fetching %s\n", url.c_str());

    WiFiClientSecure secureClient;
    secureClient.setInsecure();
    WiFiClient client;
    
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(15000); 
    http.setUserAgent("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36"); 
    
    const char * headerKeys[] = {"Location"};
    http.collectHeaders(headerKeys, 1);

    bool beginSuccess = (url.startsWith("https")) ? http.begin(secureClient, url) : http.begin(client, url);

    if (!beginSuccess) {
        errorMsg = "Connection Setup Failed";
        return entries;
    }
    
    int httpCode = http.GET();
    if (httpCode == 301 || httpCode == 302) {
        String newUrl = http.header("Location");
        http.end();
        return fetchCatalog(newUrl); 
    }

    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        XMLDocument doc;
        if (doc.Parse(payload.c_str()) == XML_SUCCESS) {
            XMLElement* root = doc.RootElement(); 
            if (root) {
                XMLElement* entry = root->FirstChildElement();
                while (entry) {
                    const char* name = entry->Name();
                    if (name && strstr(name, "entry")) {
                        OpdsEntry e;
                        XMLElement* child = entry->FirstChildElement();
                        String fallbackLink = "";
                        
                        while (child) {
                            const char* cName = child->Name();
                            if (strstr(cName, "title")) e.title = child->GetText();
                            else if (strstr(cName, "author")) {
                                XMLElement* n = findTag(child, "name");
                                if (n) e.author = n->GetText();
                            }
                            else if (strstr(cName, "link")) {
                                const char* rel = child->Attribute("rel");
                                const char* type = child->Attribute("type");
                                const char* href = child->Attribute("href");
                                if (rel && href) {
                                    // Match direct txt
                                    if ((strstr(rel, "acquisition") || strstr(rel, "open-access")) && 
                                        (type && strstr(type, "text/plain"))) {
                                        e.downloadUrl = href;
                                    }
                                    // Gutenberg fallback:subsection link often leads to actual book page
                                    if (strstr(rel, "subsection") && type && strstr(type, "atom+xml")) {
                                        fallbackLink = href;
                                    }
                                }
                            }
                            child = child->NextSiblingElement();
                        }
                        
                        // If no direct download, use the entry link as a resolver source
                        if (e.downloadUrl == "" && fallbackLink != "") e.downloadUrl = "resolve:" + fallbackLink;

                        if (e.title != "" && e.downloadUrl != "") {
                            if (e.downloadUrl.startsWith("/")) {
                                int slash3 = url.indexOf('/', 8);
                                String base = (slash3 > 0) ? url.substring(0, slash3) : url;
                                e.downloadUrl = (e.downloadUrl.startsWith("resolve:/") ? "resolve:" + base + e.downloadUrl.substring(8) : base + e.downloadUrl);
                            }
                            entries.push_back(e);
                        }
                    }
                    entry = entry->NextSiblingElement();
                }
            }
        } else {
            errorMsg = "XML Parse Error";
        }
    } else {
        errorMsg = "HTTP " + String(httpCode);
    }
    http.end();
    if (entries.empty() && errorMsg == "") errorMsg = "No entries found";
    return entries;
}

String OpdsClient::resolveBookUrl(String bookOpdsUrl) {
    if (WiFi.status() != WL_CONNECTED) return "";
    
    WiFiClientSecure secureClient; secureClient.setInsecure();
    WiFiClient client;
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setUserAgent("Mozilla/5.0");
    
    if (bookOpdsUrl.startsWith("https")) http.begin(secureClient, bookOpdsUrl);
    else http.begin(client, bookOpdsUrl);
    
    int code = http.GET();
    if (code == 200) {
        String payload = http.getString();
        XMLDocument doc;
        if (doc.Parse(payload.c_str()) == XML_SUCCESS) {
            XMLElement* root = doc.RootElement();
            // Look for first acquisition link ending in .txt or text/plain
            XMLElement* entry = root->FirstChildElement();
            while (entry) {
                if (strstr(entry->Name(), "entry")) {
                    XMLElement* link = entry->FirstChildElement();
                    while (link) {
                        if (strstr(link->Name(), "link")) {
                            const char* href = link->Attribute("href");
                            const char* type = link->Attribute("type");
                            if (href && (strstr(href, ".txt.utf-8") || (type && strstr(type, "text/plain")))) {
                                String result = href;
                                http.end(); return result;
                            }
                        }
                        link = link->NextSiblingElement();
                    }
                }
                entry = entry->NextSiblingElement();
            }
        }
    }
    http.end();
    return "";
}

bool OpdsClient::downloadBook(String url, String targetPath) {
    if (WiFi.status() != WL_CONNECTED) return false;
    WiFiClientSecure secureClient; secureClient.setInsecure();
    WiFiClient client;
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    bool beginSuccess = (url.startsWith("https")) ? http.begin(secureClient, url) : http.begin(client, url);
    if (!beginSuccess) return false;
    
    int httpCode = http.GET();
    bool success = false;
    if (httpCode == HTTP_CODE_OK) {
        File f = SD.open(targetPath, FILE_WRITE);
        if (f) {
            http.writeToStream(&f);
            f.close();
            success = true;
        }
    }
    http.end();
    return success;
}
