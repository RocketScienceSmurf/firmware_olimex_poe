#pragma once

#include "configuration.h"
#include <Arduino.h>

#if HAS_ETHERNET && defined(USE_LAN8720)

bool initEthernet();
bool isEthernetAvailable();

#endif
