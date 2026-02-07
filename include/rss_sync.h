#ifndef RSS_SYNC_H
#define RSS_SYNC_H

#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <SD.h>
#include "config.h"
#include "common.h"

// Returns true if successful
bool downloadDailyNews();

#endif
