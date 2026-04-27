#if defined(USE_LAN8720)

#include "mesh/eth/ethClient.h"
#include "DebugConfiguration.h"
#include "NodeDB.h"
#include "RTC.h"
#include "concurrency/Periodic.h"
#include "configuration.h"
#include "main.h"
#include "mesh/api/lan8720ServerAPI.h"
#include "target_specific.h"
#include <ETH.h>
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <esp_sntp.h>

static WiFiUDP syslogClient;
meshtastic::Syslog syslog(syslogClient);

// Olimex ESP32-POE-ISO LAN8720A RMII pinout
#define LAN8720_PHY_ADDR      0
#define LAN8720_PHY_POWER    12   // GPIO12 resets LAN8720 nRST after clock (GPIO17) is running
#define LAN8720_PHY_MDC      23
#define LAN8720_PHY_MDIO     18
#define LAN8720_CLK_MODE     ETH_CLOCK_GPIO17_OUT  // ESP32 drives 50 MHz REF_CLK to LAN8720 XTAL1/CLKIN

static bool ethStartupComplete = false;

static void onNTPSync(struct timeval *tv)
{
    perhapsSetRTC(RTCQualityNTP, tv);
    LOG_DEBUG("NTP synced: epoch=%lu", (unsigned long)tv->tv_sec);
}

static int32_t reconnectETH()
{
    if (!ethStartupComplete && ETH.linkUp() && (uint32_t)ETH.localIP() != 0) {
        LOG_INFO("LAN8720 link up, IP=%s", ETH.localIP().toString().c_str());

        // Use lwIP built-in SNTP — no extra library, zero heap overhead vs NTPClient
        sntp_set_time_sync_notification_cb(onNTPSync);
        configTime(0, 0, config.network.ntp_server[0] ? config.network.ntp_server : "pool.ntp.org");

        if (MDNS.begin(getDeviceName()))
            LOG_INFO("mDNS started: %s.local", getDeviceName());

#if !MESHTASTIC_EXCLUDE_SOCKETAPI
        initApiServer();
#endif

        ethStartupComplete = true;
    }
    return 5000;
}

static concurrency::Periodic *ethEvent;

bool initEthernet()
{
    // Force always-on config for this wired node regardless of saved prefs
    config.network.eth_enabled = true;
    config.network.wifi_enabled = false;
    config.power.is_power_saving = false;
    config.power.ls_secs = 0;
    // Floor smart-broadcast at 5 min so MQTT map reports are not flooded
    if (config.position.broadcast_smart_minimum_interval_secs < 300)
        config.position.broadcast_smart_minimum_interval_secs = 300;

    LOG_INFO("Init LAN8720 RMII Ethernet (PHY addr=%d MDC=%d MDIO=%d CLK=GPIO17_OUT)",
             LAN8720_PHY_ADDR, LAN8720_PHY_MDC, LAN8720_PHY_MDIO);

    // ETH.begin() configures ESP32 EMAC + LAN8720 PHY via RMII.
    // Both link negotiation and DHCP are asynchronous; reconnectETH() polls every 5 s and
    // waits until linkUp() is true AND localIP() is non-zero (DHCP complete) before starting
    // NTP, mDNS, and the API server, so those services are never started with IP 0.0.0.0.
    ETH.begin(LAN8720_PHY_ADDR, LAN8720_PHY_POWER, LAN8720_PHY_MDC, LAN8720_PHY_MDIO,
              ETH_PHY_LAN8720, LAN8720_CLK_MODE);

    ethEvent = new concurrency::Periodic("ethConnect", reconnectETH);
    return true;
}

bool isEthernetAvailable()
{
    return config.network.eth_enabled && ETH.linkUp() && (uint32_t)ETH.localIP() != 0;
}

#endif // USE_LAN8720
