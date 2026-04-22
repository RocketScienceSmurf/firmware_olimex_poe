#include "configuration.h"
#include <Arduino.h>

#if HAS_ETHERNET && defined(USE_LAN8720) && !HAS_WIFI

#include "lan8720ServerAPI.h"

static lan8720ServerPort *apiPort;

void initApiServer(int port)
{
    if (!apiPort) {
        apiPort = new lan8720ServerPort(port);
        LOG_INFO("API server listening on TCP port %d", port);
        apiPort->init();
    }
}

lan8720ServerAPI::lan8720ServerAPI(WiFiClient &_client) : ServerAPI(_client)
{
    LOG_INFO("Incoming LAN8720 Ethernet connection");
    api_type = TYPE_ETH;
}

lan8720ServerPort::lan8720ServerPort(int port) : APIServerPort(port) {}

#endif // HAS_ETHERNET && USE_LAN8720 && !HAS_WIFI
