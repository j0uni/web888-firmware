# Hosting Web-888 OTA firmware updates on your own server

The receiver’s **admin → software update** flow downloads files over **HTTPS** from a configurable **base URL** (`update_url_base` in admin config, default `https://downloads.rx-888.com/web-888`). The firmware appends a **channel** subdirectory (`stable` or `alpha`) and fixed filenames.

This document describes how your HTTP server must behave so updates succeed.

## URL layout

Let `BASE` be the configured value **without** a trailing slash (the firmware strips trailing `/` and spaces).

| Purpose | URL |
|--------|-----|
| Latest version (stable channel) | `BASE/stable/version.txt` |
| Latest version (alpha channel) | `BASE/alpha/version.txt` |
| Main binary | `BASE/{stable\|alpha}/websdr.bin` |
| SHA256 manifest | `BASE/{stable\|alpha}/checksum` |
| FPGA bitstreams (optional) | `BASE/{stable\|alpha}/websdr_hf.bit`, `websdr_vhf.bit` |

Example: if `update_url_base` is `https://jouni.kapsi.fi/web888`, then the stable version file must be reachable at:

`https://jouni.kapsi.fi/web888/stable/version.txt`

## `version.txt`

- **Method:** `GET`
- **Body:** plain text, two integers separated by a dot, e.g. `1.0.5`
- **Whitespace:** leading/trailing spaces are fine; the device uses `sscanf(..., "%d.%d", ...)`.
- **Semantics:** compared to the running firmware’s internal major/minor. If the remote pair is **greater**, an update is considered available (subject to admin “install updates” and scheduling).

## `checksum` file

After download, the device runs (conceptually):

```sh
cd /media/mmcblk0p1/update
sed '/web-888-alpine/d' checksum | sha256sum -c -
```

So your `checksum` file must be in **`sha256sum` standard format**:

```
<64 hex lowercase> *websdr.bin
<64 hex lowercase> *websdr_hf.bit
...
```

Rules:

1. **Lines whose text contains the substring `web-888-alpine` are removed** before verification. You can include extra artifacts (e.g. Alpine rootfs tarball) for other tooling; those lines are ignored by the Web-888 update step as long as they contain that marker (same behaviour as the vendor tree).
2. Every file you want **verified** must appear on a remaining line with the correct `sha256` and basename matching what was downloaded into `/media/mmcblk0p1/update/` (`websdr.bin`, and optionally the `.bit` files).
3. If `websdr_hf.bit` / `websdr_vhf.bit` downloads fail, the installer may skip replacing FPGA files; the `checksum` step must still succeed for the files that **were** downloaded (your manifest should match what you actually publish in that channel).

## Binaries and bitstreams

- **`websdr.bin`:** required for a full update path the UI expects.
- **`websdr_hf.bit` / `websdr_vhf.bit`:** optional; HTTP errors are rejected and each downloaded file must be at least 500 KiB. If either download fails validation, the code keeps both installed FPGA files and only replaces `websdr.bin` (see `net/update.cpp`).

All should be served with normal **`GET`**, **`200 OK`**, and a **correct `Content-Length`** (typical static file hosting).

## HTTPS / TLS

- Use a **valid certificate chain** for public hostnames (Let’s Encrypt on `jouni.kapsi.fi` is fine).
- The device uses **libcurl**; TLS 1.2+ and common cipher suites should work. Very old / exotic TLS settings may fail.

## Caching and CDNs

- Strong caching of `version.txt` or `checksum` can confuse operators after you publish a new build. Prefer short `Cache-Control` for those two, or cache-bust via filename (not supported by the client—so fix cache headers).
- `websdr.bin` can be large; **resume** is not required; the client uses a simple full download with a timeout (order of tens of seconds per file in code).

## Quick self-test from your PC

```sh
BASE='https://jouni.kapsi.fi/web888'
curl -fsS "$BASE/stable/version.txt"
curl -fsSI "$BASE/stable/websdr.bin" | head
curl -fsS "$BASE/stable/checksum" | head
```

Then verify checksums locally the same way the device does:

```sh
mkdir -p /tmp/w888-upd && cd /tmp/w888-upd
curl -fsS "$BASE/stable/websdr.bin" -o websdr.bin
curl -fsS "$BASE/stable/checksum" -o checksum
sed '/web-888-alpine/d' checksum | sha256sum -c -
```

## Configuring the Web-888

1. **Admin page → Software update** section.
2. Set **Update download base URL** to your `BASE` (e.g. `https://jouni.kapsi.fi/web888` — **no trailing slash**).
3. Choose **Stable** or **Alpha** to select which subdirectory is used.
4. Use **Check now** / **Install now** as before.

Existing installs pick up the default `https://downloads.rx-888.com/web-888` until the field is changed (or `dist.admin.json` / admin JSON already contains `update_url_base`).

## Mirroring the vendor channel

To stay aligned with upstream releases while serving from your own host:

1. Copy the vendor `stable/` (or `alpha/`) tree from `https://downloads.rx-888.com/web-888/` to your server under the same relative paths.
2. Re-publish your own `version.txt` / `checksum` / binaries whenever you want to promote a build (including your own `websdr.bin` from this repository’s CI or Docker build).

## Security note

Anyone who can change what your server serves controls what binary the radio installs. Protect upload credentials, use HTTPS, and consider serving updates only from a dedicated path with minimal write access.
