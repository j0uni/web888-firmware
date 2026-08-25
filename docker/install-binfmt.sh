#!/usr/bin/env sh
# One-time (per machine) setup on x86_64/amd64 hosts so linux/arm/v7 images run.
# Requires Docker with privileged mode once.
set -e
echo "Registering QEMU user handlers via tonistiigi/binfmt (needs --privileged)..."
docker run --rm --privileged tonistiigi/binfmt --install all
echo "Done. linux/arm/v7 containers should run on this host."
