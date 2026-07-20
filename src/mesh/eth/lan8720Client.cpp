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
#include <WiFi.h>
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

// Set by onEthEvent() on the network event task, consumed by startEthServices() on the
// main thread. volatile because it is written and read from two different FreeRTOS tasks.
static volatile bool ethHasIP = false;
// Owned exclusively by the main thread (startEthServices).
static bool ethStartupComplete = false;

static void onNTPSync(struct timeval *tv)
{
    perhapsSetRTC(RTCQualityNTP, tv);
    LOG_DEBUG("NTP synced: epoch=%lu", (unsigned long)tv->tv_sec);
}

// ETH event callback — fires on the ESP32 "arduino_events" network task, NOT the main
// loop() task. It MUST stay minimal: it may not create OSThreads or otherwise touch the
// mainController scheduler list, because that list is walked continuously by loop() with
// no locking. Doing so previously registered the API server's OSThread from this task,
// racing the scheduler so its runOnce()/accept() never got serviced — the TCP port was
// open but no network client could ever connect. Here we only flip a flag; the actual
// service startup runs on the main thread in startEthServices().
//
// ARDUINO_EVENT_ETH_GOT_IP is the authoritative signal that DHCP has completed and a valid
// IP is assigned; earlier events (ETH_CONNECTED) still have IP 0.0.0.0.
static void onEthEvent(WiFiEvent_t event)
{
    switch (event) {
    case ARDUINO_EVENT_ETH_GOT_IP:
        ethHasIP = true;
        break;

    case ARDUINO_EVENT_ETH_DISCONNECTED:
        LOG_INFO("LAN8720 link down");
        ethHasIP = false;
        break;

    default:
        break;
    }
}

// Runs on the main thread (meshtastic scheduler), so it is safe to allocate OSThreads and
// start network services here. Polls the flag set by onEthEvent() and performs one-time
// startup once DHCP has assigned an IP; resets when the link drops so services are
// re-initialised on reconnect.
static concurrency::Periodic *ethServices;

static int32_t startEthServices()
{
    if (ethHasIP && !ethStartupComplete) {
        LOG_INFO("LAN8720 link up, IP=%s, %dMbps %s-duplex",
                 ETH.localIP().toString().c_str(),
                 ETH.linkSpeed(),
                 ETH.fullDuplex() ? "full" : "half");

        // Use lwIP built-in SNTP — no extra library, zero heap overhead vs NTPClient
        sntp_set_time_sync_notification_cb(onNTPSync);
        configTime(0, 0, config.network.ntp_server[0] ? config.network.ntp_server : "pool.ntp.org");

        if (MDNS.begin(getDeviceName()))
            LOG_INFO("mDNS started: %s.local", getDeviceName());

#if !MESHTASTIC_EXCLUDE_SOCKETAPI
        initApiServer();
#endif

        ethStartupComplete = true;
    } else if (!ethHasIP && ethStartupComplete) {
        // Link dropped after we started services; allow re-init on the next GOT_IP.
        ethStartupComplete = false;
    }

    // Poll the flag frequently so startup is near-immediate after GOT_IP; the cost of an
    // idle boolean check every 250 ms is negligible.
    return 250;
}

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

    // Register event handler before ETH.begin() so no events are missed.
    // ETH.begin() configures ESP32 EMAC + LAN8720 PHY via RMII; link negotiation
    // and DHCP are asynchronous and will be signalled via onEthEvent() setting ethHasIP.
    WiFi.onEvent(onEthEvent);
    ETH.begin(LAN8720_PHY_ADDR, LAN8720_PHY_POWER, LAN8720_PHY_MDC, LAN8720_PHY_MDIO,
              ETH_PHY_LAN8720, LAN8720_CLK_MODE);

    // Start the main-thread service starter. OSThread creation (the API server) must happen
    // on the main thread, so the event callback only signals and this Periodic does the work.
    ethServices = new concurrency::Periodic("ethServices", startEthServices);

    return true;
}

bool isEthernetAvailable()
{
    return config.network.eth_enabled && ETH.linkUp() && (uint32_t)ETH.localIP() != 0;
}

#endif // USE_LAN8720
