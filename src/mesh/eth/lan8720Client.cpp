#include "mesh/eth/lan8720Client.h"

#if HAS_ETHERNET && defined(USE_LAN8720)

#include "NodeDB.h"
#include "RTC.h"
#include "concurrency/Periodic.h"
#include "configuration.h"
#include "main.h"
#if HAS_WIFI
#include "mesh/api/WiFiServerAPI.h" // initApiServer() provided by WiFi stack
#else
#include "mesh/api/lan8720ServerAPI.h"
#endif
#include "target_specific.h"
#include <ESPmDNS.h>
#include <ETH.h>
#include <WiFi.h>
#include <WiFiUdp.h>

#if !MESHTASTIC_EXCLUDE_WEBSERVER
#include "mesh/http/WebServer.h"
#endif

#ifndef DISABLE_NTP
#include <NTPClient.h>
static WiFiUDP ntpUDP;
static NTPClient timeClient(ntpUDP, config.network.ntp_server);
static uint32_t ntp_renew = 0;
#endif

static WiFiUDP syslogClient;
static meshtastic::Syslog syslog(syslogClient);

static bool ethStartupComplete = false;

using namespace concurrency;
static Periodic *ethEvent;

// Called once after IP is obtained; starts network services
static void onEthernetConnected()
{
    if (ethStartupComplete)
        return;

    LOG_INFO("Start LAN8720 Ethernet network services");

    if (!MDNS.begin("Meshtastic")) {
        LOG_ERROR("Error setting up mDNS responder!");
    } else {
        LOG_INFO("mDNS Host: Meshtastic.local");
        MDNS.addService("meshtastic", "tcp", SERVER_API_DEFAULT_PORT);
        MDNS.addServiceTxt("meshtastic", "tcp", "shortname", String(owner.short_name));
        MDNS.addServiceTxt("meshtastic", "tcp", "id", String(nodeDB->getNodeId().c_str()));
        MDNS.addServiceTxt("meshtastic", "tcp", "pio_env", optstr(APP_ENV));
    }

#ifndef DISABLE_NTP
    LOG_INFO("Start NTP time client");
    timeClient.begin();
    timeClient.setUpdateInterval(60 * 60); // Update once an hour
#endif

    if (config.network.rsyslog_server[0]) {
        LOG_INFO("Start Syslog client");
        int serverPort = 514;
        const char *serverAddr = config.network.rsyslog_server;
        String server = String(serverAddr);
        int delimIndex = server.indexOf(':');
        if (delimIndex > 0) {
            String port = server.substring(delimIndex + 1, server.length());
            server[delimIndex] = 0;
            serverPort = port.toInt();
            serverAddr = server.c_str();
        }
        syslog.server(serverAddr, serverPort);
        syslog.deviceHostname(getDeviceName());
        syslog.appName("Meshtastic");
        syslog.defaultPriority(LOGLEVEL_USER);
        syslog.enable();
    }

#if !MESHTASTIC_EXCLUDE_WEBSERVER
    if (config.display.displaymode != meshtastic_Config_DisplayConfig_DisplayMode_COLOR) {
        initWebServer();
    }
#endif

#if !MESHTASTIC_EXCLUDE_SOCKETAPI
    if (config.display.displaymode != meshtastic_Config_DisplayConfig_DisplayMode_COLOR) {
        initApiServer();
    }
#endif

#if HAS_UDP_MULTICAST
    if (udpHandler && config.network.enabled_protocols & meshtastic_Config_NetworkConfig_ProtocolFlags_UDP_BROADCAST) {
        udpHandler->start();
    }
#endif

    ethStartupComplete = true;
}

static void ETHEvent(WiFiEvent_t event)
{
    switch (event) {
    case ARDUINO_EVENT_ETH_START:
        LOG_INFO("LAN8720 Ethernet started");
        ETH.setHostname("Meshtastic");
        break;
    case ARDUINO_EVENT_ETH_CONNECTED:
        LOG_INFO("LAN8720 Ethernet connected");
        break;
    case ARDUINO_EVENT_ETH_GOT_IP:
        LOG_INFO("Obtained IP address: %s, %u Mbps, %s", ETH.localIP().toString().c_str(), ETH.linkSpeed(),
                 ETH.fullDuplex() ? "FULL_DUPLEX" : "HALF_DUPLEX");
        onEthernetConnected();
        break;
    case ARDUINO_EVENT_ETH_DISCONNECTED:
        syslog.disable();
        LOG_INFO("LAN8720 Ethernet disconnected");
        break;
    case ARDUINO_EVENT_ETH_STOP:
        syslog.disable();
        LOG_INFO("LAN8720 Ethernet stopped");
        break;
    default:
        break;
    }
}

static int32_t runEthPeriodic()
{
#ifndef DISABLE_NTP
    if (isEthernetAvailable() && (ntp_renew < millis())) {
        LOG_INFO("Update NTP time from %s", config.network.ntp_server);
        if (timeClient.update()) {
            LOG_DEBUG("NTP Request Success - Set RTCQualityNTP if needed");
            struct timeval tv;
            tv.tv_sec = timeClient.getEpochTime();
            tv.tv_usec = 0;
            perhapsSetRTC(RTCQualityNTP, &tv);
            ntp_renew = millis() + 43200 * 1000; // refresh every 12 hours
        } else {
            LOG_ERROR("NTP Update failed");
            ntp_renew = millis() + 300 * 1000; // retry every 5 minutes
        }
    }
#endif
    return 5000;
}

bool initEthernet()
{
    config.network.eth_enabled = true;
    config.network.wifi_enabled = false;
    config.power.is_power_saving = false;
    // Fixed node — enforce 5-minute floor on smart broadcast so MQTT is not flooded
    if (config.position.broadcast_smart_minimum_interval_secs < 300)
        config.position.broadcast_smart_minimum_interval_secs = 300;

    WiFi.onEvent(ETHEvent);
    WiFi.mode(WIFI_OFF); // free WiFi radio buffers; lwIP stays up for Ethernet

    // ETH.begin(phy_addr, power, mdc, mdio, type, clk_mode)
    // All values come from pins_arduino.h for esp32-poe-iso, or can be
    // overridden in variant.h via ETH_PHY_ADDR / ETH_PHY_POWER / etc.
    if (!ETH.begin(ETH_PHY_ADDR, ETH_PHY_POWER, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_LAN8720, ETH_CLOCK_GPIO17_OUT)) {
        LOG_ERROR("LAN8720 ETH.begin() failed");
        return false;
    }

    if (config.network.address_mode == meshtastic_Config_NetworkConfig_AddressMode_STATIC) {
        LOG_INFO("Using static IP");
        ETH.config(config.network.ipv4_config.ip, config.network.ipv4_config.gateway, config.network.ipv4_config.subnet,
                   config.network.ipv4_config.dns);
    }
    // DHCP is the default when config() is not called

    ethEvent = new Periodic("ethLAN8720", runEthPeriodic);
    return true;
}

bool isEthernetAvailable()
{
    return ETH.linkUp() && ETH.localIP() != IPAddress(0, 0, 0, 0);
}

#endif // HAS_ETHERNET && USE_LAN8720
