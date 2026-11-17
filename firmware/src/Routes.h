#pragma once
// HTTP route registration, split by area. setupRoutes() registers all of them in order.
#include "AppState.h"

namespace spoolancer {
void setupRoutes();
void registerSetupRoutes();
void registerSpoolRoutes();
void registerHardwareRoutes();
void registerConfigRoutes();
}  // namespace spoolancer
