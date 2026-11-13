/*
  ============================================================================
  File:     MultiRFDevBoard.h
  Library:  MultiRF Dev Board
  Author:   Saad-Imtiaz
  Series:   Elektor Wireless Workshop
  ----------------------------------------------------------------------------
  Main include of the library. One line in your sketch:

      #include <MultiRFDevBoard.h>

  gives you:
      MultiRF_Pins.h   the board's pin map
      E77AT.h          AT command helpers for the E77-900M22S LoRaWAN module
      GnssBinary.h     binary protocol helpers for the EWM108-GN05 GNSS receiver

  Each header can also be included on its own.
  ============================================================================
*/
#pragma once

#include "MultiRF_Pins.h"
#include "E77AT.h"
#include "GnssBinary.h"

#define MULTIRF_DEV_BOARD_LIB_VERSION "0.1.0"
