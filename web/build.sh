#!/usr/bin/env bash
# Build BlockOut II for the browser in a throwaway Emscripten container. Output in build/web/.
#   web/build.sh            build (only what changed)
#   web/build.sh clean      start over
set -euo pipefail
cd "$(dirname "${BASH_SOURCE[0]}")/.."
EMSDK=${EMSDK_VERSION:-3.1.74}
mkdir -p build/emcache
docker run --rm --platform linux/amd64 -u "$(id -u):$(id -g)" -v "$PWD:/src" -v "$PWD/build/emcache:/emcache" \
  -e EM_CACHE=/emcache -w /src "emscripten/emsdk:$EMSDK" make -f web/Makefile -j"${JOBS:-6}" "$@"
