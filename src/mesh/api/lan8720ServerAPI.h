#pragma once

#include "ServerAPI.h"

// lan8720ServerAPI is only needed when the WiFi stack is absent.
// On ESP32, HAS_WIFI is always 1, so WiFiServerAPI already provides
// initApiServer() and the WiFiClient/WiFiServer template instances.
// This file is compiled only on platforms where HAS_WIFI is 0.
#if HAS_ETHERNET && defined(USE_LAN8720) && !HAS_WIFI
#include <WiFi.h> // WiFiClient / WiFiServer live in WiFi.h on Arduino-ESP32 v2.x

/**
 * Provides both debug printing and, if the client starts sending protobufs to us, switches to send/receive protobufs.
 */
class lan8720ServerAPI : public ServerAPI<WiFiClient>
{
  public:
    explicit lan8720ServerAPI(WiFiClient &_client);
};

/**
 * Listens for incoming connections and creates instances of lan8720ServerAPI as needed.
 */
class lan8720ServerPort : public APIServerPort<lan8720ServerAPI, WiFiServer>
{
  public:
    explicit lan8720ServerPort(int port);
};

void initApiServer(int port = SERVER_API_DEFAULT_PORT);

#endif // HAS_ETHERNET && USE_LAN8720 && !HAS_WIFI
