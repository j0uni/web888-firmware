#!/bin/sh
# Run inside the builder image with the repo mounted at /workspace.
set -e

BUILD_TYPE="${1:-Release}"
OUTDIR="${BUILD_DIR:-build-docker}"

cd /workspace

if [ ! -f CMakeLists.txt ]; then
	echo "error: mount this repository at /workspace (use docker/build.sh from the repo root)" >&2
	exit 1
fi

if [ -d .git ]; then
	git config --global --add safe.directory /workspace
	if [ -f .gitmodules ]; then
		git submodule update --init --recursive
	fi
fi

if [ "${CLEAN_BUILD:-0}" = 1 ]; then
	rm -rf "$OUTDIR"
fi

mkdir -p "$OUTDIR"
cd "$OUTDIR"
cmake .. -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
cmake --build . -j"$(nproc)"

ls -lh websdr.bin
file websdr.bin
