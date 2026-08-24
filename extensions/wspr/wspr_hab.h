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

// Test whether a regular WSPR spot can be paired with a telemetry candidate.
// U4B sends the regular packet first and telemetry exactly two minutes later
// on the same nominal frequency. The caller is responsible for rejecting an
// ambiguous match when more than one regular spot satisfies these bounds.
bool wspr_hab_pair_matches(int regular_type, const char *regular_call,
                           const char *regular_grid, int regular_hour,
                           int regular_min, double regular_freq_MHz,
                           int telemetry_hour, int telemetry_min,
                           double telemetry_freq_MHz, double *delta_hz);

// Build a JSON fragment for the "hab" sub-object describing the spot.
// Output format (always includes raw character indexes; decoded values
// are present only when valid_decode is true):
//   {"kind":"u4b_basic","paired":true,"source_call":"K1ABC",
//    "grid4":"FN42","ch":{"id1":"X","id3":"X"},
//    "raw":{"c1":"X","c2":"X","c3":"X","c4":"X","c5":"X","c6":"X",
//           "g1":"X","g2":"X","g3":"X","g4":"X","p":N},
//    "grid56":"AB","grid6":"FN42ab","alt_m":N,"temp_c":N,"v":N.NN,"kn":N,
//    "gps_valid":true|false,"tlm_type":"standard"|"extended"}
//
// Without a paired regular spot, basic telemetry is emitted as
// "u4b_candidate_basic" and deliberately has no grid4/grid6/source_call.
// The 4-character field in the telemetry packet is encoded sensor data, not
// the first four characters of the balloon's Maidenhead locator.
//
// Returns number of characters written (excluding NUL), or -1 on error.
// Caller must have validated wspr_hab_looks_like_u4b first.
int wspr_hab_format_json(const char *call, const char *grid, int dBm,
                         const char *source_call, const char *source_grid,
                         double pair_delta_hz,
                         char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
