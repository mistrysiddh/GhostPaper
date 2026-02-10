#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "common.h"

void updateDashboard();
void handleDashboardTouch(int x, int y);
void drawReadingWidget(int y);
void fetchDashboardData(bool force = false);

extern bool dashboardDataReady;
extern bool isFetchingData;

#endif
