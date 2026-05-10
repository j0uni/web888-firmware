// SPDX-License-Identifier: GPL-3.0-or-later
// HAB / U4B / Traquito basic-telemetry helper for WSPR spots.
//
// Detects WSPR Type 1 spots that match the QRP-Labs U4B / Traquito
// "telemetry packet" callsign+grid pattern and decodes the basic-telemetry
// payload (grid5/6, altitude, temperature, voltage, ground speed, GPS valid,
// telemetry type) into JSON for MQTT consumers.
//
// Decoder algorithm follows the canonical Traquito reference implementation:
//   https://github.com/traquito/WsprEncoded — src/WsprMessageTelemetryBasic.h
//
// References:
//   - QRP Labs U4B protocol: https://qrp-labs.com/u4b/u4bdecoding.html
//   - Traquito basic telemetry: https://traquito.github.io/pro/telemetry/basic/

#pragma once

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Test whether (call, grid, dBm) look like a U4B "telemetry packet":
// callsign[0] in {'0','1','Q'}, callsign[2] is digit, exact 6-char callsign,
// 4-char grid letters+digits in U4B-allowed alphabet, dBm in WSPR power set.
// Caller should also pass r_valid: only Type 1 messages carry U4B telemetry.
bool wspr_hab_looks_like_u4b(int r_valid, const char *call, const char *grid, int dBm);

// Build a JSON fragment for the "hab" sub-object describing the spot.
// Output format (always includes raw character indexes; decoded values
// are present only when valid_decode is true):
//   {"kind":"u4b_basic","ch":{"id1":"X","id3":"X"},
//    "raw":{"c1":"X","c2":"X","c3":"X","c4":"X","c5":"X","c6":"X",
//           "g1":"X","g2":"X","g3":"X","g4":"X","p":N},
//    "grid56":"AB","grid6":"AB12cd","alt_m":N,"temp_c":N,"v":N.NN,"kn":N,
//    "gps_valid":true|false,"tlm_type":"standard"|"extended"}
//
// Returns number of characters written (excluding NUL), or -1 on error.
// Caller must have validated wspr_hab_looks_like_u4b first.
int wspr_hab_format_json(const char *call, const char *grid, int dBm,
                         char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
