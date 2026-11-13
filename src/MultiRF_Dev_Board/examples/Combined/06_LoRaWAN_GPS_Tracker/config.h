/*
  ============================================================================
  config.h - settings for Example 6 (LoRaWAN GPS tracker)
  Author:  Saad-Imtiaz
  ----------------------------------------------------------------------------
  Keys are plain hex from the TTN console, no spaces or colons.
  ============================================================================
*/
#pragma once

// ---- LoRaWAN -----------------------------------------------------------------
#define LORAWAN_REGION   5                                    // 5 = EU868, 8 = US915, 7 = IN865
#define LORAWAN_DEVEUI   ""                                   // "" keeps the module's own DevEUI
#define LORAWAN_APPEUI   "0000000000000000"
#define LORAWAN_APPKEY   "00000000000000000000000000000000"

// A moving node should not use ADR, so the data rate is fixed.
// EU868: 0 = SF12, 1 = SF11, 2 = SF10, 3 = SF9, 4 = SF8, 5 = SF7.
// At SF9 the 11-byte payload is about 0.2 s on air, which allows roughly
// one uplink every 10 minutes under TTN's 30 s/day fair use policy.
#define TRACKER_DATARATE  3
#define TRACKER_PORT      2
#define REPORT_INTERVAL_MS (10UL * 60UL * 1000UL)   // 10 minutes

// ---- GNSS --------------------------------------------------------------------
#define FIX_TIMEOUT_MS    90000UL   // give up on a fix after this long
#define GPS_WAKE_LEAD_MS  30000UL   // wake the receiver this long before a report
#define MAX_HDOP          5.0       // reject fixes with poor satellite geometry
#define MIN_SATS          4         // a 3D fix needs at least 4 satellites
