// ============================================================================
// TTN uplink payload formatter for Example 6 (LoRaWAN GPS tracker)
// Author: Saad-Imtiaz
// ----------------------------------------------------------------------------
// Paste into the TTN application:
//   Payload formatters > Uplink > Custom Javascript formatter
//
// Payload (11 bytes, big-endian):
//   bytes 0-3   latitude,  int32, units of 1e-7 degrees
//   bytes 4-7   longitude, int32, units of 1e-7 degrees
//   bytes 8-9   altitude,  int16, metres
//   byte  10    satellites in use
//
// Fields named latitude, longitude and altitude are recognised as a
// location by TTN and most map integrations.
// ============================================================================
function decodeUplink(input) {
  var b = input.bytes;
  if (b.length < 11) return { errors: ["expected 11 bytes"] };

  // JavaScript bit operators work on signed 32-bit numbers, so the
  // latitude and longitude come out signed without extra work.
  var lat = (b[0] << 24 | b[1] << 16 | b[2] << 8 | b[3]) / 1e7;
  var lon = (b[4] << 24 | b[5] << 16 | b[6] << 8 | b[7]) / 1e7;

  // Altitude is 16 bits: shift up and back down to restore the sign,
  // so a tracker below sea level does not report 65 km.
  var alt = (b[8] << 8 | b[9]) << 16 >> 16;

  return { data: { latitude: lat, longitude: lon, altitude: alt, sats: b[10] } };
}
