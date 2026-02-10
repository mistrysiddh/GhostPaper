#ifndef DASHBOARD_H
#define DASHBOARD_H

#include "common.h"

void updateDashboard();
void handleDashboardTouch(int x, int y);
void drawWeatherWidget(int x, int y, int w, int h);
void drawCalendarWidget(int x, int y, int w, int h);
void drawGoalWidget(int x, int y, int w, int h);
void fetchDashboardData();

#endif
