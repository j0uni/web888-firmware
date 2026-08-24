#include "wspr_hab.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_observed_g4bkc_pair()
{
    double delta_hz = -1.0;
    assert(wspr_hab_pair_matches(1, "G4BKC", "IO92", 19, 14,
                                 14.097184, 19, 16, 14.097186,
                                 &delta_hz));
    assert(delta_hz > 1.9 && delta_hz < 2.1);

    wspr_hab_pair_info_t pair = {};
    pair.status = WSPR_HAB_PAIR_MATCHED;
    pair.source_call = "G4BKC";
    pair.source_grid = "IO92";
    pair.slot_epoch = 1787565960;
    pair.source_slot_epoch = 1787565840;
    pair.pair_delta_hz = delta_hz;
    pair.source_freq_MHz = 14.097184;
    pair.telemetry_freq_MHz = 14.097186;
    pair.source_snr = -19.0;
    pair.candidate_count = 1;

    char json[1024];
    int n = wspr_hab_format_json("QJ9FGP", "MB43", 7,
                                 &pair,
                                 json, sizeof(json));
    assert(n > 0);
    assert(strstr(json, "\"schema\":2") != NULL);
    assert(strstr(json, "\"kind\":\"u4b_basic\"") != NULL);
    assert(strstr(json, "\"status\":\"confirmed\"") != NULL);
    assert(strstr(json, "\"payload_call\":\"G4BKC\"") != NULL);
    assert(strstr(json, "\"source_call\":\"G4BKC\"") != NULL);
    assert(strstr(json, "\"grid4\":\"IO92\"") != NULL);
    assert(strstr(json, "\"grid6\":\"IO92ne\"") != NULL);
    assert(strstr(json, "\"slot_epoch\":1787565960") != NULL);
    assert(strstr(json, "\"source_slot_epoch\":1787565840") != NULL);
    assert(strstr(json, "\"candidates\":1") != NULL);
    assert(strstr(json, "\"source_freq_MHz\":14.097184") != NULL);
    assert(strstr(json, "MB43ne") == NULL);
    assert(strstr(json, "\"alt_m\":140") != NULL);
    assert(strstr(json, "\"gps_valid\":true") != NULL);
}

static void test_unpaired_candidate_has_no_claimed_position()
{
    wspr_hab_pair_info_t pair = {};
    pair.status = WSPR_HAB_PAIR_NONE;
    pair.slot_epoch = 1787565960;
    pair.telemetry_freq_MHz = 14.097186;

    char json[1024];
    int n = wspr_hab_format_json("QJ9FGP", "MB43", 7,
                                 &pair,
                                 json, sizeof(json));
    assert(n > 0);
    assert(strstr(json, "\"kind\":\"u4b_candidate_basic\"") != NULL);
    assert(strstr(json, "\"status\":\"unpaired\"") != NULL);
    assert(strstr(json, "\"paired\":false") != NULL);
    assert(strstr(json, "\"reason\":\"no_regular_packet_in_previous_slot\"") != NULL);
    assert(strstr(json, "\"grid6\"") == NULL);
    assert(strstr(json, "\"source_call\"") == NULL);
    assert(strstr(json, "\"payload_call\"") == NULL);
}

static void test_ambiguous_candidate_is_not_located()
{
    wspr_hab_pair_info_t pair = {};
    pair.status = WSPR_HAB_PAIR_AMBIGUOUS;
    pair.slot_epoch = 1787565960;
    pair.telemetry_freq_MHz = 14.097186;
    pair.candidate_count = 2;

    char json[1024];
    int n = wspr_hab_format_json("QJ9FGP", "MB43", 7,
                                 &pair, json, sizeof(json));
    assert(n > 0);
    assert(strstr(json, "\"status\":\"ambiguous\"") != NULL);
    assert(strstr(json, "\"reason\":\"multiple_frequency_matches\"") != NULL);
    assert(strstr(json, "\"candidates\":2") != NULL);
    assert(strstr(json, "\"grid6\"") == NULL);
    assert(strstr(json, "\"source_call\"") == NULL);
}

static void test_slot_epoch_date_resolution()
{
    assert(wspr_hab_slot_epoch(1787566045, 10, 6) == 1787565960);
    // At 00:00:30 UTC, a 23:58 slot belongs to the preceding UTC day.
    assert(wspr_hab_slot_epoch(1787529630, 23, 58) == 1787529480);
    assert(wspr_hab_slot_epoch(1787529630, 24, 0) == -1);
}

static void test_pair_rejections()
{
    assert(!wspr_hab_pair_matches(1, "G4BKC", "IO92", 19, 12,
                                  14.097184, 19, 16, 14.097186, NULL));
    assert(!wspr_hab_pair_matches(1, "G4BKC", "IO92", 19, 14,
                                  14.097160, 19, 16, 14.097186, NULL));
    assert(!wspr_hab_pair_matches(2, "G4BKC", "IO92", 19, 14,
                                  14.097184, 19, 16, 14.097186, NULL));
    assert(!wspr_hab_pair_matches(1, "QJ9FGP", "MB43", 19, 14,
                                  14.097184, 19, 16, 14.097186, NULL));

    // Midnight rollover is still exactly one two-minute WSPR slot.
    assert(wspr_hab_pair_matches(1, "G4BKC", "IO92", 23, 58,
                                 14.097184, 0, 0, 14.097184, NULL));
}

int main()
{
    assert(wspr_hab_looks_like_u4b(1, "QJ9FGP", "MB43", 7));
    test_observed_g4bkc_pair();
    test_unpaired_candidate_has_no_claimed_position();
    test_ambiguous_candidate_is_not_located();
    test_slot_epoch_date_resolution();
    test_pair_rejections();
    puts("wspr_hab_test: PASS");
    return 0;
}
