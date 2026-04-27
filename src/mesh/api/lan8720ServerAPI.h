#pragma once
#ifdef USE_LAN8720

#include "ServerAPI.h"
#include <WiFiServer.h>

// TCP API server for LAN8720 Ethernet builds.
// Uses WiFiClient/WiFiServer because the ESP32 Arduino framework's lwIP socket
// layer is shared between WiFi and RMII Ethernet — WiFiClient works over both.

class lan8720ServerAPI : public ServerAPI<WiFiClient>
{
  public:
    explicit lan8720ServerAPI(WiFiClient &_client);
};

class lan8720ServerPort : public APIServerPort<lan8720ServerAPI, WiFiServer>
{
  public:
    explicit lan8720ServerPort(int port);
};

void initApiServer(int port = SERVER_API_DEFAULT_PORT);

#endif // USE_LAN8720
