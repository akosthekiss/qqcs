# QQCS

## 1. Project Purpose

Build a native, primarily Raspberry Pi-targeted security camera monitoring application capable of displaying multiple RTSP video streams on a TV connected via HDMI, controllable via the TV's remote control over HDMI-CEC.

The application must run from the same core codebase on desktop Linux and macOS as well.

On desktop:

* keyboard;
* mouse

must be usable for control.

On Raspberry Pi:

* the GUI must appear over HDMI;
* it must handle the TV's remote control via HDMI-CEC;
* it must start automatically.

---

## 2. Main Features

The application:

* is capable of displaying multiple RTSP cameras simultaneously;
* provides a mosaic/grid view;
* handles a focusable camera;
* displays the selected camera in fullscreen;
* automatically plays the audio stream in fullscreen, if present;
* allows switching to the previous/next camera in fullscreen;
* provides digital zoom;
* provides pan functionality on the zoomed image;
* displays the camera name and LIVE/LOST status;
* gives a text-based indication of audio presence in fullscreen view (not in mosaic, since there is no audio playback there);
* displays the time remaining until the next reconnect for a lost stream;
* provides an optional diagnostics overlay;
* provides `0–9` quick camera selection;
* is controllable via HDMI-CEC remote on Raspberry Pi;
* is controllable via keyboard and mouse on desktop;
* starts automatically on Raspberry Pi.

---

# 3. Technology Requirements

Preferred technology:

| Area          | Technology             |
| ------------- | ----------------------- |
| Main language | C++17 or C++20          |
| GUI           | Qt 6 / Qt Quick / QML   |
| Build         | CMake                   |
| Video/RTSP    | GStreamer               |
| HDMI-CEC      | libCEC                  |
| Configuration | YAML                    |

The main application must be C++.

Python may only be used for auxiliary scripts.

Do not use an Electron/webview-based GUI.

---

# 4. Platforms

Primary:

* Raspberry Pi 5;
* Raspberry Pi OS 64-bit;
* HDMI TV;
* HDMI-CEC.

Secondary:

* Linux desktop;
* macOS.

On desktop, CEC and Raspberry Pi-specific autostart are not requirements.

On startup, the desktop window should be maximized (filling the available screen space), but not in OS-level fullscreen mode — the title bar, application name, and window control buttons should remain visible.

---

# 5. Architecture

Suggested layers:

```text
Input
  │
  ▼
InputManager
  │
  ▼
NavigationController
  │
  ├───────────────┐
  ▼               ▼
Mosaic          Fullscreen
  │               │
  └───────┬───────┘
          ▼
    CameraManager
          │
          ▼
   GStreamer pipelines
          │
          ▼
       RTSP cameras
```

Input sources:

```text
CEC
Keyboard
Mouse
   │
   ▼
InputAction
   │
   ▼
NavigationController
```

The GUI must not handle CEC or the GStreamer pipelines directly.

---

# 6. Configuration

The configuration format is **YAML**.

Example:

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
    name: "Front Door"
    shortcut: 1
    mainUrl: "rtsp://192.168.1.101:554/main"
    subUrl: "rtsp://192.168.1.101:554/sub"

  - id: garden
    name: "Garden"
    shortcut: 2
    mainUrl: "rtsp://192.168.1.102:554/main"
    subUrl: "rtsp://192.168.1.102:554/sub"
```

There is **no audio enable/disable field** in the configuration.

Audio is automatic in fullscreen.

---

## 6.1 Camera Fields

### `id`

Required, unique identifier.

### `name`

Optional, human-readable name.

If not provided, no name overlay should be shown.

### `shortcut`

Optional integer between `0`–`9`.

Used for quick camera selection.

Shortcuts must be unique.

### `mainUrl`

Required RTSP stream URL.

Primarily used for fullscreen.

### `subUrl`

Optional RTSP substream.

If it exists, it should be preferred in mosaic view.

---

# 7. Configuration Validation

On startup, the following must be checked:

* YAML syntax;
* required fields;
* uniqueness of camera IDs;
* uniqueness of shortcuts;
* shortcut value `0–9`;
* layout values;
* overlay position.

In case of invalid configuration:

* a clear error must be logged;
* there must be no silent failure;
* where possible, the GUI should also show a usable error state.

---

# 8. Mosaic View

On startup, the default view is the mosaic/grid.

The layout configures the number of columns:

```yaml
layout:
  columns: 4
```

The number of rows must be determined automatically.

For example, 10 cameras and 4 columns:

```text
┌──────────┬──────────┬──────────┬──────────┐
│ Camera 1 │ Camera 2 │ Camera 3 │ Camera 4 │
├──────────┼──────────┼──────────┼──────────┤
│ Camera 5 │ Camera 6 │ Camera 7 │ Camera 8 │
├──────────┼──────────┼──────────┼──────────┤
│ Camera 9 │ Camera10 │          │          │
└──────────┴──────────┴──────────┴──────────┘
```

Non-existent cells cannot be focused.

---

# 9. Mosaic Aspect Ratio Handling

The video image must **never be distorted**.

The rendering mode in mosaic:

```text
COVER
```

This means:

* the original aspect ratio is preserved;
* the tile's full area is filled;
* the image is cropped if necessary;
* no black bars;
* no stretch/distortion.

The crop should be centered by default.

---

# 10. Mosaic Navigation

The focused tile must be marked with a clearly visible frame.

```text
Up   : row above
Down : row below
Left : previous camera
Right: next camera
```

`Left` and `Right` are **not cyclic navigation within a row**.

The camera's logical order is the order in the YAML configuration:

```text
1  2  3  4
5  6  7  8
9 10 11 12
```

Therefore:

```text
4 → Right → 5
5 → Left  → 4

8 → Right → 9
9 → Left  → 8
```

On the first camera, `Left` jumps to the last camera; on the last camera, `Right` jumps to the first.

`Up` and `Down` should move within the same column where possible.

A missing cell cannot be focused; in that case, the focus stays at the bottom of the current column, and does not jump to a different column, even if a camera exists there.

---

# 11. Fullscreen

The focused camera can be selected into fullscreen view:

```text
CEC OK
Enter
left mouse button
```

In fullscreen:

* it uses the entire available screen;
* the video's aspect ratio is unchanged;
* at `zoom = 1.0×`, the entire video image is visible;
* black letterbox/pillarbox bars are allowed where necessary.

Fullscreen 1.0×:

```text
CONTAIN
```

That is, the video does not need to fill the entire screen if that would require distortion or cropping.

---

# 12. Fullscreen Camera Switching

In fullscreen, at `zoom = 1.0×`:

```text
Left : previous camera
Right: next camera
```

The order is the order in the YAML configuration.

The list is cyclic:

```text
first, Left → last
last, Right → first
```

---

# 13. Fullscreen Audio

In fullscreen, the current camera's audio stream must be played automatically, provided that:

* the RTSP stream contains a supported audio stream;
* the audio is decodable and playable.

There is no `audio: true/false` configuration.

If there is no audio, the video should still work.

On camera switch:

1. the previous audio stream should stop;
2. the new camera's audio stream should start, if available.

An audio failure must not stop the video.

---

# 14. Zoom

Supported zoom steps, for example:

```text
1.0×
1.5×
2.0×
3.0×
4.0×
```

The maximum zoom should be easily adjustable.

Zoom must not distort the image.

In fullscreen, at `zoom > 1.0×`:

* the enlarged video should fill the viewport;
* black bars should not be necessary;
* off-screen parts should be panable;
* full visibility of the video image is no longer a requirement.

---

# 15. CEC Zoom

```text
RED    : Zoom+
GREEN  : Zoom−
BLUE   : Diagnostics on/off
YELLOW : reserved
```

For CEC, the zoom origin is the center of the image.

Repeatedly pressing Zoom− should allow returning to the `1.0×` state.

---

# 16. Desktop Zoom

On desktop:

```text
mouse wheel up   : Zoom+
mouse wheel down : Zoom−
```

The zoom center is the current position of the mouse cursor.

The video pixel under the mouse should, as much as possible, remain in the same screen position while zooming.

This should be an intuitive "zoom to cursor" behavior.

---

# 17. Zoom Reset

On desktop:

```text
Escape : Zoom reset
```

Reset:

```text
zoom = 1.0×
pan = (0, 0)
```

If `zoom > 1.0×`, Escape performs a zoom reset.

If `zoom` is already `1.0×`, in fullscreen Escape exits fullscreen.

`0` is **never** a zoom reset.

---

# 18. Zoom and Direction Keys

This is mandatory state-dependent behavior.

### `zoom == 1.0×`

Fullscreen:

```text
Left : previous camera
Right: next camera
```

### `zoom > 1.0×`

Fullscreen:

```text
Up   : pan up
Down : pan down
Left : pan left
Right: pan right
```

If zoom returns to `1.0×`:

```text
zoom = 1.0×
pan = (0, 0)
```

After that, Left/Right switch cameras again.

---

# 19. Desktop Pan

In fullscreen, at `zoom > 1.0×`:

```text
left mouse button + drag : pan
```

Panning should be constrained so that it is not possible to navigate outside the video's valid range.

---

# 20. Always-On Status Overlay

The always-on status overlay may appear:

* in mosaic view;
* in fullscreen view.

Content (in both mosaic and fullscreen):

1. camera name, if present;
2. LIVE/LOST status;
3. while LOST, the time remaining until the next reconnect.

In fullscreen view (including zoomed), additionally:

4. a text-based indication of audio presence (e.g. "(WITH AUDIO)" / "(NO AUDIO)"). This does not appear in mosaic view.

Examples:

```text
● LIVE  FRONT DOOR              (mosaic)
```

```text
● LIVE (WITH AUDIO)  FRONT DOOR  (fullscreen)
```

```text
● LIVE (NO AUDIO)  GARDEN        (fullscreen)
```

```text
● LOST  FRONT DOOR
Reconnect: 4s
```

The status overlay should be easy to read from a TV as well.

---

## 20.1 Audio Status

The audio indicator is text-based (not an emoji icon), and indicates whether the current stream contains a supported audio stream:

```text
(WITH AUDIO) : supported audio stream present
(NO AUDIO)   : no supported audio stream
```

This indicator only appears in fullscreen (including zoomed fullscreen) view; it does not appear in mosaic view.

This does **not** mean the current audio output state.

If there is an audio stream but a playback error occurs, this should be shown by the detailed diagnostics overlay.

---

## 20.2 Reconnect Countdown

In `LOST` state, the always-on status overlay must show:

```text
Reconnect: Ns
```

For example:

```text
LOST
Reconnect: 5s
```

then:

```text
LOST
Reconnect: 4s
```

etc.

The countdown:

* must update every second;
* must come from the actual reconnect scheduler;
* must not be computed from a separate, independent UI timer.

At the moment of reconnecting:

```text
LOST
Reconnect: 0s
```

need not be shown.

The state should instead switch to, for example:

```text
CONNECTING
```

---

# 21. Camera State

Minimum:

```cpp
enum class CameraState {
    Disconnected,
    Connecting,
    Live,
    Lost
};
```

Meaning:

* `Disconnected`: no active connection;
* `Connecting`: a connection attempt is in progress;
* `Live`: video is actually arriving and decodable;
* `Lost`: a previously working stream has been lost, awaiting reconnect.

---

# 22. Diagnostics

The Blue CEC button toggles the diagnostics overlay.

On desktop:

```text
I : diagnostics on/off
```

Possible information:

* camera name;
* camera state;
* audio presence;
* video codec;
* resolution;
* FPS;
* bitrate;
* audio codec;
* RTSP transport;
* dropped frames;
* reconnect count;
* reconnect backoff;
* time remaining until the next reconnect;
* RTSP URL.

If unavailable:

```text
N/A
```

Latency is intentionally not included in the diagnostics overlay: measuring true glass-to-glass latency would require RTCP-based NTP correlation, which is not reliable for most IP cameras. Rather than always showing `N/A` for this field, it is omitted from the overlay entirely, and this decision only needs to be documented (e.g. in the README).

The diagnostics overlay is an overlay, not a separate window.

The reconnect countdown is **not exclusively diagnostic information**: in the LOST state, it is also mandatory on the always-on status overlay.

---

# 23. Quick Camera Selection

The `0–9` buttons can be used for direct camera selection.

Must work:

* on the CEC remote;
* on the desktop keyboard;
* in mosaic view;
* in fullscreen view;
* in zoomed fullscreen view.

For example:

```yaml
shortcut: 5
```

results in:

```text
5 → Camera 5
```

Recommended:

```text
Mosaic + 5      → Camera 5 fullscreen
Fullscreen + 5  → Camera 5 fullscreen
Zoom + 5        → Camera 5 fullscreen
```

If an undefined shortcut is pressed, nothing should happen.

`0` is always a camera shortcut.

---

# 24. Input Abstraction

Suggested:

```cpp
enum class InputAction {
    Up,
    Down,
    Left,
    Right,

    Select,
    Back,

    Camera0,
    Camera1,
    Camera2,
    Camera3,
    Camera4,
    Camera5,
    Camera6,
    Camera7,
    Camera8,
    Camera9,

    ZoomIn,
    ZoomOut,
    ResetZoom,

    ToggleDiagnostics
};
```

The NavigationController decides the meaning of the action based on the current state.

---

# 25. CEC Input Mapping

```text
Up       : Up
Down     : Down
Left     : Left
Right    : Right
OK       : Select
Back     : Back

Red      : ZoomIn
Green    : ZoomOut
Blue     : ToggleDiagnostics

0–9      : Camera0–Camera9

Yellow   : reserved
```

The absence of CEC must not cause the application to fail to run.

---

# 26. Desktop Keyboard Input Mapping

```text
Arrow keys : Up / Down / Left / Right
Enter      : Select
Escape     : ResetZoom / Back

0–9        : Camera0–Camera9

+          : ZoomIn
-          : ZoomOut
I          : ToggleDiagnostics
```

Escape:

* at `zoom > 1.0×`, zoom reset;
* at `zoom == 1.0×`, exit from fullscreen.

---

# 27. Desktop Mouse

```text
left click         : Select
mouse wheel        : ZoomIn / ZoomOut
left button + drag : Pan, if zoom > 1.0×
```

---

# 28. CEC

On Raspberry Pi, libCEC must be used.

Supported:

* Up;
* Down;
* Left;
* Right;
* Select;
* Back;
* Red;
* Green;
* Blue;
* 0–9.

Yellow reserved.

---

# 29. Raspberry Pi Autostart

On Raspberry Pi, the application must start automatically.

Preferred:

**systemd service**

Requirements:

* start after the graphical environment;
* restart after a crash;
* must be loggable;
* must not start from `.bashrc`.

---

# 30. Performance

Raspberry Pi 5 is the primary target.

Preferred:

* hardware decoding;
* GPU rendering;
* minimal CPU frame-copy;
* substream in mosaic;
* main stream in fullscreen;
* non-blocking GUI thread.

Examine at least:

```text
4 cameras
9 cameras
16 cameras
```

Measure:

* CPU;
* GPU;
* RAM;
* FPS;
* dropped frames;
* latency;
* temperature.

There should be no arbitrarily predetermined maximum camera count.

---

# 31. RTSP / GStreamer

Using GStreamer is mandatory.

Support:

* RTSP;
* H.264;
* H.265 where possible;
* audio;
* hardware decoding, where available;
* reconnect;
* stream state;
* dropped-frame measurement, where accessible.

The GStreamer pipelines must not block the GUI thread.

---

# 32. Main/Sub Stream

If there is a:

```yaml
subUrl: ...
```

then:

```text
Mosaic     → subUrl
Fullscreen → mainUrl
```

If there is no `subUrl`, both may use `mainUrl`.

Fullscreen audio should likewise be handled from the fullscreen `mainUrl` stream.

---

# 33. Reconnect

Suggested backoff:

```text
1s
2s
5s
10s
30s
```

After that, retry every 30 seconds.

After a success, the backoff resets.

The scheduler must keep track of:

* the time of the next reconnect;
* the remaining time;
* the current backoff;
* reconnect count.

The status overlay must display the countdown from this exact same state.

The loss of one camera must not stop the application.

---

# 34. Aspect Ratio and Rendering

### Mosaic

```text
COVER
```

* aspect ratio preserved;
* full tile filled;
* cropping allowed;
* black bars not necessary.

### Fullscreen 1.0×

```text
CONTAIN
```

* full video visible;
* aspect ratio preserved;
* black bars allowed.

### Fullscreen >1.0×

```text
ZOOM/COVER
```

* aspect ratio preserved;
* full viewport filled;
* panable;
* full video visibility is no longer necessarily required.

---

# 35. UI/UX

The TV UI:

* readable from a large distance;
* minimal;
* video-centric;
* fully usable without a mouse;
* shows a clear focus.

The always-on status overlay must not significantly obscure the video.

The diagnostics overlay should only be visible on request.

---

# 36. Yellow Button

The Yellow CEC button:

```text
reserved
```

Currently should have no function.

The architecture should allow for assigning it a function later.

---

# 37. Excluded from the First Version

Do not implement:

* camera configuration GUI;
* PTZ;
* PTZ presets;
* camera recording;
* playback;
* motion detection;
* AI/object detection;
* cloud;
* mobile application;
* web frontend;
* remote administration;
* authentication.

---

# 38. Acceptance Criteria

The first version is considered functionally complete if:

1. It builds under C++17/C++20 + Qt 6.
2. Multiple cameras can be loaded from a YAML configuration.
3. Multiple RTSP streams are displayed simultaneously.
4. There is a focusable camera in mosaic view.
5. The image is not distorted in mosaic view; it fills the tile with cropping if necessary.
6. OK/left click enters fullscreen.
7. At fullscreen 1.0×, the entire video image is visible.
8. At fullscreen 1.0×, Left/Right switches the camera.
9. The red CEC button is Zoom+.
10. The green CEC button is Zoom−.
11. The blue CEC button toggles diagnostics.
12. At zoom >1.0×, the direction buttons perform panning.
13. After zoom returns to 1.0×, Left/Right switches the camera again.
14. Zoomable with the desktop mouse wheel.
15. Desktop zoom occurs around the mouse cursor.
16. The enlarged image is panable on desktop.
17. The camera's name and LIVE/LOST status can be displayed.
18. In fullscreen view, the status overlay gives a text-based indication of audio presence.
19. In the LOST state, the status overlay shows the reconnect countdown.
20. The countdown comes from the actual reconnect scheduler.
21. The `0–9` shortcuts work on CEC and on desktop.
22. `0` is still a camera selection even in a zoomed state.
23. Escape can perform a zoom reset in fullscreen.
24. Fullscreen audio works automatically when supported audio is present.
25. After an RTSP dropout, a reconnect occurs.
26. The loss of one camera does not stop the application.
27. It starts automatically on Raspberry Pi.
28. It works on Linux desktop without CEC.
29. It works on macOS.
30. The README contains installation and usage documentation.
31. Mosaic Right/Left navigation works across rows, in linear camera order.
32. Mosaic and fullscreen use the same camera order.
33. The reconnect countdown updates correctly every second.
34. During a reconnect attempt, the countdown disappears and the camera enters the CONNECTING state.
35. The audio status indicates whether the stream contains a supported audio stream, not the momentary audio output state.
36. The diagnostics overlay contains the available technical stream data.

---

# 39. Implementation Principles

The implementer must:

* first examine the repository;
* not needlessly rewrite working parts;
* introduce a new dependency only when justified;
* isolate platform-dependent code;
* separate the GUI, video pipeline, input, and state management;
* not put business logic into QML;
* not put GUI logic into the CEC adapter;
* not block the GUI thread with GStreamer operations;
* document important technical decisions.
