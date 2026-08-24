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

    char json[512];
    int n = wspr_hab_format_json("QJ9FGP", "MB43", 7,
                                 "G4BKC", "IO92", delta_hz,
                                 json, sizeof(json));
    assert(n > 0);
    assert(strstr(json, "\"kind\":\"u4b_basic\"") != NULL);
    assert(strstr(json, "\"source_call\":\"G4BKC\"") != NULL);
    assert(strstr(json, "\"grid4\":\"IO92\"") != NULL);
    assert(strstr(json, "\"grid6\":\"IO92ne\"") != NULL);
    assert(strstr(json, "MB43ne") == NULL);
    assert(strstr(json, "\"alt_m\":140") != NULL);
    assert(strstr(json, "\"gps_valid\":true") != NULL);
}

static void test_unpaired_candidate_has_no_claimed_position()
{
    char json[512];
    int n = wspr_hab_format_json("QJ9FGP", "MB43", 7,
                                 NULL, NULL, 0.0,
                                 json, sizeof(json));
    assert(n > 0);
    assert(strstr(json, "\"kind\":\"u4b_candidate_basic\"") != NULL);
    assert(strstr(json, "\"paired\":false") != NULL);
    assert(strstr(json, "\"grid6\"") == NULL);
    assert(strstr(json, "\"source_call\"") == NULL);
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
    test_pair_rejections();
    puts("wspr_hab_test: PASS");
    return 0;
}
