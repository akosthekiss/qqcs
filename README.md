# QQCS — Qt Quick Camera Station

A native C++/Qt6/QML security-camera monitor for multiple RTSP cameras.
Primary target: Raspberry Pi 5 connected to a TV over HDMI, controlled via
HDMI-CEC. The same core application also runs on Linux desktop and macOS,
controlled with a keyboard and mouse, for development and testing.

## Features

- Mosaic/grid view of every configured camera, always live, COVER-mode
  fill (no distortion, cropping allowed).
- Fullscreen view of a focused camera, CONTAIN at 1.0× (no distortion,
  letterboxing allowed), zoom/pan at >1.0× (fills the viewport, no black
  bars, panable).
- Automatic fullscreen audio when a stream has a supported audio track.
- Always-on status overlay (camera name, LIVE/LOST, reconnect countdown).
- Toggleable diagnostics overlay (codec, resolution, FPS, bitrate,
  RTSP transport, dropped frames, reconnect state, ...).
- Automatic RTSP reconnect with exponential-then-flat backoff.
- HDMI-CEC remote control on Raspberry Pi; keyboard/mouse everywhere.
- `0`–`9` camera shortcuts from any view.

## Dependencies

| Component | Purpose | macOS (Homebrew) | Debian/Raspberry Pi OS (apt) |
|---|---|---|---|
| CMake ≥ 3.21 | Build | `brew install cmake` | `apt install cmake` |
| Ninja | Build | `brew install ninja` | `apt install ninja-build` |
| Qt 6.5+ (Core, Quick, Qml, Test) | GUI | `brew install qt` | `apt install qt6-base-dev qt6-declarative-dev qml6-module-qtquick qt6-tools-dev` |
| GStreamer 1.x (core + app + video + audio + sdp + good/bad/ugly/libav plugins) | RTSP/video/audio | `brew install gstreamer` | `apt install libgstreamer1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav` |
| yaml-cpp | Config parsing | `brew install yaml-cpp` | `apt install libyaml-cpp-dev` |
| libCEC | HDMI-CEC (Raspberry Pi only) | `brew install libcec` (only if you want to build/test the real adapter on desktop) | `apt install libcec-dev` |
| pkg-config | Build | `brew install pkg-config` | `apt install pkg-config` |

yaml-cpp is fetched automatically via CMake `FetchContent` if not found on
the system, so it's the only dependency that's technically optional to
pre-install.

## Building

```sh
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
```

On macOS with Homebrew Qt (which isn't on the default CMake search path),
add `-DCMAKE_PREFIX_PATH=$(brew --prefix qt)`.

HDMI-CEC support is compiled in by default only on aarch64/arm Linux
(i.e. Raspberry Pi). It's a normal CMake option, overridable either way:

```sh
cmake -S . -B build -G Ninja -DQQCS_ENABLE_CEC=ON   # force on, e.g. to test on desktop Linux with libcec installed
cmake -S . -B build -G Ninja -DQQCS_ENABLE_CEC=OFF  # force off, e.g. on a Pi you don't want CEC on
```

The build also produces two manual verification tools (not part of
`ctest`, since they need a live RTSP camera):
`build/tools/smoke_single_stream/smoke_single_stream` and
`build/tools/smoke_multi_stream/smoke_multi_stream`. Run with
`QQCS_SMOKE_URL="rtsp://..."` (single) or `QQCS_SMOKE_MAIN_URL=... QQCS_SMOKE_SUB_URL=...`
(multi) set.

## Configuration

qqcs reads a YAML config file, resolved in this order:

1. `--config <path>` command-line argument
2. `$QQCS_CONFIG` environment variable
3. `/etc/qqcs/config.yaml`
4. `~/.config/qqcs/config.yaml`
5. `./config.yaml` (current working directory)

See [`config.example.yaml`](config.example.yaml) for a starting point.

```yaml
layout:
  columns: 4

overlay:
  enabled: true
  position: bottom
  showName: true
  showStatus: true

cameras:
  - id: front
    name: "Front door"
    shortcut: 1
    mainUrl: "rtsp://192.168.1.101:554/main"
    subUrl: "rtsp://192.168.1.101:554/sub"
```

### Adding a camera

Add an entry under `cameras:`:

- `id` — required, unique string.
- `name` — optional; if omitted, no name is shown in the overlay.
- `shortcut` — optional integer `0`–`9`; must be unique across cameras.
- `mainUrl` — required RTSP URL, used for fullscreen (and its audio).
- `subUrl` — optional RTSP URL; if present, used for the mosaic tile
  instead of `mainUrl` (lower resolution recommended, for less CPU load
  with many cameras at once). If absent, the mosaic tile also uses
  `mainUrl`.

There is deliberately no `audio: true/false` field — fullscreen audio is
automatic whenever the stream has a supported track (see below).

Startup validates: YAML syntax, required fields, unique `id`s, unique
`shortcut`s in range 0–9, `layout.columns` a positive integer, and
`overlay.position` one of `top`/`bottom`. All problems found are logged
together (not just the first one); a config that fails validation stops
the app with a clear error rather than starting in a broken state.

## Running

```sh
./build/qqcs                                   # uses the resolution order above
./build/qqcs --config /path/to/config.yaml
QQCS_CONFIG=/path/to/config.yaml ./build/qqcs
```

## Controls

### Keyboard (desktop)

| Key | Action |
|---|---|
| Arrow keys | Mosaic: grid navigation (Up/Down = same column; Left/Right = previous/next camera in config order, not row-cyclic). Fullscreen at 1.0×: Left/Right switch camera. Fullscreen zoomed in: all four pan. |
| Enter | Select focused mosaic tile → fullscreen |
| Escape | Zoomed in: reset zoom to 1.0× and pan to center. At 1.0× in fullscreen: back to mosaic. |
| `0`–`9` | Jump straight to that camera's fullscreen, from any view or zoom state. `0` is always a camera shortcut, never a zoom reset. |
| `+` / `-` (also `=` for `+`) | Zoom in/out, centered on the viewport |
| `I` | Toggle the diagnostics overlay |

### Mouse (desktop)

| Input | Action |
|---|---|
| Left click on a mosaic tile | Enter fullscreen on that camera |
| Scroll wheel (in fullscreen) | Zoom in/out, centered on the cursor ("zoom to cursor") |
| Left-button drag (in fullscreen, zoomed in) | Pan |

### HDMI-CEC (Raspberry Pi)

| Remote button | Action |
|---|---|
| Arrow keys / OK / Back | Same as keyboard equivalents |
| Red | Zoom in (centered on the video) |
| Green | Zoom out |
| Blue | Toggle diagnostics |
| `0`–`9` | Camera shortcuts |
| Yellow | Reserved, no current function |

CEC's absence — no adapter hardware, or a build without libCEC at all —
never blocks the app from running; it's a compile-time-swapped adapter
behind one interface, with a no-op stub used whenever the real one isn't
applicable.

## Zoom and pan

Zoom steps by default: 1.0×, 1.5×, 2.0×, 3.0×, 4.0× (see
`NavigationController`'s constructor if you need to change the ceiling).
At zoom > 1.0×, the video always fills the fullscreen viewport with no
black bars, regardless of the camera's native aspect ratio — arrow
keys/CEC directions pan instead of switching cameras. Resetting to 1.0×
(Escape) also resets pan to center; arrow keys/CEC directions immediately
go back to switching cameras.

## Diagnostics overlay

Toggle with `I` (keyboard) or the Blue CEC button. Shows, for the current
fullscreen camera: video codec, resolution, FPS, bitrate, audio codec,
RTSP transport, dropped frames, reconnect count/backoff/countdown, and
the RTSP URL (with any embedded username/password masked, since this
overlay can end up on a TV screen).

Latency is intentionally **not** one of the displayed fields: true
glass-to-glass latency needs RTCP-based NTP correlation that isn't
reliable across consumer IP cameras, and the spec explicitly allows
omitting a field that can't be measured reliably rather than showing a
value that would always read `N/A`. This paragraph is that field's
documentation, per the spec's own allowance for exactly this case.

## Raspberry Pi installation

1. Flash Raspberry Pi OS 64-bit, boot, connect to network.
2. Install dependencies (see the apt column above) plus `libcec-dev`.
3. Build qqcs (see *Building* above) and install the binary, e.g. to
   `/usr/bin/qqcs`.
4. Create a config at `/etc/qqcs/config.yaml` (see *Configuration*).
5. Copy [`deploy/raspberrypi/kms.json.example`](deploy/raspberrypi/kms.json.example)
   to `/etc/qqcs/kms.json` and adjust the `device`/`outputs.name` fields
   to match your Pi's actual DRM output (check with `modetest -M vc4` —
   the device path and connector name vary by hardware/kernel revision
   and need confirming on your actual unit; this repository's defaults
   are a starting point, not guaranteed values).
6. Ensure `dtoverlay=vc4-kms-v3d` is set in `/boot/firmware/config.txt`
   (default on Raspberry Pi OS Bookworm+).
7. Install the systemd unit (see below) and enable it.

### systemd autostart

Two unit variants are provided in [`deploy/raspberrypi/`](deploy/raspberrypi/):

**`qqcs.service`** (recommended) — runs via `eglfs`/KMS, rendering
directly to `/dev/dri` with no X11/Wayland compositor needed. This is
the lowest-overhead, most kiosk-appropriate setup for an always-on
single-app device.

```sh
sudo useradd --system --groups video,render,input qqcs   # if not already present
sudo cp deploy/raspberrypi/qqcs.service /etc/systemd/system/
sudo cp deploy/raspberrypi/kms.json.example /etc/qqcs/kms.json   # then edit it
sudo systemctl daemon-reload
sudo systemctl enable --now qqcs.service
journalctl -u qqcs -f   # watch logs
```

**`qqcs-wayland.service`** — an alternative for keeping the stock
Raspberry Pi OS desktop (labwc/wayfire) instead of a bare KMS kiosk.
This is a **user** unit, requires desktop autologin, and needs enabling
from inside the graphical session:

```sh
mkdir -p ~/.config/systemd/user
cp deploy/raspberrypi/qqcs-wayland.service ~/.config/systemd/user/
systemctl --user daemon-reload
systemctl --user enable --now qqcs-wayland.service
```

Either way: the QPA platform is chosen via an `Environment=` line in the
unit file, not compiled into the binary — the same executable still
auto-detects the right platform (`cocoa` on macOS, `xcb`/`wayland` on a
desktop Linux session) when run without that override.

Both units restart on failure and log via `journalctl`; neither is
started from `.bashrc` or a desktop autostart entry.

## Raspberry Pi performance notes

Raspberry Pi 5 has **no dedicated hardware video-decode block**
(Broadcom removed it compared to Pi 4), so video decode there is
software, via `avdec_h264`/`avdec_h265`. The application is designed
around this:

- Mosaic tiles use each camera's `subUrl` (lower-resolution substream)
  when configured — this is the single biggest lever for CPU load with
  many cameras, so configuring a substream is strongly recommended.
- Only the currently-fullscreen camera ever runs a full-resolution
  decode; switching cameras tears down the old one first.
- Each decoder's own thread pool is capped (`max-threads`), so several
  concurrent mosaic decodes don't all fight over every CPU core.
- Where available, video is rendered through a GL-integrated Qt Quick
  sink (GPU-side colorspace conversion); otherwise it falls back to a
  portable `appsink`-based path. Which one is active is decided at
  runtime by probing for the `qml6glsink` GStreamer element, not
  hardcoded per platform.
- RTSP uses `protocols=tcp` for reliability over typical home networks.

**Not verified from the environment this was built in**: actual
4/9/16-camera CPU/GPU/RAM/temperature measurements on real Raspberry Pi
5 hardware. The design above is reasoned from Pi 5's documented hardware
characteristics, not measured — treat it as a starting point and profile
on your actual device (`vcgencmd measure_temp`, `vcgencmd measure_clock arm`, `top`) before relying on a specific camera count.

## Troubleshooting

**App exits immediately / config errors in the log.** Check the exact
messages — every validation problem is logged, not just the first one.
Common causes: duplicate `id`/`shortcut`, a `shortcut` outside 0–9, or a
`cameras:` list that's empty or missing `mainUrl`/`id` on an entry.

**A camera tile shows LOST and never recovers.** Reconnects follow a
1s/2s/5s/10s/30s-then-30s-repeating backoff — give it a few minutes.
Check the RTSP URL is reachable from the qqcs machine (`ffprobe` or
`gst-launch-1.0 rtspsrc location=... ! fakesink` are good manual checks),
and that the camera accepts TCP transport (qqcs always requests it).

**No audio in fullscreen even though the camera has a microphone.**
Only PCMA/PCMU/AAC/MPEG4-GENERIC/OPUS RTP payloads are recognized as
decodable; check the diagnostics overlay's "Audio codec" field — if it
shows a codec name but audio still doesn't play, check
`journalctl`/stderr for an "audio playback error", which is isolated
from video and won't otherwise crash anything.

**HDMI-CEC doesn't respond to the TV remote.** Confirm the build was
compiled with `QQCS_ENABLE_CEC=ON` (default on Pi) and that
`journalctl`/stderr shows "HDMI-CEC adapter connected: ..." at startup,
not "No HDMI-CEC adapter found". If no adapter is found, check the CEC
line is enabled on the TV (often called "Anynet+"/"Bravia Sync"/
"SimpLink"/etc. depending on TV brand) and that the Pi's HDMI cable
supports CEC (some cheap cables don't wire the CEC pin through).

**Black screen on the Pi via HDMI, app appears to be running.** Check
`/etc/qqcs/kms.json`'s `device`/`outputs.name` actually match your
hardware (`modetest -M vc4` lists real connector names); a mismatch here
is the most common cause of `eglfs` producing no visible output despite
the process running normally.

**Video looks stretched/squashed.** This should not happen — mosaic
tiles crop (COVER) and fullscreen letterboxes (CONTAIN at 1.0×) or fills
without cropping-required distortion (zoomed), never stretches. If you
see stretching, it's a bug — please report it with the camera's native
resolution and the window/tile size at the time.

## Known limitations of this first version

- Real Raspberry Pi 5 hardware, a physical HDMI-CEC remote, and
  Raspberry Pi OS's apt-packaged dependencies were not available to
  verify against during development — only the underlying libraries and
  logic could be checked (e.g. the real libCEC adapter was compiled and
  exercised against Homebrew's libcec on macOS, confirming the
  graceful-no-adapter-found path, but not against actual CEC hardware).
- Per SPEC §37, intentionally out of scope for this version: a
  camera-configuration GUI, PTZ control, recording/playback, motion
  detection, AI/object detection, cloud integration, a mobile app, a web
  frontend, remote administration, and authentication.
