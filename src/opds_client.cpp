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

bool isOpdsFeed(const char* type) {
    if (!type) return false;
    return strstr(type, "application/atom+xml");
}

std::vector<OpdsEntry> OpdsClient::fetchCatalog(String url) {
    std::vector<OpdsEntry> allEntries;
    String nextUrl = url;
    int pagesFetched = 0;
    const int MAX_PAGES = 20; // Fetch up to ~500 books
    const int MIN_FREE_PSRAM = 100000; // Stop if less than 100KB PSRAM remaining
    
    errorMsg = "";

    while (nextUrl != "" && pagesFetched < MAX_PAGES) {
        // Safety Check: Memory
        if (ESP.getPsramSize() > 0 && ESP.getFreePsram() < MIN_FREE_PSRAM) {
            Serial.println("OPDS: Memory limit reached, stopping fetch.");
            break;
        }

        if (WiFi.status() != WL_CONNECTED) {
            errorMsg = "WiFi Lost";
            break;
        }

        Serial.printf("OPDS: Fetching Page %d: %s\n", pagesFetched + 1, nextUrl.c_str());

        HTTPClient http;
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
        http.setTimeout(20000);
        http.setUserAgent("Mozilla/5.0");
        
        const char * headerKeys[] = {"Location"};
        http.collectHeaders(headerKeys, 1);

        WiFiClient* client = NULL;
        if (nextUrl.startsWith("https")) {
            WiFiClientSecure *sClient = new WiFiClientSecure();
            sClient->setInsecure();
            client = sClient;
            http.begin(*sClient, nextUrl);
        } else {
            client = new WiFiClient();
            http.begin(*client, nextUrl);
        }

        int httpCode = http.GET();
        if (httpCode == 301 || httpCode == 302) {
            nextUrl = http.header("Location");
            http.end();
            if (client) delete client;
            continue; // Re-run loop with new URL
        }

        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            XMLDocument doc;
            if (doc.Parse(payload.c_str()) == XML_SUCCESS) {
                XMLElement* root = doc.RootElement(); 
                if (root) {
                    // Update nextPageUrl for the UI (this will be the URL for page 4, 7, 10...)
                    nextPageUrl = ""; 
                    XMLElement* link = root->FirstChildElement();
                    while (link) {
                        const char* lName = link->Name();
                        if (lName && strstr(lName, "link")) {
                            const char* rel = link->Attribute("rel");
                            const char* href = link->Attribute("href");
                            if (rel && href && strstr(rel, "next")) {
                                nextPageUrl = String(href);
                                if (nextPageUrl.startsWith("/")) {
                                    int slash3 = nextUrl.indexOf('/', 8);
                                    String base = (slash3 > 0) ? nextUrl.substring(0, slash3) : nextUrl;
                                    nextPageUrl = base + nextPageUrl;
                                }
                            }
                        }
                        link = link->NextSiblingElement();
                    }

                    // Parse Entries
                    XMLElement* entry = root->FirstChildElement();
                    while (entry) {
                        const char* name = entry->Name();
                        if (name && strstr(name, "entry")) {
                            OpdsEntry e;
                            XMLElement* child = entry->FirstChildElement();
                            String fallbackLink = "";
                            
                            while (child) {
                                const char* cName = child->Name();
                                if (strstr(cName, "title")) e.title = (child->GetText() ? child->GetText() : "");
                                else if (strstr(cName, "author")) {
                                    XMLElement* n = findTag(child, "name");
                                    if (n && n->GetText()) e.author = n->GetText();
                                }
                                else if (strstr(cName, "link")) {
                                    const char* rel = child->Attribute("rel");
                                    const char* type = child->Attribute("type");
                                    const char* href = child->Attribute("href");
                                    if (rel && href) {
                                        if ((strstr(rel, "acquisition") || strstr(rel, "open-access")) && 
                                            (type && strstr(type, "text/plain"))) {
                                            e.downloadUrl = href;
                                        }
                                        if (isOpdsFeed(type)) {
                                            if (fallbackLink == "") fallbackLink = href;
                                        }
                                    }
                                }
                                child = child->NextSiblingElement();
                            }
                            
                            if (e.downloadUrl == "" && fallbackLink != "") e.downloadUrl = "resolve:" + fallbackLink;

                            if (e.title != "" && e.downloadUrl != "") {
                                if (e.downloadUrl.startsWith("/")) {
                                    int slash3 = nextUrl.indexOf('/', 8);
                                    String base = (slash3 > 0) ? nextUrl.substring(0, slash3) : nextUrl;
                                    if (e.downloadUrl.startsWith("resolve:/")) e.downloadUrl = "resolve:" + base + e.downloadUrl.substring(8);
                                    else e.downloadUrl = base + e.downloadUrl;
                                }
                                allEntries.push_back(e);
                            }
                        }
                        entry = entry->NextSiblingElement();
                    }
                }
            }
            pagesFetched++;
            nextUrl = nextPageUrl; // Move to the next page for the next iteration
        } else {
            errorMsg = "HTTP " + String(httpCode);
            nextUrl = ""; // Stop
        }
        
        http.end();
        if (client) delete client;
    }
    
    return allEntries;
}

String OpdsClient::resolveBookUrl(String startUrl) {
    String currentUrl = startUrl;
    int depth = 0;

    while (depth < 4) {
        if (WiFi.status() != WL_CONNECTED) return "";
        if (currentUrl.startsWith("/")) currentUrl = "https://www.gutenberg.org" + currentUrl;
        
        if (currentUrl.indexOf("gutenberg.org/ebooks/") != -1 && currentUrl.endsWith(".opds")) {
            String guess = currentUrl;
            guess.replace(".opds", ".txt.utf-8");
            Serial.printf("OPDS: Gutenberg Heuristic Triggered -> %s\n", guess.c_str());
            return guess;
        }

        Serial.printf("OPDS: Resolving %s (D%d)\n", currentUrl.c_str(), depth);
        
        String foundUrl = "";
        String nextFeed = "";

        {
            HTTPClient http;
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            http.setTimeout(15000);
            http.setUserAgent("Mozilla/5.0");
            
            WiFiClient* client = NULL;
            if (currentUrl.startsWith("https")) {
                WiFiClientSecure *sClient = new WiFiClientSecure();
                sClient->setInsecure();
                client = sClient;
                http.begin(*sClient, currentUrl);
            } else {
                client = new WiFiClient();
                http.begin(*client, currentUrl);
            }
            
            int code = http.GET();
            if (code == 200) {
                if (ESP.getFreeHeap() > 40000) {
                    String payload = http.getString();
                    XMLDocument doc;
                    if (doc.Parse(payload.c_str()) == XML_SUCCESS) {
                        XMLElement* root = doc.RootElement();
                        auto checkLink = [&](XMLElement* link) {
                            const char* href = link->Attribute("href");
                            const char* type = link->Attribute("type");
                            const char* rel = link->Attribute("rel");
                            if (href) {
                                bool isText = (type && strstr(type, "text/plain")) || strstr(href, ".txt");
                                if (isText) {
                                    if (foundUrl == "" || strstr(href, "utf-8")) {
                                        foundUrl = String(href);
                                        if (foundUrl.startsWith("/")) foundUrl = "https://www.gutenberg.org" + foundUrl;
                                    }
                                }
                                if (foundUrl == "" && isOpdsFeed(type) && rel) {
                                    bool isNav = strstr(rel, "subsection") || strstr(rel, "acquisition") || strstr(rel, "alternate");
                                    bool isRelated = strstr(rel, "related") || strstr(rel, "author") || strstr(rel, "bookshelf");
                                    if (isNav && !isRelated) {
                                        String h = String(href);
                                        if (h != currentUrl && h.indexOf("bookshelf") == -1) {
                                            nextFeed = h;
                                            if (nextFeed.startsWith("/")) nextFeed = "https://www.gutenberg.org" + nextFeed;
                                        }
                                    }
                                }
                            }
                        };

                        XMLElement* rootChild = root ? root->FirstChildElement() : NULL;
                        while(rootChild) {
                            if (rootChild->Name() && strstr(rootChild->Name(), "link")) checkLink(rootChild);
                            rootChild = rootChild->NextSiblingElement();
                        }
                        XMLElement* entry = root ? root->FirstChildElement() : NULL;
                        while (entry) {
                            if (entry->Name() && strstr(entry->Name(), "entry")) {
                                XMLElement* child = entry->FirstChildElement();
                                while (child) {
                                    if (child->Name() && strstr(child->Name(), "link")) checkLink(child);
                                    child = child->NextSiblingElement();
                                }
                            }
                            entry = entry->NextSiblingElement();
                        }
                    }
                }
            }
            http.end();
            if (client) delete client;
        }

        if (foundUrl != "") return foundUrl;
        if (nextFeed != "" && nextFeed != currentUrl) {
            currentUrl = nextFeed;
            depth++;
        } else break;
    }
    return "";
}

bool OpdsClient::downloadBook(String startUrl, String targetPath, std::function<void(int)> progressCallback) {
    String currentUrl = startUrl;
    int redirects = 0;

    while (redirects < 6) {
        if (WiFi.status() != WL_CONNECTED) return false;
        if (currentUrl.startsWith("/")) currentUrl = "https://www.gutenberg.org" + currentUrl;

        Serial.printf("OPDS: Downloading %s (R%d)\n", currentUrl.c_str(), redirects);
        
        delay(100);

        HTTPClient http;
        // Stack-allocated clients for safety
        WiFiClientSecure secureClient;
        WiFiClient client;
        secureClient.setInsecure();

        http.setFollowRedirects(HTTPC_DISABLE_FOLLOW_REDIRECTS);
        http.setTimeout(30000);
        http.setUserAgent("Mozilla/5.0");
        const char * headerKeys[] = {"Location"};
        http.collectHeaders(headerKeys, 1);

        if (currentUrl.startsWith("https")) {
            http.begin(secureClient, currentUrl);
        } else {
            http.begin(client, currentUrl);
        }

        int httpCode = http.GET();
        Serial.printf("OPDS: HTTP %d\n", httpCode);

        if (httpCode == 301 || httpCode == 302) {
            String nextUrl = http.header("Location");
            http.end(); 
            
            if (nextUrl == "" || nextUrl == currentUrl) return false;
            currentUrl = nextUrl;
            redirects++;
            continue;
        }

        bool success = false;
        if (httpCode == 200) {
            File f = SD.open(targetPath, FILE_WRITE);
            if (f) {
                uint8_t buff[1024] = { 0 };
                int len = http.getSize();
                int totalRead = 0;
                int lastProgress = -1;
                
                WiFiClient *stream = http.getStreamPtr();
                
                while(http.connected() && (len > 0 || len == -1)) {
                    size_t size = stream->available();
                    if(size) {
                        int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
                        f.write(buff, c);
                        if(len > 0) len -= c;
                        totalRead += c;
                        
                        if (progressCallback && len > -1) {
                            // Calculate total expected based on original len + what we read if len decrements
                            // Actually len decrements in this loop logic.
                            // Better: track total size separately
                            int originalSize = http.getSize();
                            if (originalSize > 0) {
                                int pct = (totalRead * 100) / originalSize;
                                if (pct != lastProgress && pct % 5 == 0) { // Update every 5%
                                    progressCallback(pct);
                                    lastProgress = pct;
                                }
                            }
                        }
                    }
                    delay(1);
                }
                
                f.close();
                Serial.printf("OPDS: Written %d bytes\n", totalRead);
                success = (totalRead > 0);
            }
        }
        
        http.end();
        return success;
    }
    return false;
}