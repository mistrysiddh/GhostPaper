#ifndef STORE_H
#define STORE_H

#include "common.h"
#include "opds_client.h"

extern std::vector<OpdsEntry> storeCatalog;
void updateStore();
void handleStoreTouch(int x, int y);
void syncStore(); // Fetches the catalog

#endif
