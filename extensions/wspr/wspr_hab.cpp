// SPDX-License-Identifier: GPL-3.0-or-later
// HAB / U4B / Traquito basic-telemetry decoder for WSPR spots.
// See wspr_hab.h for design notes and references.

#include "wspr_hab.h"

#include <ctype.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// WSPR power levels in dBm (19 entries), index = "powerVal" used by U4B encode.
static const uint8_t kWsprPowerDbm[19] = {
    0,  3,  7,
    10, 13, 17,
    20, 23, 27,
    30, 33, 37,
    40, 43, 47,
    50, 53, 57,
    60
};

static int decode_power_dbm_to_num(int dBm)
{
    for (int i = 0; i < (int)(sizeof(kWsprPowerDbm)/sizeof(kWsprPowerDbm[0])); i++) {
        if ((int)kWsprPowerDbm[i] == dBm) return i;
    }
    return -1;
}

static int decode_base36(char c)
{
    if (c >= 'A' && c <= 'Z') return 10 + (c - 'A');
    if (c >= '0' && c <= '9') return c - '0';
    return -1;
}

// id3 of U4B channel: 3rd callsign character (0-9).
// id1: 1st callsign character (0/1/Q). The pair (id1,id3) identifies the
// "channel" group together with the timeslot and frequency lane.
static bool valid_id1(char c) { return c == '0' || c == '1' || c == 'Q'; }

static int minute_of_day(int hour, int min)
{
    if (hour < 0 || hour > 23 || min < 0 || min > 59) return -1;
    return hour * 60 + min;
}

bool wspr_hab_looks_like_u4b(int r_valid, const char *call, const char *grid, int dBm)
{
    // Only Type 1 carries U4B basic telemetry.
    if (r_valid != 1) return false;
    if (!call || !grid) return false;
    if (strlen(call) != 6) return false;
    if (strlen(grid) != 4) return false;

    // Channel constraints: callsign[0] ∈ {0,1,Q}, callsign[2] is digit.
    if (!valid_id1(call[0])) return false;
    if (!isdigit((unsigned char)call[2])) return false;

    // Data positions: callsign[1] base36, callsign[3..5] uppercase letters.
    if (decode_base36(call[1]) < 0) return false;
    if (!(call[3] >= 'A' && call[3] <= 'Z')) return false;
    if (!(call[4] >= 'A' && call[4] <= 'Z')) return false;
    if (!(call[5] >= 'A' && call[5] <= 'Z')) return false;

    // Grid: U4B uses letters A..R for chars 0,1 (the standard Maidenhead
    // alphabet) and digits 0..9 for chars 2,3.
    if (!(grid[0] >= 'A' && grid[0] <= 'R')) return false;
    if (!(grid[1] >= 'A' && grid[1] <= 'R')) return false;
    if (!isdigit((unsigned char)grid[2])) return false;
    if (!isdigit((unsigned char)grid[3])) return false;

    // Power must be a valid WSPR power.
    if (decode_power_dbm_to_num(dBm) < 0) return false;

    return true;
}

bool wspr_hab_pair_matches(int regular_type, const char *regular_call,
                           const char *regular_grid, int regular_hour,
                           int regular_min, double regular_freq_MHz,
                           int telemetry_hour, int telemetry_min,
                           double telemetry_freq_MHz, double *delta_hz)
{
    if (delta_hz) *delta_hz = 0.0;
    if (regular_type != 1 || !regular_call || !regular_grid) return false;
    if (regular_call[0] == '\0' || valid_id1(regular_call[0])) return false;
    if (strlen(regular_grid) != 4) return false;

    int regular_mod = minute_of_day(regular_hour, regular_min);
    int telemetry_mod = minute_of_day(telemetry_hour, telemetry_min);
    if (regular_mod < 0 || telemetry_mod < 0) return false;
    if ((telemetry_mod - regular_mod + 24 * 60) % (24 * 60) != 2) return false;

    double diff_hz = telemetry_freq_MHz - regular_freq_MHz;
    if (diff_hz < 0.0) diff_hz = -diff_hz;
    diff_hz *= 1e6;
    if (diff_hz > 10.0) return false;

    if (delta_hz) *delta_hz = diff_hz;
    return true;
}

// Returns true if decoded telemetry is "basic" (telemetryId == 1).
// Fills out parameters even on extended/unknown telemetry types so the
// raw sub-fields are always available to the consumer.
static bool decode_u4b_basic(const char *call, const char *grid, int dBm,
                             char grid56[3], int *alt_m, int *temp_c,
                             double *voltage, int *speed_kn,
                             bool *gps_valid, int *tlm_type)
{
    int id2Val = decode_base36(call[1]);
    int id4Val = call[3] - 'A';
    int id5Val = call[4] - 'A';
    int id6Val = call[5] - 'A';

    // First "big number": grid5, grid6, altitude — encoded into callsign
    // chars 2,4,5,6.
    uint32_t val1 = 0;
    val1 = val1 * 36 + (uint32_t)id2Val;
    val1 = val1 * 26 + (uint32_t)id4Val;
    val1 = val1 * 26 + (uint32_t)id5Val;
    val1 = val1 * 26 + (uint32_t)id6Val;

    uint32_t altFracM = val1 % 1068; val1 /= 1068;
    uint32_t grid6Val = val1 %   24; val1 /=   24;
    uint32_t grid5Val = val1;        // remainder, < 24

    grid56[0] = (char)('A' + grid5Val);
    grid56[1] = (char)('A' + grid6Val);
    grid56[2] = '\0';
    *alt_m = (int)(altFracM * 20);

    // Second "big number": temperature, voltage, speed, gps valid, telemetry id
    // — encoded into 4-char grid + power.
    int g1Val = grid[0] - 'A';
    int g2Val = grid[1] - 'A';
    int g3Val = grid[2] - '0';
    int g4Val = grid[3] - '0';
    int powerVal = decode_power_dbm_to_num(dBm);

    uint32_t val2 = 0;
    val2 = val2 * 18 + (uint32_t)g1Val;
    val2 = val2 * 18 + (uint32_t)g2Val;
    val2 = val2 * 10 + (uint32_t)g3Val;
    val2 = val2 * 10 + (uint32_t)g4Val;
    val2 = val2 * 19 + (uint32_t)powerVal;

    uint32_t telemetryId   = val2 %  2; val2 /=  2;
    uint32_t bit2          = val2 %  2; val2 /=  2;
    uint32_t speedKnotsNum = val2 % 42; val2 /= 42;
    uint32_t voltageNum    = val2 % 40; val2 /= 40;
    uint32_t tempCNum      = val2 % 90;

    *tlm_type  = (int)telemetryId;        // 1 = basic, 0 = extended/other
    *gps_valid = bit2 != 0;
    *speed_kn  = (int)(speedKnotsNum * 2);
    *voltage   = 3.0 + (((voltageNum + 20) % 40) * 0.05);
    *temp_c    = (int)tempCNum - 50;

    return telemetryId == 1;
}

int wspr_hab_format_json(const char *call, const char *grid, int dBm,
                         const char *source_call, const char *source_grid,
                         double pair_delta_hz,
                         char *out, size_t out_size)
{
    if (!out || out_size == 0) return -1;
    if (!call || !grid) return -1;
    if (strlen(call) != 6 || strlen(grid) != 4) return -1;

    char grid56[3] = {0};
    int alt_m = 0, temp_c = 0, speed_kn = 0, tlm_type = 0;
    double voltage = 0.0;
    bool gps_valid = false;

    bool basic = decode_u4b_basic(call, grid, dBm,
                                  grid56, &alt_m, &temp_c,
                                  &voltage, &speed_kn,
                                  &gps_valid, &tlm_type);

    int powerVal = decode_power_dbm_to_num(dBm);

    // Always emit raw position breakdown so consumers can run their own
    // decoders (e.g. for extended telemetry / vendor-defined).
    bool paired = source_call && *source_call && source_grid && strlen(source_grid) == 4;
    int n;
    if (basic) {
        if (paired) {
            // The regular packet carries grid chars 1..4. The telemetry
            // packet's apparent grid is sensor data; only chars 5..6 are
            // recovered from its callsign payload.
            char grid6[7];
            snprintf(grid6, sizeof(grid6), "%s%s", source_grid, grid56);
            grid6[4] = (char)tolower((unsigned char)grid6[4]);
            grid6[5] = (char)tolower((unsigned char)grid6[5]);
            n = snprintf(out, out_size,
            "{\"kind\":\"u4b_basic\",\"paired\":true,"
            "\"source_call\":\"%s\",\"grid4\":\"%s\",\"pair_delta_hz\":%.1f,"
            "\"ch\":{\"id1\":\"%c\",\"id3\":\"%c\"},"
            "\"raw\":{\"c1\":\"%c\",\"c2\":\"%c\",\"c3\":\"%c\",\"c4\":\"%c\",\"c5\":\"%c\",\"c6\":\"%c\","
                    "\"g1\":\"%c\",\"g2\":\"%c\",\"g3\":\"%c\",\"g4\":\"%c\",\"p\":%d},"
            "\"grid56\":\"%s\",\"grid6\":\"%s\","
            "\"alt_m\":%d,\"temp_c\":%d,\"v\":%.2f,\"kn\":%d,"
            "\"gps_valid\":%s,\"tlm_type\":\"standard\"}",
            source_call, source_grid, pair_delta_hz,
            call[0], call[2],
            call[0], call[1], call[2], call[3], call[4], call[5],
            grid[0], grid[1], grid[2], grid[3], powerVal,
            grid56, grid6,
            alt_m, temp_c, voltage, speed_kn,
            gps_valid ? "true" : "false");
        } else {
            n = snprintf(out, out_size,
            "{\"kind\":\"u4b_candidate_basic\",\"paired\":false,"
            "\"ch\":{\"id1\":\"%c\",\"id3\":\"%c\"},"
            "\"raw\":{\"c1\":\"%c\",\"c2\":\"%c\",\"c3\":\"%c\",\"c4\":\"%c\",\"c5\":\"%c\",\"c6\":\"%c\","
                    "\"g1\":\"%c\",\"g2\":\"%c\",\"g3\":\"%c\",\"g4\":\"%c\",\"p\":%d},"
            "\"grid56\":\"%s\","
            "\"alt_m\":%d,\"temp_c\":%d,\"v\":%.2f,\"kn\":%d,"
            "\"gps_valid\":%s,\"tlm_type\":\"standard\"}",
            call[0], call[2],
            call[0], call[1], call[2], call[3], call[4], call[5],
            grid[0], grid[1], grid[2], grid[3], powerVal,
            grid56, alt_m, temp_c, voltage, speed_kn,
            gps_valid ? "true" : "false");
        }
    } else {
        // tlm_type == 0: this packet is U4B-shaped but is *not* basic
        // telemetry (likely "extended telemetry" — header bit reserved for
        // user/vendor-defined formats). We expose only the raw bits so a
        // downstream consumer can apply the appropriate extended decoder.
        if (paired) {
            n = snprintf(out, out_size,
                "{\"kind\":\"u4b_unknown\",\"paired\":true,"
                "\"source_call\":\"%s\",\"grid4\":\"%s\",\"pair_delta_hz\":%.1f,"
                "\"ch\":{\"id1\":\"%c\",\"id3\":\"%c\"},"
                "\"raw\":{\"c1\":\"%c\",\"c2\":\"%c\",\"c3\":\"%c\",\"c4\":\"%c\",\"c5\":\"%c\",\"c6\":\"%c\","
                        "\"g1\":\"%c\",\"g2\":\"%c\",\"g3\":\"%c\",\"g4\":\"%c\",\"p\":%d},"
                "\"tlm_type\":\"extended\"}",
                source_call, source_grid, pair_delta_hz, call[0], call[2],
                call[0], call[1], call[2], call[3], call[4], call[5],
                grid[0], grid[1], grid[2], grid[3], powerVal);
        } else {
            n = snprintf(out, out_size,
                "{\"kind\":\"u4b_unknown\",\"paired\":false,"
                "\"ch\":{\"id1\":\"%c\",\"id3\":\"%c\"},"
                "\"raw\":{\"c1\":\"%c\",\"c2\":\"%c\",\"c3\":\"%c\",\"c4\":\"%c\",\"c5\":\"%c\",\"c6\":\"%c\","
                        "\"g1\":\"%c\",\"g2\":\"%c\",\"g3\":\"%c\",\"g4\":\"%c\",\"p\":%d},"
                "\"tlm_type\":\"extended\"}",
                call[0], call[2],
                call[0], call[1], call[2], call[3], call[4], call[5],
                grid[0], grid[1], grid[2], grid[3], powerVal);
        }
    }

    if (n < 0 || (size_t)n >= out_size) return -1;
    return n;
}
