# QQCS

## 1. Projekt célja

Készíts egy natív, elsősorban Raspberry Pi-re szánt biztonságikamera-monitor alkalmazást, amely több RTSP videostreamet képes megjeleníteni HDMI-n csatlakoztatott TV-n, és a TV távirányítójával HDMI-CEC-en keresztül vezérelhető.

Az alkalmazás ugyanazon core kódbázissal fusson desktop Linuxon és macOS-en is.

Desktopon:

* billentyűzettel;
* egérrel

legyen vezérelhető.

Raspberry Pi-n:

* HDMI-n jelenjen meg a GUI;
* HDMI-CEC-en keresztül kezelje a TV távirányítóját;
* automatikusan induljon el.

---

## 2. Fő funkciók

Az alkalmazás:

* több RTSP kamera egyidejű megjelenítésére képes;
* mozaik/grid nézetet biztosít;
* fókuszálható kamerát kezel;
* kiválasztott kamerát fullscreenben jelenít meg;
* fullscreenben automatikusan lejátssza az audio streamet, ha van;
* fullscreenben előző/következő kamerára lehet váltani;
* digitális zoomot biztosít;
* zoomolt képen pan funkciót biztosít;
* kamera nevet és LIVE/LOST állapotot jelenít meg;
* az állandó status overlayen jelzi az audio jelenlétét;
* elveszett stream esetén megjeleníti a következő reconnectig hátralévő időt;
* opcionális diagnosztikai overlayt biztosít;
* `0–9` gyorskameraválasztást biztosít;
* HDMI-CEC távirányítóval vezérelhető Raspberry Pi-n;
* billentyűzettel és egérrel vezérelhető desktopon;
* Raspberry Pi-n automatikusan indul.

---

# 3. Technológiai követelmények

Preferált technológia:

| Terület      | Technológia           |
| ------------ | --------------------- |
| Fő nyelv     | C++17 vagy C++20      |
| GUI          | Qt 6 / Qt Quick / QML |
| Build        | CMake                 |
| Video/RTSP   | GStreamer             |
| HDMI-CEC     | libCEC                |
| Konfiguráció | YAML                  |

A fő alkalmazás C++ legyen.

Python csak segédscriptekhez használható.

Ne használj Electron/webview-alapú GUI-t.

---

# 4. Platformok

Elsődleges:

* Raspberry Pi 5;
* Raspberry Pi OS 64-bit;
* HDMI TV;
* HDMI-CEC.

Másodlagos:

* Linux desktop;
* macOS.

Desktopon a CEC és Raspberry Pi-specifikus autostart nem követelmény.

---

# 5. Architektúra

Javasolt rétegek:

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

Inputforrások:

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

A GUI ne kezelje közvetlenül a CEC-et vagy a GStreamer pipeline-okat.

---

# 6. Konfiguráció

A konfiguráció formátuma **YAML**.

Példa:

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
    name: "Bejárat"
    shortcut: 1
    mainUrl: "rtsp://192.168.1.101:554/main"
    subUrl: "rtsp://192.168.1.101:554/sub"

  - id: garden
    name: "Kert"
    shortcut: 2
    mainUrl: "rtsp://192.168.1.102:554/main"
    subUrl: "rtsp://192.168.1.102:554/sub"
```

A konfigurációban **nincs audio enable/disable mező**.

Fullscreenben az audio automatikus.

---

## 6.1 Kamera mezők

### `id`

Kötelező, egyedi azonosító.

### `name`

Opcionális, ember által olvasható név.

Ha nincs megadva, név-overlay ne jelenjen meg.

### `shortcut`

Opcionális egész szám `0–9` között.

A kamera gyorsválasztására szolgál.

A shortcutok legyenek egyediek.

### `mainUrl`

Kötelező RTSP stream URL.

Elsősorban fullscreenhez használatos.

### `subUrl`

Opcionális RTSP substream.

Ha létezik, mozaik nézetben ezt kell preferálni.

---

# 7. Konfiguráció validáció

Induláskor ellenőrizni kell:

* YAML szintaxis;
* kötelező mezők;
* kamera-ID-k egyedisége;
* shortcutok egyedisége;
* shortcut értéke `0–9`;
* layout értékek;
* overlay pozíció.

Hibás konfiguráció esetén:

* egyértelmű hiba kerüljön a logba;
* ne legyen silent failure;
* lehetőség szerint a GUI is mutasson használható hibaállapotot.

---

# 8. Mozaik nézet

Induláskor az alapértelmezett nézet a mozaik/grid.

A layout az oszlopok számát konfigurálja:

```yaml
layout:
  columns: 4
```

A sorok számát automatikusan kell meghatározni.

Például 10 kamera és 4 oszlop:

```text
┌──────────┬──────────┬──────────┬──────────┐
│ Camera 1 │ Camera 2 │ Camera 3 │ Camera 4 │
├──────────┼──────────┼──────────┼──────────┤
│ Camera 5 │ Camera 6 │ Camera 7 │ Camera 8 │
├──────────┼──────────┼──────────┼──────────┤
│ Camera 9 │ Camera10 │          │          │
└──────────┴──────────┴──────────┴──────────┘
```

A nem létező cellák nem fókuszálhatók.

---

# 9. Mozaik képarány-kezelés

A videó képe **soha nem torzítható**.

Mozaikban a renderelési mód:

```text
COVER
```

Ez azt jelenti:

* az eredeti képarány megmarad;
* a csempe teljes területe ki van töltve;
* szükség esetén a kép cropolva van;
* nincs fekete sáv;
* nincs stretch/distortion.

A crop alapértelmezés szerint középre igazított legyen.

---

# 10. Mozaik navigáció

A fókuszált csempét jól látható vizuális keret jelölje.

```text
Up   : fenti sor
Down : alsó sor
Left : előző kamera
Right: következő kamera
```

A `Left` és `Right` **nem soron belüli ciklikus navigáció**.

A kamera logikai sorrendje a YAML konfiguráció sorrendje:

```text
1  2  3  4
5  6  7  8
9 10 11 12
```

Ezért:

```text
4 → Right → 5
5 → Left  → 4

8 → Right → 9
9 → Left  → 8
```

Az első kamera `Left` esetén az utolsó kamerára, az utolsó `Right` esetén az első kamerára ugrik.

Az `Up` és `Down` lehetőség szerint ugyanabban az oszlopban mozogjon.

Hiányzó cellára nem lehet fókuszt helyezni; ilyenkor a legközelebbi létező kamera legyen kiválasztva.

---

# 11. Fullscreen

A fókuszált kamera fullscreen nézetbe választható:

```text
CEC OK
Enter
bal egérgomb
```

Fullscreenben:

* a teljes rendelkezésre álló képernyőt használja;
* a video képaránya változatlan;
* `zoom = 1.0×` esetén a teljes videókép látható;
* szükség esetén fekete letterbox/pillarbox sáv megengedett.

Fullscreen 1.0×:

```text
CONTAIN
```

Tehát nem kell a video teljes képernyőt kitöltse, ha ez torzítást vagy cropot igényelne.

---

# 12. Fullscreen kamera-váltás

Fullscreenben `zoom = 1.0×` esetén:

```text
Left : előző kamera
Right: következő kamera
```

A sorrend a YAML konfiguráció sorrendje.

A lista ciklikus:

```text
első Left → utolsó
utolsó Right → első
```

---

# 13. Fullscreen audio

Fullscreenben az aktuális kamera audio streamje automatikusan lejátszandó, amennyiben:

* az RTSP stream tartalmaz támogatott audio streamet;
* az audio dekódolható és lejátszható.

Nincs `audio: true/false` konfiguráció.

Ha nincs audio, a video ettől még működjön.

Kameraváltáskor:

1. az előző audio stream álljon le;
2. az új kamera audio streamje induljon, ha elérhető.

Audiohiba nem állíthatja le a videót.

---

# 14. Zoom

Támogatott zoomlépések például:

```text
1.0×
1.5×
2.0×
3.0×
4.0×
```

A maximális zoom legyen könnyen módosítható.

A zoom nem torzíthatja a képet.

Fullscreenben `zoom > 1.0×` esetén:

* a nagyított video töltse ki a viewportot;
* fekete sáv ne legyen szükséges;
* a képernyőn kívüli részek panolhatók;
* a teljes videókép láthatósága már nem követelmény.

---

# 15. CEC zoom

```text
PIROS : Zoom+
ZÖLD  : Zoom−
KÉK   : Diagnosztika ki/be
SÁRGA : reserved
```

A zoom origin CEC esetén a kép közepe.

A Zoom− ismételt megnyomásával vissza lehessen térni `1.0×` állapotba.

---

# 16. Desktop zoom

Desktopon:

```text
egérgörgő fel : Zoom+
egérgörgő le  : Zoom−
```

A zoom középpontja az egérkurzor aktuális pozíciója.

Az egér alatt lévő video-képpont maradjon lehetőleg ugyanazon képernyőpozícióban zoomolás közben.

Ez legyen intuitív „zoom to cursor” működés.

---

# 17. Zoom reset

Desktopon:

```text
Escape : Zoom reset
```

Reset:

```text
zoom = 1.0×
pan = (0, 0)
```

Ha `zoom > 1.0×`, az Escape zoom resetet végez.

Ha már `zoom = 1.0×`, fullscreenben az Escape visszalép a fullscreenből.

A `0` **soha nem zoom reset**.

---

# 18. Zoom és iránygombok

Ez kötelező állapotfüggő viselkedés.

### `zoom == 1.0×`

Fullscreen:

```text
Left : előző kamera
Right: következő kamera
```

### `zoom > 1.0×`

Fullscreen:

```text
Up   : pan fel
Down : pan le
Left : pan balra
Right: pan jobbra
```

Ha a zoom visszaáll `1.0×` értékre:

```text
zoom = 1.0×
pan = (0, 0)
```

Ezután Left/Right ismét kamerát vált.

---

# 19. Desktop pan

Fullscreenben `zoom > 1.0×` esetén:

```text
bal egérgomb + húzás : pan
```

A pan legyen korlátozva úgy, hogy ne lehessen a video érvényes tartományán kívülre navigálni.

---

# 20. Állandó status overlay

Az állandó status overlay megjelenhet:

* mozaik nézetben;
* fullscreen nézetben.

Tartalma:

1. kamera neve, ha van;
2. LIVE/LOST állapot;
3. audio jelenléte;
4. LOST esetén a következő reconnectig hátralévő idő.

Példák:

```text
● LIVE  🔊  BEJÁRAT
```

```text
● LIVE  🔇  KERT
```

```text
● LOST  🔊  BEJÁRAT
Reconnect: 4s
```

A status overlay legyen könnyen olvasható TV-ről is.

---

## 20.1 Audio státusz

Az audio ikon azt jelenti, hogy az aktuális stream tartalmaz-e támogatott audio streamet.

```text
🔊 : van támogatott audio stream
🔇 : nincs támogatott audio stream
```

Ez **nem** a pillanatnyi hangkimeneti állapotot jelenti.

Ha van audio stream, de lejátszási hiba történik, ezt a részletes diagnosztikai overlay mutassa.

---

## 20.2 Reconnect countdown

`LOST` állapotban az állandó status overlay mutassa:

```text
Reconnect: Ns
```

Például:

```text
LOST
Reconnect: 5s
```

majd:

```text
LOST
Reconnect: 4s
```

stb.

A countdown:

* másodpercenként frissüljön;
* a tényleges reconnect schedulerből származzon;
* ne külön, független UI-timerből számolódjon.

A reconnect pillanatában:

```text
LOST
Reconnect: 0s
```

ne legyen szükséges megjeleníteni.

Az állapot váltson például:

```text
CONNECTING
```

állapotra.

---

# 21. Kameraállapot

Minimum:

```cpp
enum class CameraState {
    Disconnected,
    Connecting,
    Live,
    Lost
};
```

Jelentés:

* `Disconnected`: nincs aktív kapcsolat;
* `Connecting`: kapcsolódási kísérlet folyik;
* `Live`: ténylegesen érkező és dekódolható video;
* `Lost`: korábban működő stream elveszett, reconnectre vár.

---

# 22. Diagnosztika

A kék CEC gomb kapcsolja a diagnosztikai overlayt.

Desktopon:

```text
I : diagnosztika ki/be
```

Lehetséges információk:

* kamera neve;
* kameraállapot;
* audio jelenléte;
* video codec;
* felbontás;
* FPS;
* bitrate;
* audio codec;
* RTSP transport;
* latency, ha megbízhatóan mérhető;
* dropped frames;
* reconnect count;
* reconnect backoff;
* következő reconnectig hátralévő idő;
* RTSP URL.

Ha nem érhető el:

```text
N/A
```

A diagnosztika overlay, nem külön ablak.

A reconnect countdown **nem kizárólag diagnosztikai információ**: LOST állapotban az állandó status overlayen is kötelező.

---

# 23. Gyorskameraválasztás

A `0–9` gombok közvetlen kamera-választásra használhatók.

Működjön:

* CEC távirányítón;
* desktop billentyűzeten;
* mozaikban;
* fullscreenben;
* zoomolt fullscreenben.

Például:

```yaml
shortcut: 5
```

esetén:

```text
5 → Camera 5
```

Ajánlott:

```text
Mozaik + 5      → Camera 5 fullscreen
Fullscreen + 5  → Camera 5 fullscreen
Zoom + 5        → Camera 5 fullscreen
```

Ha nem definiált shortcutot nyomnak meg, ne történjen semmi.

A `0` mindig kamera shortcut.

---

# 24. Input absztrakció

Javasolt:

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

A NavigationController dönti el az action jelentését az aktuális állapot alapján.

---

# 25. CEC input mapping

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

A CEC hiánya nem okozhat alkalmazásleállást.

---

# 26. Desktop keyboard input mapping

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

* `zoom > 1.0×` esetén zoom reset;
* `zoom == 1.0×` esetén fullscreenből visszalépés.

---

# 27. Desktop mouse

```text
bal kattintás    : Select
egérgörgő        : ZoomIn / ZoomOut
bal gomb + húzás : Pan, ha zoom > 1.0×
```

---

# 28. CEC

Raspberry Pi-n libCEC használata szükséges.

Támogatott:

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

# 29. Raspberry Pi autostart

Raspberry Pi-n az alkalmazás automatikusan induljon.

Preferált:

**systemd service**

Elvárások:

* grafikus környezet után induljon;
* crash után restartoljon;
* logolható legyen;
* ne `.bashrc`-ból induljon.

---

# 30. Teljesítmény

Raspberry Pi 5 az elsődleges cél.

Preferált:

* hardware decoding;
* GPU rendering;
* minimális CPU frame-copy;
* substream mozaikban;
* main stream fullscreenben;
* nem blokkoló GUI thread.

Legalább:

```text
4 kamera
9 kamera
16 kamera
```

esetén vizsgáld.

Mérendő:

* CPU;
* GPU;
* RAM;
* FPS;
* dropped frames;
* latency;
* hőmérséklet.

Ne legyen előre önkényesen meghatározott maximális kameradarabszám.

---

# 31. RTSP / GStreamer

GStreamer használata kötelező.

Támogatás:

* RTSP;
* H.264;
* lehetőség szerint H.265;
* audio;
* hardware decoding, ahol elérhető;
* reconnect;
* stream state;
* dropped-frame mérés, ahol hozzáférhető.

A GStreamer pipeline-ok ne blokkolják a GUI threadet.

---

# 32. Main/sub stream

Ha van:

```yaml
subUrl: ...
```

akkor:

```text
Mozaik    → subUrl
Fullscreen → mainUrl
```

Ha nincs `subUrl`, mindkettő használhatja a `mainUrl`-t.

Fullscreen audio szintén a fullscreen `mainUrl` streamből kezelendő.

---

# 33. Reconnect

Javasolt backoff:

```text
1s
2s
5s
10s
30s
```

Ezután 30 másodpercenként újrapróbálkozás.

Siker után a backoff resetelődik.

A scheduler tartsa nyilván:

* következő reconnect időpontja;
* hátralévő idő;
* aktuális backoff;
* reconnect count.

A status overlay ugyanebből az állapotból jelenítse meg a countdownot.

Egy kamera kiesése nem állíthatja le az alkalmazást.

---

# 34. Képarány és rendering

### Mozaik

```text
COVER
```

* képarány megőrzése;
* teljes csempe kitöltése;
* crop megengedett;
* fekete sáv nem szükséges.

### Fullscreen 1.0×

```text
CONTAIN
```

* teljes video látható;
* képarány megőrzése;
* fekete sáv megengedett.

### Fullscreen >1.0×

```text
ZOOM/COVER
```

* képarány megőrzése;
* teljes viewport kitöltése;
* panolható;
* a teljes video már nem feltétlenül látható.

---

# 35. UI/UX

A TV-s UI:

* nagy távolságból olvasható;
* minimális;
* video-központú;
* egér nélkül teljesen használható;
* egyértelmű fókuszt mutat.

Az állandó status overlay ne takarja ki jelentősen a videót.

A diagnosztikai overlay csak kérésre legyen látható.

---

# 36. Sárga gomb

A Yellow CEC gomb:

```text
reserved
```

Jelenleg ne legyen funkciója.

Az architektúra tegye lehetővé későbbi hozzárendelését.

---

# 37. Első verzióból kimarad

Ne implementáld:

* kamera-konfiguráció GUI;
* PTZ;
* PTZ preset;
* kamera-felvétel;
* playback;
* mozgásérzékelés;
* AI/object detection;
* cloud;
* mobilalkalmazás;
* webes frontend;
* távoli adminisztráció;
* autentikáció.

---

# 38. Elfogadási kritériumok

Az első verzió akkor tekinthető funkcionálisan késznek, ha:

1. C++17/C++20 + Qt 6 alatt buildelhető.
2. YAML konfigurációból több kamera betölthető.
3. Több RTSP stream egyszerre megjelenik.
4. Mozaikban fókuszálható kamera van.
5. Mozaikban a kép nem torzul, szükség esetén cropolva tölti ki a csempét.
6. OK/bal kattintás fullscreenbe visz.
7. Fullscreen 1.0× esetén a teljes videókép látható.
8. Fullscreen 1.0× esetén Left/Right kamerát vált.
9. Piros CEC gomb Zoom+.
10. Zöld CEC gomb Zoom−.
11. Kék CEC gomb diagnosztikát kapcsol.
12. Zoom >1.0× esetén az iránygombok pan funkciót végeznek.
13. Zoom 1.0×-re visszaállása után Left/Right ismét kamerát vált.
14. Desktop egérgörgővel zoomolható.
15. Desktop zoom az egérkurzor körül történik.
16. Desktopon nagyított kép panolható.
17. Kamera neve és LIVE/LOST állapota megjeleníthető.
18. A status overlay mutatja az audio jelenlétét.
19. LOST állapotban a status overlay mutatja a reconnect countdownot.
20. A countdown a tényleges reconnect schedulerből származik.
21. `0–9` shortcut működik CEC-en és desktopon.
22. A `0` zoomolt állapotban is kameraválasztás.
23. Escape fullscreenben zoom resetet tud végezni.
24. Fullscreen audio automatikusan működik, ha van támogatott audio.
25. RTSP kiesés után reconnect történik.
26. Egy kamera kiesése nem állítja le az alkalmazást.
27. Raspberry Pi-n automatikusan elindul.
28. Linux desktopon CEC nélkül működik.
29. macOS-en működik.
30. README tartalmazza a telepítési és használati dokumentációt.
31. Mozaik Right/Left navigáció sorokon át, lineáris kamera-sorrendben működik.
32. Mozaik és fullscreen ugyanazt a kamera-sorrendet használja.
33. A reconnect countdown másodpercenként helyesen frissül.
34. A reconnect kísérlet idején a countdown eltűnik, és a kamera CONNECTING állapotba kerül.
35. Az audio státusz azt jelzi, hogy a stream tartalmaz-e támogatott audio streamet, nem pedig a pillanatnyi hangkimenet állapotát.
36. A diagnosztikai overlay tartalmazza a rendelkezésre álló technikai streamadatokat.

---

# 39. Implementációs elvek

Az implementáló:

* először vizsgálja meg a repositoryt;
* ne írja át indokolatlanul a működő részeket;
* új dependency-t csak indokolt esetben vezessen be;
* izolálja a platformfüggő kódot;
* különítse el a GUI-t, video pipeline-t, inputot és state managementet;
* ne tegyen üzleti logikát QML-be;
* ne tegyen GUI-logikát a CEC adapterbe;
* ne blokkolja a GUI threadet GStreamer műveletekkel;
* dokumentálja a fontos technikai döntéseket.
