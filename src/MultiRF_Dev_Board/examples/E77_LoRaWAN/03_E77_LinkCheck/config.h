/*
  ============================================================================
  config.h - settings for Example 3 (link check)
  Author:  Saad-Imtiaz
  ----------------------------------------------------------------------------
  Use the same TTN device as Example 2. Keys are plain hex.
  ============================================================================
*/
#pragma once

#define LORAWAN_REGION   5                                    // 5 = EU868, 8 = US915, 7 = IN865
#define LORAWAN_DEVEUI   ""                                   // "" keeps the module's own DevEUI
#define LORAWAN_APPEUI   "0000000000000000"
#define LORAWAN_APPKEY   "00000000000000000000000000000000"

// Fixed data rate for the walk test (ADR is switched off).
// EU868: 0 = SF12, 1 = SF11, 2 = SF10, 3 = SF9, 4 = SF8, 5 = SF7
#define TEST_DATARATE    3

#define TEST_PORT        3

// Each test cycle costs one downlink (the link check answer), and TTN allows
// 10 downlinks per device per day. Ten cycles, 3 minutes apart, gives a
// 30-minute walk test that stays inside that limit.
#define TEST_INTERVAL_MS (3UL * 60UL * 1000UL)
#define TEST_CYCLES      10
