// ============================================================================
// TTN uplink payload formatter for Example 2 (Join The Things Network)
// Author: Saad-Imtiaz
// ----------------------------------------------------------------------------
// Paste into the TTN application:
//   Payload formatters > Uplink > Custom Javascript formatter
//
// Payload (4 bytes, big-endian):
//   bytes 0-1  uplink counter
//   bytes 2-3  supply voltage of the module in mV
// ============================================================================
function decodeUplink(input) {
  var b = input.bytes;
  if (b.length < 4) return { errors: ["payload too short"] };
  return {
    data: {
      counter: (b[0] << 8) | b[1],
      supply_mv: (b[2] << 8) | b[3]
    }
  };
}
