/*
  ============================================================================
  config.h - LoRaWAN settings for Example 2 (Join The Things Network)
  Author:  Saad-Imtiaz
  ----------------------------------------------------------------------------
  Fill in the values from your device page in The Things Network console.
  Keys are plain hex: no spaces, no colons, no "0x".
  ============================================================================
*/
#pragma once

// Region: 0 = AS923, 1 = AU915, 5 = EU868, 6 = KR920, 7 = IN865, 8 = US915, 9 = RU864
#define LORAWAN_REGION   5

// Leave DEVEUI empty ("") to keep the module's own DevEUI. Read it with
// Example 1 (AT+CDEVEUI=?) and enter that value on TTN.
#define LORAWAN_DEVEUI   ""
#define LORAWAN_APPEUI   "0000000000000000"                   // JoinEUI: all zeros is fine for TTN
#define LORAWAN_APPKEY   "00000000000000000000000000000000"   // 32 hex characters from TTN

// Application port for the uplinks (1 to 199)
#define UPLINK_PORT      2

// Time between uplinks. 5 minutes stays well inside TTN's fair use policy
// (30 s of airtime per device per day).
#define UPLINK_INTERVAL_MS (5UL * 60UL * 1000UL)
