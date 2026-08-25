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
#include <stdint.h>

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

typedef enum {
    WSPR_HAB_PAIR_NONE = 0,
    WSPR_HAB_PAIR_MATCHED,
    WSPR_HAB_PAIR_AMBIGUOUS
} wspr_hab_pair_status_t;

// Pairing evidence included in schema-v2 MQTT objects. source_* fields are
// required only for WSPR_HAB_PAIR_MATCHED. candidate_count counts all regular
// packets inside the pairing bounds, including candidates rejected because
// their frequencies were too close to distinguish safely.
typedef struct {
    wspr_hab_pair_status_t status;
    const char *source_call;
    const char *source_grid;
    int64_t slot_epoch;
    int64_t source_slot_epoch;
    double pair_delta_hz;
    double source_freq_MHz;
    double telemetry_freq_MHz;
    double source_snr;
    int candidate_count;
} wspr_hab_pair_info_t;

// Resolve an HH:MM WSPR slot to the most recent such UTC time at or before
// publish_epoch. This handles the 23:58 -> 00:00 date rollover.
int64_t wspr_hab_slot_epoch(int64_t publish_epoch, int hour, int min);

// Build a JSON fragment for the "hab" sub-object describing the spot.
// Schema-v2 output always includes raw character indexes. A confirmed basic
// packet includes status, a payload_call alias, exact slot epochs and nested
// frequency-pairing evidence in addition to the original schema-v1 fields:
//   {"schema":2,"kind":"u4b_basic","status":"confirmed","paired":true,
//    "payload_call":"K1ABC","source_call":"K1ABC","grid4":"FN42",
//    "slot_epoch":N,"source_slot_epoch":N,"pair":{...},
//    "ch":{"id1":"X","id3":"X"},
//    "raw":{"c1":"X","c2":"X","c3":"X","c4":"X","c5":"X","c6":"X",
//           "g1":"X","g2":"X","g3":"X","g4":"X","p":N},
//    "grid56":"AB","grid6":"FN42ab","alt_m":N,"temp_c":N,"v":N.NN,"kn":N,
//    "gps_valid":true|false,"tlm_type":"standard"|"extended"}
//
// Without a paired regular spot, basic telemetry has status "unpaired" or
// "ambiguous", is emitted as "u4b_candidate_basic", and deliberately has no
// grid4/grid6/source_call/payload_call.
// The 4-character field in the telemetry packet is encoded sensor data, not
// the first four characters of the balloon's Maidenhead locator.
//
// Returns number of characters written (excluding NUL), or -1 on error.
// Caller must have validated wspr_hab_looks_like_u4b first.
int wspr_hab_format_json(const char *call, const char *grid, int dBm,
                         const wspr_hab_pair_info_t *pair,
                         char *out, size_t out_size);

#ifdef __cplusplus
}
#endif
