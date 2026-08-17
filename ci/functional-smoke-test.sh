#!/usr/bin/env bash
# Copyright (c) 2026 Akos Kiss.
#
# Licensed under the BSD 3-Clause License
# <LICENSE.md or https://opensource.org/licenses/BSD-3-Clause>.
# This file may not be copied, modified, or distributed except
# according to those terms.
#
# Exercises the real RTSP -> GStreamer -> Qt Quick path end to end,
# headlessly: starts a local RTSP server (mediamtx) and publishes a
# synthetic test video+audio stream to it (ffmpeg), points a temporary
# qqcs config at that stream, runs qqcs with its built-in debug hooks
# (see README.md's "Development/debug hooks" section) to click into
# fullscreen and grab a screenshot, and checks that a real frame was
# actually decoded and rendered -- not just that the binary starts.
#
# Used by .github/workflows/ci.yml on both macOS and Linux; not part of
# `ctest` since it needs network sockets and external tools (ffmpeg,
# mediamtx), not just the qqcs binary itself.

set -euo pipefail

BUILD_DIR=${1:-build}
ARTIFACT_DIR=${QQCS_CI_ARTIFACT_DIR:-/tmp/qqcs-ci}
mkdir -p "$ARTIFACT_DIR"

WORKDIR=$(mktemp -d)
MEDIAMTX_PID=""
FFMPEG_PID=""
cleanup() {
    [ -n "$MEDIAMTX_PID" ] && kill "$MEDIAMTX_PID" 2>/dev/null || true
    [ -n "$FFMPEG_PID" ] && kill "$FFMPEG_PID" 2>/dev/null || true
    rm -rf "$WORKDIR"
}
trap cleanup EXIT

case "$(uname -s)" in
    Darwin) PLATFORM=darwin ;;
    Linux)  PLATFORM=linux ;;
    *) echo "Unsupported platform for this smoke test: $(uname -s)" >&2; exit 1 ;;
esac
case "$(uname -m)" in
    x86_64|amd64)   ARCH=amd64 ;;
    arm64|aarch64)  ARCH=arm64 ;;
    *) echo "Unsupported architecture for this smoke test: $(uname -m)" >&2; exit 1 ;;
esac

# A plain, possibly-empty array under `set -u` isn't portable to bash
# 3.2 (macOS's default /bin/bash), which errors on "${arr[@]}" even when
# the array is declared-but-empty; a small wrapper function sidesteps
# that entirely.
gh_curl() {
    if [ -n "${GITHUB_TOKEN:-}" ]; then
        curl -fsSL -H "Authorization: Bearer ${GITHUB_TOKEN}" "$@"
    else
        curl -fsSL "$@"
    fi
}

echo "==> Fetching latest mediamtx release for ${PLATFORM}_${ARCH}"
MEDIAMTX_TAG=$(gh_curl https://api.github.com/repos/bluenviron/mediamtx/releases/latest | jq -r .tag_name)
MEDIAMTX_ASSET="mediamtx_${MEDIAMTX_TAG}_${PLATFORM}_${ARCH}.tar.gz"
gh_curl "https://github.com/bluenviron/mediamtx/releases/download/${MEDIAMTX_TAG}/${MEDIAMTX_ASSET}" -o "$WORKDIR/mediamtx.tar.gz"
tar -xzf "$WORKDIR/mediamtx.tar.gz" -C "$WORKDIR"

echo "==> Starting mediamtx (local RTSP server on 127.0.0.1:8554)"
(cd "$WORKDIR" && ./mediamtx > "$WORKDIR/mediamtx.log" 2>&1) &
MEDIAMTX_PID=$!
sleep 2

echo "==> Publishing a synthetic H.264+AAC test stream via ffmpeg"
# -g/-keyint_min force a keyframe every 10 frames (0.4s @ 25fps), so a
# client that connects mid-stream (qqcs's fullscreen pipeline, started
# by the debug hook below well after this ffmpeg process) does not have
# to wait for a distant GOP boundary before it can decode anything.
ffmpeg -nostdin -loglevel warning -re \
    -f lavfi -i "testsrc=size=640x480:rate=25" \
    -f lavfi -i "sine=frequency=1000" \
    -c:v libx264 -preset ultrafast -tune zerolatency -pix_fmt yuv420p -g 10 -keyint_min 10 \
    -c:a aac \
    -f rtsp -rtsp_transport tcp "rtsp://127.0.0.1:8554/qqcs-ci" \
    > "$WORKDIR/ffmpeg.log" 2>&1 &
FFMPEG_PID=$!
sleep 2

echo "==> Writing a temporary single-camera config pointing at the local stream"
cat > "$WORKDIR/config.yaml" <<EOF
layout:
  columns: 1
overlay:
  enabled: true
cameras:
  - id: ci
    name: "CI test pattern"
    mainUrl: "rtsp://127.0.0.1:8554/qqcs-ci"
    subUrl: "rtsp://127.0.0.1:8554/qqcs-ci"
EOF

SCREENSHOT="$ARTIFACT_DIR/screenshot.png"
echo "==> Running qqcs against the local stream (offscreen QPA, software Quick backend)"
QT_QPA_PLATFORM=offscreen \
QT_QUICK_BACKEND=software \
QQCS_CONFIG="$WORKDIR/config.yaml" \
QQCS_DEBUG_CLICK_TILE=0 \
QQCS_DEBUG_SCREENSHOT="$SCREENSHOT" \
QQCS_DEBUG_QUIT_AFTER_MS=8000 \
"$BUILD_DIR/qqcs" > "$WORKDIR/qqcs.log" 2>&1 || true

echo "----- qqcs log -----"
cat "$WORKDIR/qqcs.log"
echo "----- ffmpeg log (tail) -----"
tail -n 20 "$WORKDIR/ffmpeg.log" || true
echo "----- mediamtx log (tail) -----"
tail -n 20 "$WORKDIR/mediamtx.log" || true
echo "------------------------------"

if [ ! -f "$SCREENSHOT" ]; then
    echo "FAIL: no screenshot was produced -- qqcs likely crashed or never rendered a frame." >&2
    exit 1
fi

SIZE=$(stat -f%z "$SCREENSHOT" 2>/dev/null || stat -c%s "$SCREENSHOT")
echo "Screenshot size: ${SIZE} bytes"

# A blank/failed render (solid black, or Qt Quick's default background)
# compresses to a PNG of only a few hundred bytes; a real decoded video
# frame with visible test-pattern detail does not. This is a heuristic,
# not a pixel-accurate check, but it reliably tells "something was
# actually decoded and rendered" apart from "the window came up empty".
MIN_SIZE=20000
if [ "$SIZE" -lt "$MIN_SIZE" ]; then
    echo "FAIL: screenshot is only ${SIZE} bytes (< ${MIN_SIZE}), suspiciously small for a real video frame." >&2
    exit 1
fi

echo "OK: qqcs connected to the local RTSP stream, decoded, and rendered a frame."
