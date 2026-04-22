// Olimex ESP32-POE-ISO + Olimex MOD-LoRa868/915 (SX1276)
//
// Plug the MOD-LoRa868 (or MOD-LoRa915) directly into the ESP32-POE-ISO UEXT connector.
// The MOD-LoRa UEXT connector remaps the standard UEXT UART/I2C pins to SX1276 DIO/RESET:
//
//   UEXT  1 (3.3V)     -> MOD-LoRa VCC
//   UEXT  2 (GND)      -> MOD-LoRa GND
//   UEXT  3 (GPIO4)    -> SX1276 DIO0  (RxDone/TxDone/CadDone interrupt)
//   UEXT  4 (GPIO36)   -> SX1276 DIO1  (RxTimeout; GPIO36 is input-only, OK for IRQ)
//   UEXT  5 (GPIO16)   -> SX1276 RESET (active low, min 100us pulse)
//   UEXT  6 (GPIO13)   -> SX1276 DIO2  (FhssChangeChannel; unused in Meshtastic LoRa mode)
//   UEXT  7 (GPIO15)   -> SX1276 MISO
//   UEXT  8 (GPIO2)    -> SX1276 MOSI
//   UEXT  9 (GPIO14)   -> SX1276 SCK
//   UEXT 10 (GPIO5)    -> SX1276 NSS (CS)
//
// LAN8720 Ethernet (RMII) is not supported by Meshtastic firmware.
// The following GPIO pins are reserved by the LAN8720 PHY and must not be used:
//
//   ESPHome / yaml reference config for LAN8720 on this board:
//     ethernet:
//       type: LAN8720
//       mdc_pin: GPIO23        -> reserved
//       mdio_pin: GPIO18       -> reserved
//       clk_mode: GPIO17_OUT   -> reserved (GPIO17 drives 50MHz ref clock to LAN8720)
//       phy_addr: 0
//       power_pin: GPIO12      -> reserved (LAN8720 reset/enable)
//
//   Additionally, the RMII data bus occupies:
//       GPIO0  (ETH_CLK input, or ref clock in alternate mode)
//       GPIO19 (EMAC_TXD0), GPIO21 (EMAC_TX_EN), GPIO22 (EMAC_TXD1)
//       GPIO25 (EMAC_RXD0), GPIO26 (EMAC_RXD1), GPIO27 (EMAC_CRS_DV)
//
//   Total reserved by LAN8720: GPIO0, 12, 17, 18, 19, 21, 22, 23, 25, 26, 27

// No I2C - UEXT SCL/SDA are used by MOD-LoRa (RESET and DIO2)
#define HAS_WIRE 0
#undef I2C_SDA
#undef I2C_SCL

// No screen
#define HAS_SCREEN 0

// No GPS - UEXT UART pins are used by MOD-LoRa (DIO0 and DIO1)
#undef GPS_RX_PIN
#undef GPS_TX_PIN
#define HAS_GPS 0

// Built-in button (KEY_BUILTIN on ESP32-POE-ISO)
#define BUTTON_PIN 34
#define BUTTON_NEED_PULLUP

// No battery ADC (board is POE/USB powered)
#undef BATTERY_PIN

// SPI for SX1276 via UEXT
#undef LORA_SCK
#define LORA_SCK  14 // UEXT pin 9
#undef LORA_MISO
#define LORA_MISO 15 // UEXT pin 7
#undef LORA_MOSI
#define LORA_MOSI  2 // UEXT pin 8
#undef LORA_CS
#define LORA_CS    5 // UEXT pin 10 (NSS)

// SX1276 DIO and RESET via UEXT
#define LORA_DIO0  4  // UEXT pin 3: primary interrupt (RxDone/TxDone/CadDone)
#define LORA_RESET 16 // UEXT pin 5: active-low reset
#define LORA_DIO1  36 // UEXT pin 4: RxTimeout (input-only GPIO36, valid for interrupt)
#define LORA_DIO2  13 // UEXT pin 6: not used in LoRa mode
#define LORA_DIO3     // Not connected

// SX1276 via RF95 driver only
#define USE_RF95

// LAN8720 RMII Ethernet via native ESP32 ETH driver
// Pins are already defined in the board's pins_arduino.h:
//   ETH_PHY_ADDR  0   (PHYAD0 pulled down on ESP32-POE-ISO)
//   ETH_PHY_POWER 12  (LAN8720 nRST / enable)
//   ETH_PHY_MDC   23
//   ETH_PHY_MDIO  18
//   ETH_CLK_MODE  ETH_CLOCK_GPIO17_OUT  (ESP32 drives 50MHz ref clock to LAN8720 via GPIO17)
#define HAS_ETHERNET 1
#define USE_LAN8720
