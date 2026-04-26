#pragma once

// Olimex ESP32-POE-ISO + MOD-LoRa868 (SX1276) via UEXT
// UEXT SPI pins are shared with the LoRa module — no other SPI devices.

// SX1276 (RF95) via UEXT SPI
#define USE_RF95
#define LORA_SCK  14   // UEXT pin 9
#define LORA_MISO 15   // UEXT pin 7
#define LORA_MOSI 2    // UEXT pin 8
#define LORA_CS   5    // UEXT pin 10 (NSS)
#define LORA_DIO0 4    // UEXT pin 3
#define LORA_RESET 16  // UEXT pin 5
#define LORA_DIO1 36   // UEXT pin 4 (input-only GPIO)
#define LORA_DIO2 13   // UEXT pin 6

// User button
#define BUTTON_PIN 34

// No I2C (UEXT I2C lines occupied by LoRa SPI), no GPS, no screen, no battery ADC
#define HAS_WIRE  0
#define HAS_GPS   0
#define HAS_SCREEN 0
#undef BATTERY_PIN
