#!/usr/bin/env sh
# Build websdr.bin using the Alpine armv7 Docker image (from repo root).
set -e
ROOT="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

IMAGE="${WEB888_BUILD_IMAGE:-web888-builder:alpine-armv7}"

docker build --platform linux/arm/v7 -f docker/Dockerfile -t "$IMAGE" "$ROOT"

exec docker run --rm \
	--platform linux/arm/v7 \
	-v "$ROOT:/workspace" \
	-w /workspace \
	-e BUILD_DIR="${BUILD_DIR:-}" \
	-e CLEAN_BUILD="${CLEAN_BUILD:-}" \
	"$IMAGE" \
	"$@"
