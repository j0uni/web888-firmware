# WSPR MQTT topic — enhanced WSPR + HAB / U4B telemetry

This document specifies the MQTT message format that the Web-888 firmware (this fork) emits for every decoded WSPR spot, including the optional **HAB** (high-altitude balloon) sub-object that carries decoded U4B / Traquito basic-telemetry.

The intent is that consumers can:

- **Build a generic WSPR receiver dashboard** without touching the HAB part.
- **Run their own HAB / U4B decoder** using only the raw fields, even for telemetry types this firmware does not know about.
- **Cross-reference balloon telemetry** with external data (channel maps, operator callsigns, frequency-segment fingerprints from `wspr.live`/Traquito) — all the bits that were on the air are exposed.

## 1. Transport

- **Broker:** the MQTT broker configured in *Admin → Network → MQTT* (`mqtt_server`, `mqtt_port`, `mqtt_user`, `mqtt_password`).
- **Client ID:** the device's 64-bit DNA, formatted as `printf("%08x%08x", net.dna)` — 16 hex chars.
- **Protocol:** plain MQTT v3.1.1, QoS 0, no retain. Connection is auto-reconnected only at startup; messages are dropped silently if the broker is unreachable.
- **Topic structure:**

  ```
  web888/<client_id>/<event>
  ```

  With `<event>` ∈ { `start`, `stop`, `stat`, `WSPR`, … }. This document covers `WSPR`.

## 2. Outer envelope

All publishes are wrapped by `net/mqttpub.cpp` into:

```jsonc
{
  "timestamp": "<unix_seconds_as_string>",
  "server":    "<client_id>",
  // … fields written by the publisher …
}
```

So a WSPR message is always:

```jsonc
{
  "timestamp": "1747037880",
  "server":    "abcd1234deadbeef",
  // base WSPR fields (Section 3) …
  // optional "hab" sub-object (Section 4) …
}
```

`timestamp` is when the spot was published (after decode/upload), **not** the WSPR slot time. Use the `utc` / `hour` / `min` fields below for the slot.

## 3. Base WSPR fields

For every successfully decoded spot whose callsign is resolved, the publisher emits:

| Field        | Type    | Meaning                                                                                                                                              |
|--------------|---------|------------------------------------------------------------------------------------------------------------------------------------------------------|
| `call`       | string  | Unpacked callsign as displayed by `wsprd`. Type 3 hashed calls must be resolved in the running decoder's hash table; unresolved `"..."` spots are skipped. |
| `grid`       | string  | Maidenhead locator. **4 chars** for Type 1, **empty** for Type 2 (compound callsign), **6 chars** for Type 3.                                        |
| `msg`        | string  | Raw decoded WSPR text exactly as `wsprd` formats it: `"K1ABC FN42 30"`, `"PA0SKT/2 no_grid 33"`, `"<K1ABC> JO22hf 23"`. This is the canonical c_l_p. |
| `type`       | integer | `1`, `2`, or `3` — WSPR message type returned by the `unpk_()` decoder.                                                                              |
| `snr`        | number  | Signal-to-noise ratio in dB (1-decimal), as reported by `wsprd`.                                                                                     |
| `dt`         | number  | Time offset of the decoded signal vs slot start, seconds (1-decimal).                                                                                |
| `drift`      | integer | Frequency drift across the 110 s transmission, Hz/min, integer.                                                                                      |
| `freq`       | number  | Decoded carrier frequency in MHz to 6 decimals (e.g. `14.097123`).                                                                                  |
| `dial_MHz`   | number  | The receiver's WSPR dial frequency for this band, MHz to 6 decimals. `freq − dial_MHz` ≈ audio offset Hz/1e6 + 1500 Hz.                              |
| `dBm`        | integer | Reported transmit power in dBm (0…60, only WSPR-legal values).                                                                                       |
| `pwr`        | string  | Same value as a string (e.g. `"23"`). Kept for compatibility.                                                                                        |
| `utc`        | string  | UTC slot start as `"HHMM"`, zero-padded. Critical for U4B channel matching (see §4).                                                                 |
| `hour`       | integer | UTC hour of the slot, `0…23`.                                                                                                                        |
| `min`        | integer | UTC minute of the slot, `0…59`. WSPR slots are even minutes (`hh:00`, `hh:02`, …).                                                                   |

### Examples

Type 1 (regular ham):

```jsonc
{
  "timestamp":"1747037880","server":"abcd1234deadbeef",
  "call":"K1ABC","grid":"FN42","msg":"K1ABC FN42 30","type":1,
  "snr":-22.0,"dt":0.4,"drift":0,
  "freq":14.097123,"dial_MHz":14.0956,
  "dBm":30,"pwr":"30",
  "utc":"1138","hour":11,"min":38
}
```

Type 2 (compound callsign, no grid):

```jsonc
{
  "...":"...",
  "call":"PA0SKT/2","grid":"","msg":"PA0SKT/2 no_grid 33","type":2,
  "snr":-19.0,"dt":0.6,"drift":0,
  "freq":7.040123,"dial_MHz":7.0386,
  "dBm":33,"pwr":"33",
  "utc":"0820","hour":8,"min":20
}
```

Type 3 (hashed call, 6-char grid):

```jsonc
{
  "...":"...",
  "call":"K1ABC","grid":"JO22hf","msg":"<K1ABC> JO22hf 23","type":3,
  "snr":-25.0,"dt":0.4,"drift":0,
  "freq":14.097205,"dial_MHz":14.0956,
  "dBm":23,"pwr":"23",
  "utc":"1138","hour":11,"min":38
}
```

## 4. HAB / U4B sub-object

When a Type 1 spot's `call` + `grid` + `dBm` match the **QRP-Labs U4B / Traquito** telemetry-packet pattern, an additional **`hab`** object is appended. The firmware then tries to pair it with the regular WSPR packet decoded by the same receiver two minutes earlier.

This distinction matters: in a telemetry packet the apparent four-character WSPR `grid` is encoded sensor data, **not a locator**. A six-character position is emitted only after a unique preceding regular packet supplies the real first four locator characters.

### 4.1 Detection rule

A spot is treated as a U4B/Traquito candidate iff **all** of the following hold (`extensions/wspr/wspr_hab.cpp` → `wspr_hab_looks_like_u4b`):

| Test on Type 1 spot                                | Reason                                      |
|----------------------------------------------------|---------------------------------------------|
| `len(call) == 6` and `len(grid) == 4`              | U4B encodes everything into Type 1 fields.  |
| `call[0] ∈ {'0','1','Q'}`                          | Channel `id1`. ITU never assigns these.     |
| `call[2]` is `0–9`                                 | Channel `id3`.                              |
| `call[1]` is base-36 (`0–9`, `A–Z`)                | Holds part of the encoded big-number.       |
| `call[3..5]` are `A–Z`                             | U4B encoding alphabet (no spaces).          |
| `grid[0..1]` are `A–R`                             | Maidenhead field range.                     |
| `grid[2..3]` are `0–9`                             | Maidenhead square digits.                   |
| `dBm` is one of `{0,3,7,10,13,17,…,57,60}`         | WSPR-legal power; index in big-number.      |

Spots that don't pass all checks get **no** `"hab"` object.

Passing these checks establishes only that a packet has the U4B shape. It does not by itself prove that the packet belongs to a balloon or establish its position.

### 4.2 Pairing rule

A candidate is confirmed as paired only when the immediately preceding decoded slot contains one unambiguous regular Type 1 spot that:

- was transmitted exactly two minutes earlier, including across UTC midnight;
- was decoded by the same Web-888 WSPR receiver instance on the same band;
- is within 10 Hz of the telemetry carrier; and
- has a normal callsign and a four-character locator.

The closest frequency match is used. If two possible regular packets are within 0.5 Hz of the best match, the result is deliberately left unpaired. This conservative rule avoids manufacturing a plausible but incorrect position.

### 4.3 Channel identity

Two characters of the WSPR callsign are repurposed by U4B as channel identifiers:

- `id1` = `call[0]` ∈ `{0, 1, Q}` (3 values)
- `id3` = `call[2]` ∈ `0–9` (10 values)

Combined with the **slot minute** (`min mod 10` ∈ `{0,2,4,6,8}`) and the **frequency lane** (which 40 Hz slice of the 200 Hz WSPR sub-band the signal sits in — derivable from `freq − dial_MHz` only with calibrated receivers), this yields up to `3 × 10 × 5 × 4 = 600` "channels".

The packet pair establishes the transmitting callsign and locator, but it does not prove that the station is a balloon or resolve a public flight name. Those still require operator-published channel/flight metadata or an external tracker.

### 4.4 Variants

#### 4.4.1 `kind: "u4b_basic"`

The `telemetryId` bit decoded as `1`, and a unique regular packet was found. Decoded values and the paired position are present. For the observed sequence `G4BKC IO92 13` followed two minutes later by `QJ9FGP MB43 7`, the object is:

```jsonc
"hab": {
  "kind": "u4b_basic",
  "paired": true,
  "source_call": "G4BKC",
  "grid4": "IO92",
  "pair_delta_hz": 2.0,
  "ch":   { "id1": "Q", "id3": "9" },
  "raw":  { "c1":"Q","c2":"J","c3":"9","c4":"F","c5":"G","c6":"P",
            "g1":"M","g2":"B","g3":"4","g4":"3","p":2 },
  "grid56":"NE",
  "grid6": "IO92ne",
  "alt_m":  140,
  "temp_c": 11,
  "v":      4.95,
  "kn":     2,
  "gps_valid": true,
  "tlm_type":  "standard"
}
```

The outer/base `grid` remains `"MB43"`, preserving the WSPR packet exactly as received. Consumers must use `hab.grid6` for the paired position.

| Field        | Type    | Range / unit                                                            | Notes |
|--------------|---------|-------------------------------------------------------------------------|-------|
| `kind`       | string  | `"u4b_basic"`                                                            | Constant for this variant. |
| `paired`     | bool    | `true`                                                                    | The regular and telemetry packets were uniquely correlated. |
| `source_call`| string  | WSPR callsign                                                             | Callsign from the regular packet two minutes earlier. |
| `grid4`      | string  | 4-char Maidenhead locator                                                 | Real locator prefix from the regular packet. |
| `pair_delta_hz` | number | 0.0–10.0 Hz                                                            | Absolute carrier-frequency difference between the two packets. |
| `ch.id1`     | string  | one of `"0"`, `"1"`, `"Q"`                                               | First callsign char. |
| `ch.id3`     | string  | `"0"`…`"9"`                                                              | Third callsign char. |
| `raw.c1..c6` | string  | one ASCII char each                                                      | Original 6-char callsign. |
| `raw.g1..g4` | string  | one ASCII char each                                                      | Original 4-char grid. |
| `raw.p`      | integer | 0–18                                                                     | Index into `[0,3,7,10,13,17,…,60]` dBm list. |
| `grid56`     | string  | 2 chars `A`…`X`                                                          | The 5th and 6th Maidenhead chars decoded by U4B. |
| `grid6`      | string  | 6 chars                                                                  | Concatenation of paired `grid4` + decoded `grid56`, e.g. `"IO92ne"`. |
| `alt_m`      | integer | 0–21340 m (step 20)                                                      | Altitude. |
| `temp_c`     | integer | −50…+39 °C                                                               | Tracker-reported temperature. Traquito uses RP2040 internal sensor. |
| `v`          | number  | 3.00–4.95 V (step 0.05; rounded to 2 decimals)                           | Tracker input voltage during high-load TX. |
| `kn`         | integer | 0–82 kn (step 2)                                                         | Ground speed. |
| `gps_valid`  | bool    | `true`/`false`                                                           | Vendors may always report `true`; treat as a hint. |
| `tlm_type`   | string  | `"standard"`                                                             | Constant for basic telemetry. |

Voltage range note: the canonical Traquito implementation clamps the encoded voltage to `3.0–4.95 V`. The actual underlying U4B encoding rolls over every 1.95 V, so a tracker that does not implement Traquito's clamping (e.g. ones running on alkaline cells) may transmit 2.x V values that here appear as 4.0–4.95 V due to that rollover. We do **not** attempt to disambiguate — `v` is the value Traquito's reference decoder produces.

#### 4.4.2 `kind: "u4b_candidate_basic"`

The packet decodes as standard basic telemetry, but no unique preceding regular packet was heard. Sensor values are provided, but fields that would claim an identity or position are omitted:

```jsonc
"hab": {
  "kind": "u4b_candidate_basic",
  "paired": false,
  "ch": { "id1":"Q", "id3":"9" },
  "raw": { "c1":"Q","c2":"J","c3":"9","c4":"F","c5":"G","c6":"P",
           "g1":"M","g2":"B","g3":"4","g4":"3","p":2 },
  "grid56":"NE",
  "alt_m":140,"temp_c":11,"v":4.95,"kn":2,
  "gps_valid":true,"tlm_type":"standard"
}
```

There is intentionally no `source_call`, `grid4`, or `grid6`.

#### 4.4.3 `kind: "u4b_unknown"`

Spot matches the U4B *shape* but `telemetryId` decodes to `0`. This is reserved for **extended telemetry** / vendor-defined / user-defined formats; this firmware does not include their decoders. We emit only the raw bits so a consumer can apply the appropriate decoder:

```jsonc
"hab": {
  "kind": "u4b_unknown",
  "paired": false,
  "ch":   { "id1":"0", "id3":"4" },
  "raw":  { "c1":"0","c2":"K","c3":"4","c4":"X","c5":"Y","c6":"Z",
            "g1":"L","g2":"M","g3":"3","g4":"7","p":13 },
  "tlm_type": "extended"
}
```

No decoded `alt_m` etc.; `raw` + `ch` carry every bit that was on the air. When an extended packet is uniquely paired, `paired` is `true` and `source_call`, `grid4`, and `pair_delta_hz` are included, but no `grid6` is invented because this firmware does not understand that format.

#### 4.4.4 No `hab` object

Spot did not match the U4B shape, OR was Type 2/3, OR power/grid was outside U4B-legal ranges. The base WSPR fields (Section 3) are still complete — including `msg` and `type`, which is what a Type 3 hashed call looks like to a generic consumer.

## 5. Recommended consumer patterns

### 5.1 Build a flight track from a single Web-888

Use only `kind:"u4b_basic"` messages for a position track. These contain a receiver-local packet correlation and a defensible `grid6`. Treat `kind:"u4b_candidate_basic"` as unlocated telemetry.

You still **cannot** infer that the transmitter is airborne or assign a flight name from this stream alone. A ground test uses the same packet format, and flight identity is operator-published metadata.

Practical recipe:

- Track `u4b_basic` by `source_call` and `grid6`.
- Use an operator registry or external tracker to decide whether `source_call` represents an active balloon flight.
- Retain `pair_delta_hz`, `raw`, and the base packet fields for auditing.
- Log `u4b_candidate_basic` packets separately; do not plot their `grid56` as a position.

### 5.2 Republish into a HAB-aware topic

A small MQTT bridge can subscribe to `web888/+/WSPR` and:

- Drop messages without `hab`.
- Resolve the channel to a flight via your own table or an upstream API.
- Republish to `web888/+/HAB` with extra fields (`flight`, `operator`, `lat`, `lon` derived from `grid6`).

This is the simplest path to a useful balloon dashboard while keeping the firmware-side logic minimal.

### 5.3 Generic WSPR consumer

Just consume the base fields. The schema is back-compatible with the previous version, except that the base now includes additional fields (`msg`, `type`, `dial_MHz`, `dBm`, `utc`, `hour`, `min`). Old consumers that referenced only `call`, `grid`, `snr`, `dt`, `drift`, `freq`, `pwr` keep working unchanged.

## 6. Error handling and edge cases

- **Hash failures.** Type 3 spots whose hashed callsign is unknown to the running decoder appear internally as `call:"..."` and are not published.
- **Buffer truncation.** The WSPR body is built into a 1.5 KB stack buffer. If a future change makes the body longer than that, the publisher may emit a truncated JSON object. Validate JSON on the consumer side.
- **No retransmission.** QoS 0; if your broker is offline, messages are silently lost.
- **Spot deduplication.** The wsprd decoder already de-duplicates spots within a 3 Hz window per slot. Consumers seeing duplicates across multiple Web-888 receivers should dedupe by `(server, hour, min, call, grid, freq rounded to 2 Hz)` or similar.
- **Time skew.** The `utc` field uses the device's clock at sample time. GPS-disciplined clocks are accurate to ms; non-GPS or boot-time-only clocks may drift.

## 7. Implementation references

- Encoder/decoder spec: <https://traquito.github.io/pro/telemetry/basic/>
- QRP Labs U4B description: <https://qrp-labs.com/u4b/u4bdecoding.html>
- Reference C++ implementation (used to validate this firmware's decoder):
  <https://github.com/traquito/WsprEncoded> (`src/WsprMessageTelemetryBasic.h`)
- Channel map / frequency-lane definitions:
  <https://traquito.github.io/pro/telemetry/channels/>

## 8. Versioning

This MQTT schema is **additive**. Future changes will:

- Only add new fields, never rename or change types of fields documented in §3 and §4.
- Introduce new `hab.kind` values (e.g. `u4b_extended_gps`) by widening the enum, never repurpose `u4b_basic` / `u4b_unknown`.
- If a breaking change is ever needed, a new event topic (e.g. `web888/<id>/WSPR2`) will be introduced in parallel.
