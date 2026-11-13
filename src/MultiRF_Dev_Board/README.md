# MultiRF Dev Board Arduino Library

![Platform](https://img.shields.io/badge/platform-ESP32--S3-blue)
![Arduino core](https://img.shields.io/badge/arduino--esp32-3.x-green)
![Version](https://img.shields.io/badge/version-0.1.0-lightgrey)

Arduino library and examples for the **Elektor MultiRF Dev Board**: one ESP32-S3 with four radios on a single PCB. It is the companion code to the Elektor **Wireless Workshop** series, which takes the radios one at a time, from a first "is it alive?" test to a working application.

**Author:** Saad-Imtiaz (Elektor)

---

## Contents

- [What is on the board](#what-is-on-the-board)
- [Quick start](#quick-start)
- [Installation](#installation)
- [Examples](#examples)
- [Using the library in your own sketch](#using-the-library-in-your-own-sketch)
- [API reference](#api-reference)
- [Pin map](#pin-map)
- [The Things Network setup](#the-things-network-setup)
- [Troubleshooting](#troubleshooting)
- [Radio regulations](#radio-regulations)
- [Roadmap](#roadmap)
- [References](#references)
- [Author and license](#author-and-license)

---

## What is on the board

| Module | Radio | Interface | Library support |
| --- | --- | --- | --- |
| Ebyte **E77-900M22S** (MOD1) | LoRaWAN node, 850 to 930 MHz, STM32WLE5 | UART1, AT commands | Yes: `E77AT.h`, 3 examples |
| Ebyte **EWM108-GN05** (MOD3) | GNSS receiver, GPS/Galileo/BeiDou/GLONASS, L1 + L5 | UART2, NMEA + binary | Yes: `GnssBinary.h`, 2 examples |
| Both together | LoRaWAN GPS tracker | | 1 example |
| **CC1101** (K3) | 433 MHz sub-GHz transceiver | SPI | Coming (Wireless Workshop 1) |
| Ebyte **E01-2G4M27D** nRF24L01P (K2) | 2.4 GHz transceiver | SPI | Coming (Wireless Workshop 2) |

The controller is an **ESP32-S3-WROOM-1**, which also brings Wi-Fi and Bluetooth LE.

## Quick start

Five steps from a fresh board to your first LoRaWAN message:

1. Install the library and its dependencies (see [Installation](#installation)).
2. Open **File > Examples > MultiRF Dev Board > E77_LoRaWAN > 01_E77_SerialBridge**, upload, and type `AT` in the serial monitor (115200 baud, "Both NL & CR"). The module answers `OK`.
3. Type `AT+CDEVEUI=?` and note the DevEUI.
4. Register the device on The Things Network (see [The Things Network setup](#the-things-network-setup)) and copy the keys into `config.h` of **02_E77_JoinTTN**.
5. Upload **02_E77_JoinTTN**. When LED5 lights up, the module has joined; a few seconds later the first uplink shows up in the TTN console.

For the GPS: upload **EWM108_GNSS > 04_GNSS_ReadNMEA**, take the antenna outside or to a window, and the position appears within about half a minute.

## Installation

### 1. Board support

In the Arduino IDE, open **Tools > Board > Boards Manager**, search for **esp32** and install **esp32 by Espressif Systems**, version 3.x.

Board settings:

| Setting | Value |
| --- | --- |
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | **Enabled** (so `Serial` uses the USB-C port) |
| Serial monitor | 115200 baud |

### 2. This library

- **ZIP:** download the repository as a ZIP and use **Sketch > Include Library > Add .ZIP Library...**, selecting the `MultiRF_Dev_Board` folder or ZIP.
- **By hand:** copy the `MultiRF_Dev_Board` folder into your `libraries` folder (`Documents/Arduino/libraries` on Windows and macOS, `~/Arduino/libraries` on Linux), then restart the IDE.

### 3. Dependencies

Install **TinyGPSPlus** by Mikal Hart from the Library Manager. The GNSS and tracker examples need it to decode NMEA. The IDE offers to install it automatically when you install this library.

## Examples

All examples are in **File > Examples > MultiRF Dev Board**, grouped by module.

| Example | What it does | What you learn |
| --- | --- | --- |
| `E77_LoRaWAN/01_E77_SerialBridge` | Bridges the USB console to the LoRaWAN module | How AT commands work; checking firmware and region |
| `E77_LoRaWAN/02_E77_JoinTTN` | Sets region and keys, joins TTN, sends a counter and supply voltage every 5 min | OTAA join, events, uplinks, payload formatters |
| `E77_LoRaWAN/03_E77_LinkCheck` | Ten link checks, 3 min apart, at a fixed data rate | RSSI, SNR, link margin, why ADR is off for moving nodes |
| `EWM108_GNSS/04_GNSS_ReadNMEA` | Prints time, position, altitude, satellites, HDOP; `r` shows raw NMEA | NMEA sentences, fix validity, UART buffer sizing |
| `EWM108_GNSS/05_GNSS_Config` | Serial menu for binary commands with a time-to-first-fix stopwatch | Cold vs hot start, the Allystar binary protocol |
| `Combined/06_LoRaWAN_GPS_Tracker` | Sends an 11-byte position every 10 min at SF9; GNSS sleeps between reports | Binary payloads, airtime and fair use, power saving |

Notes:

- The LoRaWAN examples read their keys from **`config.h`** in the sketch folder. Open it as a tab in the IDE.
- Examples 02 and 06 include **`ttn_payload_formatter.js`**: paste it into the TTN console to see decoded values.
- On the board, **LED2** flashes on transmit, **LED4** on receive, and **LED5** stays on once the module has joined. These LEDs are driven by the module itself, so they work with any sketch.

## Using the library in your own sketch

```cpp
#include <TinyGPSPlus.h>
#include <MultiRFDevBoard.h>

TinyGPSPlus gps;

void setup() {
  Serial.begin(115200);
  e77Begin();                              // LoRaWAN module on UART1
  gnssBegin();                             // GNSS receiver on UART2

  e77EnsureRegion(5);                      // EU868
  e77SetKeys("", "0000000000000000", "<your 32-character AppKey>");
  e77Join();
}

void loop() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());

  static uint32_t last = 0;
  if (millis() - last > 600000UL && gps.location.isValid()) {   // every 10 min
    last = millis();
    uint8_t sats = gps.satellites.value();
    e77Send(2, &sats, 1);                  // send one byte on port 2
  }
}
```

## API reference

### LoRaWAN module: `E77AT.h`

| Function | Description |
| --- | --- |
| `e77Begin(rx, tx, baud)` | Open UART1. Defaults: IO48, IO47, 9600 baud |
| `e77SetEcho(on)` | Print every command and reply on `Serial` (default on) |
| `e77SetSendFields(n)` | `AT+SEND` format: 4 fields (default) or 3, see [Troubleshooting](#troubleshooting) |
| `atCmd(cmd, timeoutMs)` | Send a command, return the reply up to `OK` or an `AT_` error |
| `atOk(reply)` | True if the reply is `OK` without an error |
| `waitFor(token, timeoutMs)` | Wait for an event such as `+EVT:JOINED` |
| `e77ReadLine(line)` | Non-blocking: returns true with one complete line from the module |
| `e77EnsureRegion(region)` | Set the region; restores defaults only if the region was read and differs |
| `e77SetKeys(devEui, appEui, appKey)` | Write OTAA keys as hex; warns about wrong lengths or an all-zero AppKey |
| `e77Join(maxAttempts)` | OTAA join, waiting for the event, with back-off 15 s, 30 s, 60 s, up to 2 min |
| `e77Send(port, data, len, confirmed)` | Send up to 51 bytes on port 1 to 199. Returns an `E77SendResult` |
| `e77ParseRx(line, info)` | Decode a `+EVT:RX_...` line into an `E77RxInfo` |

`E77SendResult`: `E77_SENT`, `E77_DUTYCYCLE`, `E77_NOT_JOINED`, `E77_BUSY`, `E77_ERROR`.

`E77RxInfo` fields: `window`, `dr`, `rssi` (dBm), `snr` (dB), `margin` (dB, link check only), `gateways` (link check only).

The UART is available as **`lora`** (a reference to `Serial1`).

Region numbers: 0 = AS923, 1 = AU915, 5 = EU868, 6 = KR920, 7 = IN865, 8 = US915, 9 = RU864.

### GNSS receiver: `GnssBinary.h`

| Function | Description |
| --- | --- |
| `gnssBegin(rx, tx, baud)` | Open UART2 with a 1 KB receive buffer. Defaults: IO16, IO15, 115200 baud |
| `gnssSend(group, id, payload, len)` | Send any Allystar binary frame; the checksum is added |
| `gnssSetNmeaRate(sentence, period)` | Turn an NMEA sentence off (0) or on (1). `NMEA_GGA`, `NMEA_GSV`, `NMEA_GSA` ... |
| `gnssRestart(mode)` | `GNSS_COLD`, `GNSS_WARM` or `GNSS_HOT` start |
| `gnssSave(mask)` | Save settings to flash (default: baud rate and sentence rates) |
| `gnssFactoryReset()` | Restore all factory settings |
| `gnssSleep(ms, mode)` | Sleep for `ms`, then resume by itself. Mode 1 (deep sleep) keeps satellite data |
| `GnssBinParser` | Separates binary replies from NMEA text in the incoming stream |
| `gnssPrintFrame(parser)` | Print an ACK/NAK or other binary frame |

The UART is available as **`gpsSerial`** (a reference to `Serial2`). Feed its bytes to TinyGPSPlus.

### Pins: `MultiRF_Pins.h`

`MULTIRF_E77_RX`, `MULTIRF_E77_TX`, `MULTIRF_E77_BAUD`, `MULTIRF_GNSS_RX`, `MULTIRF_GNSS_TX`, `MULTIRF_GNSS_BAUD`.

## Pin map

Traced on the board schematic (250550-001). RX and TX are seen from the ESP32.

| Signal | ESP32-S3 | Module pin |
| --- | --- | --- |
| LoRaWAN module TX to ESP32 RX | IO48 | E77-900M22S PA2 (pin 9) |
| ESP32 TX to LoRaWAN module RX | IO47 | E77-900M22S PA3 (pin 10) |
| GNSS TXD to ESP32 RX | IO16 | EWM108-GN05 pin 2, also K8 pin 3 |
| ESP32 TX to GNSS RXD | IO15 | EWM108-GN05 pin 3, also K8 pin 4 |
| USB D- / D+ | IO19 / IO20 | USB-C, native USB |

Good to know:

- The LoRaWAN module's reset pin is not connected to the ESP32. Use `AT+RST`.
- Both modules run directly from the 3.3 V rail. The GNSS receiver can be put to sleep by command (`gnssSleep()`), but not switched off.
- The GNSS backup pin (V_BCKP) is fed by a 0.1 F supercapacitor (C12) through D3. After power-off it keeps satellite data for a hot start for roughly two to three hours (calculated, not yet measured).
- The GNSS antenna input has a bias feed (L1), so active antennas get power; passive antennas work too.
- The GNSS UART is also on header **K8** (GND, 3.3 V, TXD, RXD) for a USB-to-serial adapter or Ebyte's Satrack_client tool. Do not drive RXD from an adapter while a sketch drives IO15.

## The Things Network setup

1. Create an account and an application in the [TTN console](https://www.thethingsnetwork.org/).
2. Choose **Register end device > Enter end device specifics manually**.
3. Frequency plan: your region (for example Europe 863-870 MHz). LoRaWAN version: **1.0.4**.
4. **DevEUI:** the value from `AT+CDEVEUI=?` (example 01). **JoinEUI:** all zeros. **AppKey:** let TTN generate it.
5. Copy the values into `config.h` as plain hex, without spaces or colons.
6. For examples 02 and 06: **Payload formatters > Uplink > Custom Javascript formatter**, paste `ttn_payload_formatter.js`.

How often may you send? TTN allows 30 s of uplink airtime and 10 downlinks per device per day, and EU868 law allows 1% airtime. For the tracker's 11-byte message that means roughly one uplink every 3 minutes at SF7, every 10 minutes at SF9, and every 72 minutes at SF12.

## Troubleshooting

| Symptom | Cause and fix |
| --- | --- |
| No reply to `AT` | Check 9600 baud, "Both NL & CR", and that RX and TX are crossed |
| `AT+REGION` rejected, `AT+BAND=?` works | The module has ST's reference firmware, not Ebyte's. See ST AN5481 for its commands |
| "Could not read the region" | Wiring or firmware issue. The library does not restore the module in this case |
| Join fails every time | Wrong region, mistyped AppKey, or no gateway in range (check the TTN coverage map) |
| Joined once, never again after `AT+RESTORE` | LoRaWAN 1.0.4 join counter (DevNonce) was reset. Reset the used DevNonces in the TTN console |
| `E77_ERROR` on every send | Your firmware may use the 3-field `AT+SEND`. Type `AT+SEND?`; if it shows three fields, call `e77SetSendFields(3)` after `e77Begin()` |
| `E77_DUTYCYCLE` | Sending too often for the data rate. Lengthen the interval |
| Link check shows no margin | The answer format depends on the firmware. Example 03 prints raw lines; check them once |
| GPS never gets a fix | Go outside. A cold start takes about 30 s in open sky, minutes indoors. Look for several satellites above 30 dB-Hz in the GSV sentences |
| Bad checksums in GPS data | The receive buffer overflows while the loop blocks. Use `gnssBegin()` (1 KB buffer) and turn off GSV/GSA if you do not need them |

## Radio regulations

The E77-900M22S can transmit at up to 22 dBm, more than the permitted power on the standard EU868 LoRaWAN channels. Always set the correct region before transmitting, keep `AT+CTXP=0` (the maximum allowed in the selected region), leave the duty cycle limit enabled, and only transmit with an antenna made for the band. Rules differ by country; check the regulations where you operate.

## Roadmap

- [x] E77-900M22S LoRaWAN helpers and examples (Wireless Workshop 3)
- [x] EWM108-GN05 GNSS helpers and examples (Wireless Workshop 3)
- [ ] CC1101 examples and pins (Wireless Workshop 1)
- [ ] nRF24L01P examples and pins (Wireless Workshop 2)
- [ ] ESP-NOW, Wi-Fi and Bluetooth LE examples
- [ ] Range test across all radios, with GPS-tagged results

## References

- [E77-900M22S product page and downloads](https://www.cdebyte.com/products/E77-900M22S/4#Downloads) (user manual V1.6, AT commands)
- [EWM108-GN05 product page and downloads](https://www.cdebyte.com/products/EWM108-GN05/4#Downloads) (user manual V1.1, Allystar binary protocol V2.3.8)
- [ST AN5481, LoRaWAN AT commands for STM32CubeWL](https://www.st.com/resource/en/application_note/an5481-lorawan-at-commands-for-stm32cubewl-stmicroelectronics.pdf)
- [The Things Network](https://www.thethingsnetwork.org/) and its [duty cycle and fair use policy](https://www.thethingsnetwork.org/docs/lorawan/duty-cycle/)
- [TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus)

Elektor articles:

- Saad Imtiaz, "MultiRF Dev Board," Elektor 7-8/2026: https://elektormagazine.com/250550-01
- Saad Imtiaz, "Wireless Workshop (1): 433 MHz CC1101 Sub-GHz Transceiver," Elektor 9-10/2026: https://elektormagazine.com/260397-01
- Saad Imtiaz, "Wireless Workshop (3): LoRaWAN with the E77-900M22S and Dual-Band GNSS with the EWM108-GN05," Elektor

## Author and license

Written by **Saad-Imtiaz** (Elektor) for the Elektor Wireless Workshop series.

Questions about the board or the articles: saad.imtiaz@elektor.com or editor@elektor.com.

License: *to be added*.

### Changelog

- **0.1.0**: first release. LoRaWAN (E77-900M22S) and GNSS (EWM108-GN05) helpers, six examples.
