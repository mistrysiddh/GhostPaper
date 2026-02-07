#include "rss_sync.h"

// Helper to strip HTML tags
String stripHTML(String html) {
    String text = "";
    bool insideTag = false;
    for (unsigned int i = 0; i < html.length(); i++) {
        char c = html[i];
        if (c == '<') {
            insideTag = true;
            continue;
        }
        if (c == '>') {
            insideTag = false;
            // Add a newline after paragraph or break tags for readability
            if (i > 3 && html.substring(i-3, i) == "br") text += "\n";
            if (i > 3 && html.substring(i-3, i) == "/p") text += "\n\n";
            continue;
        }
        if (!insideTag) {
            text += c;
        }
    }
    // Basic entity decoding
    text.replace("&amp;", "&");
    text.replace("&lt;", "<");
    text.replace("&gt;", ">");
    text.replace("&quot;", "\"");
    text.replace("&#39;", "'");
    text.replace("&nbsp;", " ");
    return text;
}

bool downloadDailyNews() {
    if (DEBUG_ON) Serial.println(F("[RSS] Starting Sync..."));

    // 1. Connect to WiFi
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(WIFI_SSID_1, WIFI_PASS_1);
        int retries = 0;
        while (WiFi.status() != WL_CONNECTED && retries < 20) {
            delay(500);
            if (DEBUG_ON) Serial.print(".");
            retries++;
        }
        if (WiFi.status() != WL_CONNECTED) {
            if (DEBUG_ON) Serial.println(F("\n[RSS] WiFi Connection Failed"));
            return false;
        }
    }
    if (DEBUG_ON) Serial.println(F("\n[RSS] WiFi Connected"));

    // 2. Fetch Feed
    HTTPClient http;
    http.begin(RSS_FEED_URL);
    int httpCode = http.GET();

    if (httpCode == HTTP_CODE_OK) {
        if (DEBUG_ON) Serial.println(F("[RSS] Feed Downloaded"));
        
        File file = SD.open("/Daily_News.txt", FILE_WRITE);
        if (!file) {
            if (DEBUG_ON) Serial.println(F("[RSS] SD Write Failed"));
            http.end();
            return false;
        }

        String payload = http.getString();
        int articleCount = 0;
        int itemStart = 0;

        file.println("DAILY NEWS BRIEFING");
        file.println("===================\n");

        while (articleCount < MAX_ARTICLES) {
            itemStart = payload.indexOf("<item>", itemStart);
            if (itemStart == -1) break;

            int itemEnd = payload.indexOf("</item>", itemStart);
            if (itemEnd == -1) break;

            String itemXml = payload.substring(itemStart, itemEnd);

            // Extract Title
            int titleStart = itemXml.indexOf("<title>");
            int titleEnd = itemXml.indexOf("</title>");
            String title = "";
            if (titleStart != -1 && titleEnd != -1) {
                title = itemXml.substring(titleStart + 7, titleEnd);
                title = stripHTML(title); 
            }

            // Extract Description
            int descStart = itemXml.indexOf("<description>");
            int descEnd = itemXml.indexOf("</description>");
            String desc = "";
            if (descStart != -1 && descEnd != -1) {
                desc = itemXml.substring(descStart + 13, descEnd);
                desc = stripHTML(desc);
            }

            // Write to file
            if (title.length() > 0) {
                file.println(title);
                file.println("-------------------");
                if (desc.length() > 0) {
                    file.println(desc);
                }
                file.println("\n");
                articleCount++;
            }

            itemStart = itemEnd;
        }

        file.close();
        http.end();
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        if (DEBUG_ON) Serial.printf("[RSS] Saved %d articles\n", articleCount);
        return true;

    } else {
        if (DEBUG_ON) Serial.printf("[RSS] HTTP Failed: %d\n", httpCode);
        http.end();
        return false;
    }
}