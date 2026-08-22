# QQCS — Qt Quick Camera Station

A native C++/Qt6/QML security-camera monitor for multiple RTSP cameras.
Primary target: Raspberry Pi 5 connected to a TV over HDMI, controlled via
HDMI-CEC. The same core application also runs on Linux desktop and macOS,
controlled with a keyboard and mouse, for development and testing.

This document is split in two parts: **Using QQCS** (configuring and
running an already-built copy — most readers want this) and **Building
from Source** (only needed if you're compiling it yourself or packaging
it for a new platform).

## Features

- Mosaic/grid view of every configured camera, always live, COVER-mode
  fill by default (no distortion, cropping allowed) — configurable, see
  *Configuration*.
- Fullscreen view of a focused camera, CONTAIN at 1.0× by default (no
  distortion, letterboxing allowed) — also configurable — zoom/pan at
  >1.0× (fills the viewport, no black bars, panable).
- Automatic fullscreen audio when a stream has a supported audio track.
- Always-on status overlay (camera name, LIVE/LOST, reconnect countdown).
- Toggleable diagnostics overlay (codec, resolution, FPS, bitrate,
  RTSP transport, dropped frames, reconnect state, ...).
- Automatic RTSP reconnect with exponential-then-flat backoff.
- HDMI-CEC remote control on Raspberry Pi; keyboard/mouse everywhere.
- `0`–`9` camera shortcuts from any view.

---

# Using QQCS

This part assumes you already have a `qqcs` binary — either built
yourself (see *Building from Source* below) or provided to you. It has
no build tooling in it at all.

## Configuration

qqcs reads a single YAML config file, resolved in this order (first
match wins):

1. `--config <path>` command-line argument
2. `$QQCS_CONFIG` environment variable
3. `/etc/qqcs/config.yaml`
4. `~/.config/qqcs/config.yaml`
5. `./config.yaml` (current working directory)

See [`config.example.yaml`](config.example.yaml) for a ready-to-copy
starting point.

```yaml
layout:
  columns: 4
  mosaicFillMode: cover
  fullscreenFillMode: contain

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

### Field reference

Every field's default is what applies when it (or its entire parent
section) is left out of the file entirely.

| Section | Field | Required? | Default | Effect |
|---|---|---|---|---|
| `layout` | — | No (whole section optional) | — | Mosaic grid layout and video rendering modes. |
| | `columns` | No | `4` | Number of mosaic grid columns; positive integer. Row count is derived automatically from the camera count. |
| | `mosaicFillMode` | No | `"cover"` | `"cover"`, `"contain"`, or `"fill"` — how each mosaic tile renders video whose aspect ratio doesn't match the tile's. `cover` crops, `contain` letterboxes; either way the image is never distorted. `fill` is the one exception: it stretches the image to the tile's exact bounds, not preserving aspect ratio — an explicit, opt-in trade-off, not the default. Any other value is a validation error. |
| | `fullscreenFillMode` | No | `"contain"` | Same three values and same distortion caveat for `fill`, but for the fullscreen view at `zoom = 1.0×` (zoom itself never adds further distortion on top of whichever mode is active). Any other value is a validation error. |
| `overlay` | — | No (whole section optional) | — | The always-on status overlay (camera name / LIVE-LOST / reconnect countdown) shown on every mosaic tile and in fullscreen. |
| | `enabled` | No | `true` | Master on/off switch for the overlay. |
| | `position` | No | `"bottom"` | `"top"` or `"bottom"` — vertical placement of the overlay within its tile/the fullscreen view. Any other value is a validation error. |
| | `showName` | No | `true` | Whether the camera's `name` is shown in the overlay. |
| | `showStatus` | No | `true` | Whether the LIVE/LOST/CONNECTING line (and, while LOST, the reconnect countdown) is shown. |
| `cameras` | — | **Yes** — must be a non-empty list | — | One entry per camera. |
| (each camera) | `id` | **Yes** | — | Unique string identifying the camera. Duplicates are a validation error. |
| | `name` | No | *(none)* | Human-readable name. If omitted, no name is shown in the overlay at all (not even a blank line). |
| | `shortcut` | No | *(none)* | Integer `0`–`9` for direct camera-select shortcuts (keyboard/CEC). Must be unique across cameras if set; out-of-range or duplicate values are a validation error. |
| | `mainUrl` | **Yes** | — | RTSP URL used for fullscreen (and fullscreen audio, if the stream has a supported track). |
| | `subUrl` | No | *(none — falls back to `mainUrl`)* | RTSP URL used for the mosaic tile instead of `mainUrl`. A lower-resolution substream here is strongly recommended once you have more than a couple of cameras — see *Raspberry Pi performance notes*. |

There is deliberately no `audio: true/false` field anywhere — fullscreen
audio is fully automatic whenever the stream actually has a supported
audio track (see *Diagnostics overlay* below for how that's determined).

At startup, every problem found in the file is validated and logged
together (not just the first one) — YAML syntax errors, missing
required fields, duplicate `id`/`shortcut`, an out-of-range `shortcut`,
a non-positive `layout.columns`, an invalid `overlay.position`, or an
invalid `layout.mosaicFillMode`/`layout.fullscreenFillMode`. A config
that fails validation stops the app with a clear error rather than
starting in a partially-broken state.

## Running on desktop (macOS / Linux)

```sh
./qqcs                                   # uses the resolution order above
./qqcs --config /path/to/config.yaml
QQCS_CONFIG=/path/to/config.yaml ./qqcs
```

The window opens maximized (filling the screen, but keeping the normal
title bar and window controls — not OS-level fullscreen) and starts in
mosaic view. No other setup is needed on desktop: HDMI-CEC simply isn't
present there, and the app runs identically without it.

## Controls

### Keyboard (desktop)

| Key | Action |
|---|---|
| Arrow keys | Mosaic: grid navigation (Up/Down = same column, staying at the bottom of a short column rather than jumping to a different one; Left/Right = previous/next camera in config order, not row-cyclic). Fullscreen at 1.0×: Left/Right switch camera. Fullscreen zoomed in: all four pan. |
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
`NavigationController`'s constructor in the source if you need to change
the ceiling). At zoom > 1.0×, the video always fills the fullscreen
viewport with no black bars, regardless of the camera's native aspect
ratio — arrow keys/CEC directions pan instead of switching cameras.
Resetting to 1.0× (Escape) also resets pan to center; arrow keys/CEC
directions immediately go back to switching cameras.

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

## Deploying to a Raspberry Pi

This section builds `qqcs` from source directly on the Pi. If you'd
rather skip building entirely — including every future update — see
*Installing a prebuilt package* further down, which covers the same
`/etc/qqcs`/systemd setup via a `.deb` instead.

### Hardware setup

- Raspberry Pi 5 board, with its official 27 W USB-C power supply (Pi 5
  needs more power than most older USB chargers/cables can reliably
  deliver).
- microSD card (16 GB+) or a USB/NVMe SSD, for the OS.
- An HDMI cable from one of the Pi's two micro-HDMI ports to the TV,
  connected to a TV input that has CEC enabled (TVs vary in which
  HDMI port(s) support it and what they call the feature — see
  *Troubleshooting*).
- A network connection: Ethernet is recommended for reliability;
  Wi-Fi can be configured during OS imaging instead.
- Optional: a USB keyboard/mouse, only useful for initial setup or
  desktop-style debugging directly on the Pi — normal operation is
  driven entirely by the TV remote over CEC.

### OS installation

1. Flash a current, actively-maintained 64-bit Linux distribution onto
   your storage using Raspberry Pi's own imaging tool ("Raspberry Pi
   Imager"). No specific distribution is required (see SPEC §4) —
   **Ubuntu 26.04** (Server or Desktop) is what this project's CI
   actually builds and tests against (see *Building for Raspberry Pi*
   below for the simpler steps that choice implies), and
   **Raspberry Pi OS (64-bit, Bookworm-based)** is the other option
   documented there. Either way, the imaging tool's advanced options
   let you enable SSH and set a username/password/Wi-Fi/hostname up
   front, for a fully headless setup with no monitor/keyboard ever
   needed on the Pi itself.
2. Insert the storage, connect HDMI + network + power, and boot.
3. Connect over SSH (`ssh <username>@<hostname>.local`, or by IP
   address) or use a directly-attached keyboard, and run through the
   rest of this section from there. `.local` (mDNS) resolution comes
   preinstalled on Raspberry Pi OS, but **not** on Ubuntu Server — if
   `<hostname>.local` doesn't resolve from your other machine, either
   find the Pi's IP address another way (e.g. your router's client
   list) for this first connection, then
   `sudo apt install avahi-daemon libnss-mdns` on the Pi itself so
   `.local` works for subsequent connections too.

### Software setup

If you're deploying a binary built elsewhere (see *Cross-compiling from
a Linux host* below) rather than building directly on the Pi, you still
need the runtime libraries present. The exact runtime-only (non-`-dev`)
package names/version suffixes vary by OS release, so the simplest
reliable option is to install the same `-dev` packages listed under
*Building for Raspberry Pi* below — `-dev` packages depend on their
runtime libraries too, so this always works correctly, just with some
unused headers taking a little extra disk space (this applies whether
you're on Ubuntu 26.04 or Raspberry Pi OS — the package names below are
the same either way; only whether Qt itself also needs to come from
`aqtinstall` differs, per that section):

```sh
sudo apt update
sudo apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base \
    gstreamer1.0-plugins-good gstreamer1.0-plugins-bad \
    gstreamer1.0-plugins-ugly gstreamer1.0-libav \
    qt6-base-dev qt6-declarative-dev qml6-module-qtquick qt6-tools-dev \
    libcec-dev libp8-platform-dev libyaml-cpp-dev
```

If you're building directly on the Pi instead, installing the full
dependency list under *Building for Raspberry Pi* covers this step too
— there's nothing extra to do.

Ensure `dtoverlay=vc4-kms-v3d` is set in `/boot/firmware/config.txt`
(this is the default on Raspberry Pi OS Bookworm+, so usually nothing
to change).

### Installing the application

However you obtained the `qqcs` binary (built directly on the Pi, or
cross-compiled and copied over — see *Building from Source*):

```sh
sudo cmake --install build --prefix /usr   # see *Installing* below; puts it at /usr/bin/qqcs
sudo mkdir -p /etc/qqcs
```

Copy your `config.yaml` onto the Pi — e.g. from your development
machine: `scp config.yaml <user>@<pi-host>:/tmp/config.yaml`, then on
the Pi: `sudo mv /tmp/config.yaml /etc/qqcs/config.yaml`. Alternatively,
edit it directly on the Pi with `sudo nano /etc/qqcs/config.yaml` (or
your editor of choice). See *Configuration* above for the file's format.

Copy and adjust the KMS output configuration:

```sh
sudo mkdir -p /etc/qqcs
sudo cp deploy/raspberrypi/kms.json.example /etc/qqcs/kms.json
```

Then edit `/etc/qqcs/kms.json`'s `device`/`outputs.name` fields to match
your Pi's actual DRM output — check with `sudo apt install libdrm-tests`
then `sudo modetest -M vc4` (both the package and `sudo` are required;
neither is preinstalled/permitted by default), since the device path
and connector name vary by hardware/kernel revision; this repository's
defaults are a starting point, not guaranteed values.

### Manual test run (before systemd)

Before wiring `qqcs` into systemd, it's worth starting it by hand once,
from the Pi's own console (not over SSH — it needs to own the DRM
master, which an already-running graphical session/other DRM client
would hold instead):

```sh
sudo QT_QPA_PLATFORM=eglfs QT_QPA_EGLFS_KMS_CONFIG=/etc/qqcs/kms.json QT_QPA_EGLFS_HIDECURSOR=1 /usr/bin/qqcs --config=/etc/qqcs/config.yaml
```

The environment variables must come **after** `sudo` on the command
line, not before (`export`ing them first and then plain `sudo qqcs`
loses them, since `sudo` resets the environment by default).
`QT_QPA_EGLFS_HIDECURSOR` hides the mouse cursor that `eglfs` otherwise
always composites on screen even with no mouse ever attached — drop it
temporarily if you're debugging with a physical mouse plugged in.

### systemd autostart

Two unit variants are provided in [`deploy/raspberrypi/`](deploy/raspberrypi/):

**`qqcs.service`** (recommended) — runs via `eglfs`/KMS, rendering
directly to `/dev/dri` with no X11/Wayland compositor needed. This is
the lowest-overhead, most kiosk-appropriate setup for an always-on
single-app device.

```sh
sudo useradd --system --groups video,render,input qqcs || \
    sudo usermod -aG video,render,input qqcs   # if the qqcs user already exists (e.g. a previous setup attempt)
sudo cp deploy/raspberrypi/qqcs.service /etc/systemd/system/
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

**Some buttons work over CEC, but the number buttons (0–9) don't.**
Confirmed on real hardware to be a TV/remote-side limitation, not a bug
in qqcs: many TVs reserve number buttons for their own channel-input UI
and never put the press on the CEC bus at all, regardless of client
software — nothing on the qqcs side can work around this. Digit
shortcuts still work fine from a keyboard.

**Black screen on the Pi via HDMI, app appears to be running.** Check
`/etc/qqcs/kms.json`'s `device`/`outputs.name` actually match your
hardware (`modetest -M vc4` lists real connector names); a mismatch here
is the most common cause of `eglfs` producing no visible output despite
the process running normally.

**Video looks stretched/squashed.** With the default `cover`/`contain`
fill modes (see *Configuration*), this should not happen — mosaic tiles
crop and fullscreen letterboxes at 1.0× (or fills without cropping-
required distortion when zoomed), never stretches. If you *did*
explicitly set `mosaicFillMode`/`fullscreenFillMode` to `fill`, this is
expected — that mode trades aspect-ratio correctness for filling the
tile/screen exactly, on purpose. Otherwise, it's a bug — please report
it with the camera's native resolution, the configured fill mode, and
the window/tile size at the time.

---

# Installing a prebuilt package

If you don't want to build from source at all — and in particular, if
you don't want to wait for a from-scratch compile on a Raspberry Pi's
own CPU every time — a tagged push to this repository's `main` branch
triggers `release.yml`, which builds and publishes `.deb` packages to
this repository's [Releases page](https://github.com/akosthekiss/qqcs/releases),
built and tested on the exact same Ubuntu 26.04 (amd64/arm64) targets
described throughout this document. Once installed this way, updating
to a new version never requires a rebuild on the target machine again
— just `apt install` the next release's `.deb`.

Two packages come out of each release, matching the two deployment
scenarios described above:

**`qqcs_<version>_<arch>.deb`** — just the binary, for desktop Linux:

```sh
sudo apt install ./qqcs_<version>_amd64.deb   # or _arm64.deb
```

Bring your own `config.yaml` wherever you like and start it by hand —
see *Running on desktop*, above.

**`qqcs-raspberrypi_<version>_arm64.deb`** — for Raspberry Pi appliance
deployment. It depends on (and pulls in) the plain `qqcs` package
above, and additionally installs the systemd unit and `/etc/qqcs`
config templates described under *Deploying to a Raspberry Pi*:

```sh
sudo apt install ./qqcs-raspberrypi_<version>_arm64.deb
```

Installing it prints the exact next steps. The service is deliberately
**not** started automatically — there's no valid default camera list to
start it with — but it's safe to `systemctl enable` right away even
before configuring, since the unit's `ConditionPathExists=` inertly
skips startup (not a crash loop) until `/etc/qqcs/config.yaml` exists:

```
1. sudo cp /etc/qqcs/config.yaml.example /etc/qqcs/config.yaml
   sudo nano /etc/qqcs/config.yaml            # add your real cameras
2. check /etc/qqcs/kms.json against `sudo modetest -M vc4`'s output
   (the shipped default usually already matches a Raspberry Pi 5)
3. sudo systemctl enable --now qqcs
```

Only Ubuntu 26.04 (amd64/arm64) is covered by these packages, matching
this project's one CI/support baseline throughout this document —
**Raspberry Pi OS still needs building from source** (see *Building
for Raspberry Pi* below): a `.deb` built against Ubuntu's own Qt6/
GStreamer shared libraries won't install cleanly against a different
distribution's package set.

**Not yet exercised end-to-end on real hardware**: this packaging
pipeline itself (the CPack/`dpkg-shlibdeps`-generated `Depends:`, the
postinst user/group creation, the `ConditionPathExists=`-gated startup)
has only been validated by CI's build step so far, not by an actual
`apt install` of a real `.deb` on a real Raspberry Pi — treat the first
real install as the true test of this section, not this description.

# Building from Source

## Supported platforms and versions

| Platform | Versions | Verified |
|---|---|---|
| macOS | Whatever your installed Qt 6.8+ supports as a minimum (Qt 6.8 itself requires macOS 12 Monterey or later) | Yes — local development (arm64, against a real 7-camera RTSP setup) and CI (macos-15, macos-26, both arm64: build, ctest, functional RTSP smoke test) |
| Linux desktop | Ubuntu 26.04, either architecture — the only Linux version this project actually tests anywhere; other distributions/versions providing Qt 6.8+ and a reasonably current GStreamer 1.x will likely also work (nothing exotic is used), but aren't verified, so aren't listed as supported | Yes — CI (ubuntu-26.04, amd64 and arm64: build, ctest, functional RTSP smoke test) |
| Raspberry Pi 5 | No specific distribution is required (see SPEC §4) — either Ubuntu 26.04 (arm64) or Raspberry Pi OS 64-bit (Bookworm-based); see *Building for Raspberry Pi* for both | Partially — CI verifies the same OS/architecture/CEC-enabled build+test+smoke-test path as the Ubuntu 26.04 option, on a generic arm64 runner rather than real Raspberry Pi hardware. Real hardware and Raspberry Pi OS specifically remain unverified anywhere, locally or in CI — see *Known limitations* |

## Dependencies

| Component | Purpose | macOS (Homebrew) | Debian/Ubuntu (apt) |
|---|---|---|---|
| C/C++ toolchain | Build | Xcode Command Line Tools (`xcode-select --install`; already required by Homebrew itself) | `apt install build-essential` — present by default on GitHub's CI runner images and on Raspberry Pi OS, but **not** on a stock Ubuntu Server image |
| CMake ≥ 3.21 | Build | `brew install cmake` | `apt install cmake` |
| Ninja | Build | `brew install ninja` | `apt install ninja-build` |
| chrpath | Build (RPATH fixup used by Qt6's CMake tooling) | not needed | `apt install chrpath` — same "present on CI/Raspberry Pi OS, not on stock Ubuntu Server" caveat as above; without it, `cmake --build` fails with an RPATH-related Ninja error |
| Qt 6.8+ (Core, Quick, Qml, Test) | GUI | `brew install qt` | `apt install qt6-base-dev qt6-declarative-dev qml6-module-qtquick qt6-tools-dev` on Ubuntu 26.04 (ships 6.10.2). Older Debian/Ubuntu releases and Raspberry Pi OS Bookworm ship older Qt6, below this project's 6.8+ requirement; use the [Qt online installer](https://www.qt.io/download-qt-installer-oss) or [`aqtinstall`](https://github.com/miurahr/aqtinstall) there instead |
| GStreamer 1.x (core + app + video + audio + sdp + good/bad/ugly/libav plugins) | RTSP/video/audio | `brew install gstreamer` | `apt install libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev gstreamer1.0-plugins-base gstreamer1.0-plugins-good gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav` |
| yaml-cpp | Config parsing | `brew install yaml-cpp` | `apt install libyaml-cpp-dev` |
| libCEC | HDMI-CEC (Raspberry Pi only) | `brew install libcec` (only if you want to build/test the real adapter on desktop) | `apt install libcec-dev libp8-platform-dev` |
| pkg-config | Build | `brew install pkg-config` | `apt install pkg-config` |

yaml-cpp is fetched automatically via CMake `FetchContent` if not found on
the system, so it's the only dependency that's technically optional to
pre-install.

## Building for macOS

```sh
brew install cmake ninja qt gstreamer yaml-cpp pkg-config
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=$(brew --prefix qt)
cmake --build build
```

The resulting binary is `build/qqcs`; the two manual smoke-test tools
(see *Testing*) land at
`build/tools/smoke_single_stream/smoke_single_stream` and
`build/tools/smoke_multi_stream/smoke_multi_stream`.

## Building for Linux desktop

This is only tested against **Ubuntu 26.04**, whose own Qt6 apt
packages (6.10.2) already clear this project's 6.8+ requirement, so
everything comes straight from apt:

```sh
sudo apt update
sudo apt install build-essential chrpath cmake ninja-build \
    qt6-base-dev qt6-declarative-dev qml6-module-qtquick qt6-tools-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav libyaml-cpp-dev pkg-config

cmake -S . -B build -G Ninja
cmake --build build
```

`build-essential`/`chrpath` are already present on GitHub's CI runner
images, which is why `ci.yml` doesn't list them explicitly — a stock
Ubuntu Server image (e.g. freshly flashed onto a Raspberry Pi) has
neither, so they're spelled out here.

On an older Debian/Ubuntu release, or any other distribution whose own
Qt6 packages fall short of 6.8+, get Qt from
[`aqtinstall`](https://github.com/miurahr/aqtinstall) (a thin wrapper
around Qt's own official prebuilt packages) or the
[Qt online installer](https://www.qt.io/download-qt-installer-oss)
instead — untested here, but the same mechanism *Building for
Raspberry Pi* below uses and describes in more detail:

```sh
sudo apt install build-essential chrpath cmake ninja-build libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav libyaml-cpp-dev pkg-config python3-pip

pip install --user aqtinstall
python3 -m aqt install-qt linux desktop 6.8.1 linux_gcc_64 -O ~/Qt

cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=~/Qt/6.8.1/gcc_64
cmake --build build
```

HDMI-CEC is off by default on non-ARM Linux; add `-DQQCS_ENABLE_CEC=ON`
if you've also installed `libcec-dev libp8-platform-dev` and want to
build/test the real adapter (see the CMake option note under *Building
for Raspberry Pi* below).

## Building for Raspberry Pi

### Directly on the Pi (recommended, simplest)

This is by far the most reliable approach — it needs no cross-compiler,
sysroot, or multiarch setup, run on the Pi itself. Which exact steps
you need depends on which OS you flashed:

**On Ubuntu 26.04** — the OS/architecture this project's CI actually
builds and tests against (on a generic arm64 runner, not real
Raspberry Pi hardware): its own Qt6 apt packages (6.10.2) already
clear this project's 6.8+ requirement, so this is a single step,
identical in shape to the Linux desktop build above:

```sh
sudo apt update
sudo apt install build-essential chrpath cmake ninja-build \
    qt6-base-dev qt6-declarative-dev qml6-module-qtquick qt6-tools-dev \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav libyaml-cpp-dev libcec-dev libp8-platform-dev pkg-config

cmake -S . -B build -G Ninja -DQQCS_ENABLE_CEC=ON
cmake --build build
```

`build-essential`/`chrpath` are present on GitHub's CI runner images
(which is why `ci.yml` doesn't list them) and on Raspberry Pi OS, but
**not** on a stock Ubuntu Server image — confirmed by actually flashing
and building on real Raspberry Pi 5 hardware, where their absence
first showed up as "no C compiler found" and then a Ninja RPATH error
respectively.

**On Raspberry Pi OS** (Bookworm-based) — the other documented option,
not independently verified anywhere, locally or in CI: its own Qt6 apt
packages are just as far below 6.8 as Debian/Ubuntu 22.04/24.04's are,
so Qt needs to come from elsewhere. `aqtinstall` has an arm64-native
package set (host `linux_arm64`) for exactly this case — official
prebuilt Qt binaries that run directly on the Pi's own aarch64 OS, no
compiling Qt from source:

```sh
sudo apt update
sudo apt install build-essential chrpath cmake ninja-build libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev \
    gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
    gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly \
    gstreamer1.0-libav libyaml-cpp-dev libcec-dev libp8-platform-dev pkg-config python3-pip

pip install --user aqtinstall
python3 -m aqt install-qt linux_arm64 desktop 6.8.1 linux_gcc_arm64 -O ~/Qt

cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=~/Qt/6.8.1/gcc_arm64 -DQQCS_ENABLE_CEC=ON
cmake --build build
```

Either way, HDMI-CEC support (`QQCS_ENABLE_CEC`) defaults to **on**
here regardless of the explicit flag above, since CMake detects the
aarch64/Linux combination automatically. The trade-off of building
directly on the Pi either way is build time: compiling on the Pi
itself, rather than on a faster machine, is noticeably slower — expect
it to take a while (Qt itself doesn't need compiling in either case
above, only qqcs's own, much smaller, source tree).

### Cross-compiling from a Linux host (advanced, faster)

If you have a faster x86_64 (or arm64) Linux machine, you can
cross-compile instead of building on-device. **This exact recipe was
not run or verified in this project's own development** — no ARM64
cross-toolchain or target sysroot was available in that environment —
but it follows the standard, well-documented approach for cross-building
against a Debian-family target (which Raspberry Pi OS is), using Debian's
own multiarch support rather than a hand-maintained sysroot. Treat it as
a starting point to debug against your actual toolchain/package versions.

On an x86_64 Debian/Ubuntu build host:

```sh
# 1. Add the target architecture and a matching cross-compiler.
sudo dpkg --add-architecture arm64
sudo apt update
sudo apt install crossbuild-essential-arm64

# 2. Add arm64 apt sources alongside your existing (host-architecture)
#    ones -- e.g. in /etc/apt/sources.list.d/arm64.list, pointing at
#    Raspberry Pi OS's or Debian's own arm64 archive, and mark your
#    existing entries [arch=amd64] so apt doesn't try to fetch amd64
#    packages from an arm64-only mirror or vice versa.
sudo apt update

# 3. Install target (arm64) *runtime and dev* packages alongside your
#    host's own (amd64) tools -- multiarch keeps them in separate paths
#    (/usr/lib/aarch64-linux-gnu/ vs /usr/lib/x86_64-linux-gnu/), so
#    nothing conflicts. Qt is deliberately not in this list -- see
#    below.
sudo apt install \
    libgstreamer1.0-dev:arm64 libgstreamer-plugins-base1.0-dev:arm64 \
    gstreamer1.0-plugins-base:arm64 \
    gstreamer1.0-plugins-good:arm64 gstreamer1.0-plugins-bad:arm64 \
    gstreamer1.0-plugins-ugly:arm64 gstreamer1.0-libav:arm64 \
    libyaml-cpp-dev:arm64 libcec-dev:arm64 libp8-platform-dev:arm64

# 4. Get an arm64 Qt build via aqtinstall instead of apt (Raspberry Pi
#    OS's own Qt6 packages fall well short of this project's 6.8+
#    requirement -- see *Building for Linux desktop* above for the
#    same issue on desktop Linux). aqtinstall's `linux_arm64` host is
#    meant for installing directly on arm64 hardware, but since it's
#    only downloading and unpacking prebuilt files, nothing stops
#    fetching those same files on an x86_64 host to use as a
#    cross-compile target -- confirmed by actually running this command
#    and inspecting its output (a plain `<dir>/6.8.1/gcc_arm64` tree
#    with a working `lib/cmake/Qt6/Qt6Config.cmake` in it), though the
#    *combination* with the cross-compiler above was not verified
#    end-to-end: Qt's official arm64 build is built against a specific
#    Ubuntu/glibc version, which may or may not exactly match whatever
#    `crossbuild-essential-arm64` provides on your build host -- if
#    linking fails with undefined/version-mismatched symbols, that ABI
#    gap is the first thing to check.
pip install --user aqtinstall
python3 -m aqt install-qt linux_arm64 desktop 6.8.1 linux_gcc_arm64 -O ~/Qt
```

Write a CMake toolchain file, e.g. `arm64-toolchain.cmake`:

```cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_C_COMPILER aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)
set(CMAKE_FIND_ROOT_PATH /usr/lib/aarch64-linux-gnu)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(ENV{PKG_CONFIG_LIBDIR} /usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig)
set(ENV{PKG_CONFIG_SYSROOT_DIR} /)
```

Configure and build with it:

```sh
cmake -S . -B build-rpi -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$PWD/arm64-toolchain.cmake \
    -DCMAKE_PREFIX_PATH="$HOME/Qt/6.8.1/gcc_arm64;/usr/lib/aarch64-linux-gnu/cmake" \
    -DQQCS_ENABLE_CEC=ON
cmake --build build-rpi
```

(`CMAKE_PREFIX_PATH` here lists two directories, semicolon-separated:
the aqtinstall Qt tree for `find_package(Qt6 ...)`, and the multiarch
apt tree for `find_package(yaml-cpp ...)`; GStreamer is found via
`pkg-config`, using the toolchain file's `PKG_CONFIG_LIBDIR` instead.)

Copy the resulting `build-rpi/qqcs` to the Pi (e.g.
`scp build-rpi/qqcs <user>@<pi-host>:/tmp/`) and install it there — it
needs the arm64 **runtime** libraries present on the Pi regardless of
where it was compiled, which the *Software setup* step under *Deploying
to a Raspberry Pi* above already installs.

## Installing

```sh
cmake --install build                       # installs to the default prefix (e.g. /usr/local)
cmake --install build --prefix /usr          # or a specific prefix
```

This copies the built binary to `<prefix>/bin/qqcs`. On Raspberry Pi,
`sudo cmake --install build --prefix /usr` puts it at `/usr/bin/qqcs`,
matching the path the provided systemd units (`ExecStart=/usr/bin/qqcs`)
expect. There's nothing else to install — QML files, the video-rendering
pipeline, and everything else needed at runtime are compiled directly
into the one executable.

## Testing

### Automated unit tests

```sh
ctest --test-dir build --output-on-failure
```

Runs 7 independent Qt Test binaries — config validation, navigation
math/state machine, reconnect scheduling, keyboard mapping, CEC mapping,
video fill math, and camera manager structure — all fast and hermetic;
none needs a real camera, network access, or GUI.

### Manual smoke tests against a real camera

Two additional executables are built (not part of `ctest`, since they
need a live RTSP source):

| Tool | Purpose | Required environment variables |
|---|---|---|
| `build/tools/smoke_single_stream/smoke_single_stream` | Opens one RTSP stream in a window; prints pipeline state transitions and a heartbeat, to confirm the GUI thread never blocks. | `QQCS_SMOKE_URL="rtsp://user:pass@host/path"` |
| `build/tools/smoke_multi_stream/smoke_multi_stream` | Runs a mosaic (sub-stream) and fullscreen (main-stream) pipeline for one camera concurrently, side by side. | `QQCS_SMOKE_MAIN_URL`, `QQCS_SMOKE_SUB_URL` (both required) |

Both also honor `QQCS_SMOKE_SCREENSHOT=<path>`, saving a frame a few
seconds in via an in-process `QQuickWindow::grabWindow()` — this works
over SSH or without a real display server showing the window, and
without needing OS screen-recording permissions.

### Development/debug hooks in the main application

The following environment variables are compiled into every `qqcs`
build (there is no separate "debug build"). They exist because this
project was developed and verified without reliable OS-level GUI
automation available (no Accessibility permission for synthetic
keystrokes, no display server to screenshot directly, etc.); they let
specific interactions be triggered deterministically for a screenshot or
a log trace, instead of needing a person at a keyboard/mouse. None of
them has any effect unless set, and they are not meant for end-user
use — only set these if you're debugging the application itself.

| Variable | Effect |
|---|---|
| `QQCS_DEBUG_CLICK_TILE=<index>` | ~1s after startup, simulates clicking mosaic tile `<index>` (0-based, in config order). |
| `QQCS_DEBUG_ZOOM_STEPS=<n>` | ~1.5s after startup, simulates `<n>` wheel-zoom-in steps in fullscreen. |
| `QQCS_DEBUG_ZOOM_CURSOR=<x>,<y>` | Cursor position (viewport-local, logical pixels) used by `QQCS_DEBUG_ZOOM_STEPS`; defaults to the viewport center if unset. |
| `QQCS_DEBUG_PAN_DELTA=<dx>,<dy>` | ~2.5s after startup, simulates a single pan-drag delta of `(dx, dy)` in fullscreen. |
| `QQCS_DEBUG_INJECT_KEY=<key>[,<key>...]` | ~2.5s after startup, injects one or more real `QKeyEvent`s (comma-separated `Qt::Key` integer codes, e.g. `16777236` for Right) directly into the main window — the same Qt Quick event-delivery path OS keyboard input uses, exercising the real `Keys.onPressed` → `InputManager` → `NavigationController` chain. Multiple keys are sent in order, each fully processed before the next is sent. |
| `QQCS_DEBUG_SCREENSHOT=<path>` | ~4s after startup, saves a frame of the main window to `<path>` via `QQuickWindow::grabWindow()` (same rationale as the smoke tools' equivalent variable). |
| `QQCS_DEBUG_QUIT_AFTER_MS=<ms>` | Quits the application after `<ms>` milliseconds, so it exits on its own and flushes buffered output instead of needing to be killed (important whenever stdout/stderr is redirected to a file). |

Example, combining several of these to verify a mosaic→fullscreen→zoom
sequence non-interactively against a real config:

```sh
QQCS_DEBUG_CLICK_TILE=0 QQCS_DEBUG_ZOOM_STEPS=2 \
QQCS_DEBUG_SCREENSHOT=/tmp/qqcs_frame.png QQCS_DEBUG_QUIT_AFTER_MS=6000 \
./build/qqcs
```

## Known limitations of this first version

- Real Raspberry Pi 5 hardware, a physical HDMI-CEC remote, and
  Raspberry Pi OS's apt-packaged dependencies remain unavailable to
  verify against — CI builds and tests the arm64+CEC-enabled code path
  (Ubuntu 26.04, arm64), but on a generic cloud runner, not real
  Raspberry Pi hardware, and not Raspberry Pi OS specifically. The real
  libCEC adapter has only ever been exercised against Homebrew's libcec
  on macOS and against a plain arm64 Linux runner in CI, confirming the
  graceful-no-adapter-found path in both cases, but never against
  actual CEC hardware. The cross-compilation recipe above is likewise
  unverified end-to-end.
- Linux desktop is now built and tested in CI (Ubuntu 26.04, amd64 and
  arm64: build, ctest, functional RTSP smoke test), but not against a
  real physical machine or a real multi-camera RTSP setup the way
  macOS has been.
- Per SPEC §37, intentionally out of scope for this version: a
  camera-configuration GUI, PTZ control, recording/playback, motion
  detection, AI/object detection, cloud integration, a mobile app, a web
  frontend, remote administration, and authentication.

## Copyright and Licensing

Licensed under the [BSD 3-Clause License](LICENSE.md).
