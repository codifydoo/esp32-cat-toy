# Cat toy — design spec

Date: 2026-09-22
Status: approved in chat, pending written-spec review

## 1. What we are building

A handheld cat toy for a 5-year-old, running on a Waveshare ESP32-S3-Touch-AMOLED-1.75
(1.75" round 466 × 466 AMOLED, touch, IMU, 2 mics, speaker, battery; the 1.75C has no RTC chip, §3.2). A cartoon cat hosts
everything: it lives on the menu, announces each app in Croatian, and does silly voices.

**Who it is for:** a 5-year-old who loves cats and silly voices, cannot read yet, and speaks Croatian.

**Success criteria**

1. She can turn it on, pick an app, play, and get back to the menu without asking for help.
2. No screen can trap her: the Home button returns to the menu from every screen except the
   bedtime lock, within 300 ms.
3. The UI contains no text she needs to read; every app is identified by picture and spoken name.
4. Adding an app is one new source file plus one line in the app registry.
5. Every sound respects the parent's volume cap.
6. Nothing she says leaves the device or is stored: recordings live in RAM only and are discarded
   when the app closes.

## 2. Source of truth for the UI

The UI design is the Claude Design canvas "Cat toy · round screen UI":
https://claude.ai/artifact/X7YZmxQEsLuHKrT755j53Q

A snapshot of its source is committed at `design/claude-design/`:

| File | Contents |
| --- | --- |
| `Main.dc.html` | Clickable prototype |
| `Screens.dc.html` | Every screen and state (about 40) |
| `Screen.dc.html` | The screen component all screens render through |
| `Cat.dc.html` | The cat, drawn as SVG from parameters (variant, expression, costume, look) |
| `CatSheet.dc.html` | Character sheet: 3 variants × 15 expressions, 4 costumes |
| `AppIcon.dc.html` | The 6 app icons as SVG |
| `Spec.dc.html` | Geometry, color tokens, icons, touch targets, gestures, motion, burn-in rules, sounds |
| `Assets.dc.html` | Asset list: parts, sizes, frame counts, fps |

This spec adopts the design as-is, except for the changes in section 3. Where the design and
this spec disagree, this spec wins.

### Summary of the design

- **Launcher:** a carousel with one 240 px app icon per page and the cat peeking over its rim.
  Swipe to change (the cat meows the app name), tap to open, and page dots run along the bottom arc.
- **Apps:**

  | # | App | Spoken name |
  | --- | --- | --- |
  | 1 | Silly voices | "Smiješni glasovi" |
  | 2 | Pet the cat | "Maži macu" |
  | 3 | Magic paws | "Čarobne šapice" |
  | 4 | Yarn roll | "Klupko" |
  | 5 | Sleepy clock | "Pospani sat" |
  | 6 | Meow piano | "Mijau klavir" |

- **Cat:** three variants (orange tabby, grey, tuxedo), 15 expressions and 4 costumes (mouse ears,
  lion mane, robot helmet, cave/miner hat), built from parts at three scales (S 0.65, M 1.0, L 1.3).
- **System screens:** boot, the first-run cat picker, idle dim, low battery, charging and bedtime lock.
- **Parent area:** volume cap, sleep times, show/hide apps and cat choice.
- **Burn-in rules:** dim after 20 s idle, the cat sleeps at 60 s, panel off at 120 s, static layers
  shift ±4 px every 60 s, and nothing bright stays static for longer than 60 s.

## 3. Changes to the design

### 3.1 Parent entry: two-finger hold

The parent area opens with **two fingers held on the screen for 5 s**, anywhere on the screen,
with neither point moving more than 40 px. The gesture works on the menu and on the bedtime lock
screen, so settings stay reachable at night. A one-finger hold does nothing special: she can hold
the cat as long as she likes. The progress ring from the design appears around the cat after 1 s
of a valid two-finger hold. The whole screen counts because two adult fingertips don't fit on the
150 px cat target.

The CST9217 reports up to two points. LVGL's pointer input only tracks one, so the gesture reads
the point count directly from the touch driver (`esp_lcd_touch`). The Waveshare CST9217 driver
stops at one point, so the count and the second point come from the controller's own report
(`touch_report.c`), read alongside the driver.

The first slice of the parent area (2026-09-23) opens straight on **Max volume**: the design's
270° arc, 10 % steps from 10 % to 100 %, and a "Home button exits" hint in place of the "‹ Parents"
button. It uses LVGL's built-in Montserrat until the full parent area brings Fredoka and the
four-tile home screen (M7).

### 3.2 Time source

The 1.75C has no RTC chip (docs/device-checks.md), so the ESP32-S3 keeps the time itself. Its clock
survives restarts and deep sleep, but not a power loss. Decided 2026-09-24:

- **NTP:** if Wi-Fi credentials are configured, the device connects and syncs over NTP
  (`pool.ntp.org`) at boot, and again once a day when the clock reaches the wake-up minute. Then it
  turns Wi-Fi off and releases it. The timeout is 10 s, and a failure is silent.
- **Manual:** the parent area's **Sleep times** screen gains a third row, **Now**, which sets the
  clock by hand (± buttons, press-and-hold to repeat). A manual set keeps today's date, or uses
  2 January 2026 when the clock has no date yet. Without NTP the date is then not the real one, so a
  DST change can come on the wrong night, leaving the clock an hour off until Now is set again.
- Time zone: `Europe/Zagreb` (POSIX `CET-1CEST,M3.5.0,M10.5.0/3`).
- **Valid time** means the clock reads 2026-01-01 or later, which only an NTP sync or a manual set
  since the last power-on can make true. If the time is invalid, the time-based features switch
  off rather than guess: no bedtime lock, and the Sleepy clock shows the day scene. Play is never
  blocked by a bad clock.
- The internal oscillator drifts. M6 measures how much (docs/device-checks.md), and the daily NTP
  sync bounds it when Wi-Fi is available.

### 3.3 Silly voice effects

All effects run on the finished clip, after recording and before playback. Nothing runs in real time.

| Costume | Effect |
| --- | --- |
| Mouse ears | Resample to play 1.5× faster: higher and quicker (≈ +7 semitones) |
| Lion mane | Resample to play 0.67× speed: lower and slower (≈ −7 semitones) |
| Robot helmet | 30 Hz ring modulation plus light bitcrush (8-bit, sample-and-hold ×2), as designed |
| Cave | Echo: 250 ms delay, 4 repeats, each at half volume, as designed |

The design called for a tempo-preserving pitch shift. Resampling replaces it: it is much cheaper,
and the speed change adds to the fun at 5.

### 3.4 Animation performance fallbacks

Two animations transform large images in software:
- The app-open zoom (240 px icon to 1.9×).
- The dizzy rotation (whole cat, ±7°).

Both are built as designed first. If the device measures below 24 fps during either animation
(LVGL system monitor), the fallback is:
- **Open:** fade plus scale 1.0 → 1.2.
- **Dizzy:** a horizontal wobble of ±10 px instead of rotation, with the spiral eyes and stars kept.

### 3.5 Sounds

The design lists when sounds play but has no sound asset list. This spec adds one (section 6.2).
- **Phrases:** recorded by the parent, then pitched up into a cat voice by the asset pipeline.
- **Effects:** synthesized by script where possible (blips, chimes, notes, ticks, clicks). CC0-licensed
  recordings are used for organic sounds (meows, purr, crunch, broom swish, yawn, snore, roar, munching).

### 3.6 Buttons

| Button | Wiring | Action |
| --- | --- | --- |
| BOOT | GPIO0, low when pressed | **Home.** Returns to the menu from anywhere except the bedtime lock; plays the descending "mew". |
| PWR | AXP2101; state readable on TCA9554 `EXIO4`, high when pressed | Short press: screen off/on. Long press: hardware power-off by the AXP2101. |

The design places the Home paw hint at 2 o'clock. Its angle is a single constant
(`HOME_HINT_ANGLE_DEG`), set once the physical position of BOOT is confirmed on the board
(milestone M0). If BOOT cannot be pressed through the case, Home moves to a short PWR press, and
screen off/on moves to a double press of PWR.

### 3.7 Hardware-driven adjustments

- **Audio rate:** the board support package's voice profile is 24 kHz. Recording uses MIC1 at 24 kHz
  mono instead of the design's 16 kHz. Five seconds is 240 KB in PSRAM.
- **Brightness:** the AMOLED has no backlight. Everything the design calls "backlight" is the
  CO5300 brightness command.
- **Deep sleep:** on battery, after the panel has been off for 10 minutes, the device enters deep
  sleep, woken by touch (GPIO11), BOOT (GPIO0), or a timer set for the next wake time. On waking it
  checks the schedule first:
  - Inside bedtime, it shows the bedtime lock without the boot animation.
  - At the wake-time timer, it plays the sunrise.
  - Otherwise it plays the boot animation.

  On USB power it never deep-sleeps.

### 3.8 Clarification: Sleepy clock and bedtime lock

- Allowed hours run from wake time to bedtime. The defaults are wake 07:00 and bedtime 19:30, and
  the parent can change both.
- **Bedtime starts:** any open app closes and the device shows the bedtime lock. The lock reuses
  the Sleepy clock's night scene: sleeping cat, stars for the hours left and 30–45 % brightness.
- **Bedtime lock on battery:** the panel turns off after 30 s. A touch shows the scene for 5 s with
  the "Pssst… spavaj" whisper (capped at 30 % volume). The deep-sleep rule in section 3.7 applies,
  and a timer wakes the device at wake time for the sunrise.
- **Bedtime lock on USB power:** the scene stays on all night as a night light. Burn-in
  protection shifts the whole scene by ±4 px every 60 s, and the stars twinkle.
- **Wake time:** the sunrise sequence plays (4 s, brightness 10 → 100 %), then "Dobro jutro!", and
  the menu follows after 60 s or on a tap.
- **Sleepy clock during allowed hours:** shows the day scene (sun, hills, awake cat), and tapping
  the cat purrs.

### 3.9 Cat parts addition

The Pet app's "eyes follow the ball" needs movable pupils. `cat_{v}_eyes_open` is exported
without pupils, and two shared parts are added:

| Part | Size at M |
| --- | --- |
| `cat_pupil` | 18 × 18 |
| `cat_pupil_highlight` | 8 × 8 |

The pupils are offset at runtime by up to ±4 px at M, matching `Cat.dc.html`.

### 3.10 Pet the cat details (2026-09-24)

The design leaves these open; they apply to Pet the cat (M4), and the tilt rules to Yarn roll (M5).

- **Level is her grip.** The way the toy is held when an app opens counts as level. Tilt is how far
  the direction of gravity has moved from that grip, so rolling works the same in all four
  directions whether she holds it flat or tipped toward her face.
- **Tilt input:** the accelerometer is read at 50 Hz and lightly smoothed, so a single bump does not
  roll the ball. Shake and tilt are ignored while the screen is dark and on the menu.
- **The ball** appears on the first tilt past the 8° dead zone, starting at (330, 110). Its center
  stays within r 190 of the screen center, and it hides 3 s after the tilt returns inside the dead
  zone. One bell tick plays per 60 px rolled, only while no other reaction is playing.
- **Pupils follow the ball** only while the cat shows its plain face. During a reaction the ball
  keeps rolling and the eyes do whatever that reaction's face does.
- **Reactions:**
  - Tap: purr face for 1.8 s, with the purr loop for 1.5 s.
  - Rub: laughing face for 1.8 s, with the giggle and then "Hihihi!".
  - Shake: dizzy for 2 s, with "wheee" and a boing.
  - Fish: eating for 2.4 s, with crunch, crunch, "Njam!". The fish button drops to 50 % while she eats.

  A new reaction replaces the current one, sound included.
- **Rub** counts finger travel in steps of 10 px or more, so touch jitter under a finger held still
  never adds up to a giggle.
- **Activity:** rolling the ball and shaking count as activity, so tilt-only play is not closed by
  the 60 s idle timeout. A ball resting against the rim does not count, so a toy left tilted on a
  table still returns to the menu. Movement under about 25 px/s (a tilt within about 2° of the dead
  zone) neither ticks nor counts as activity, so accelerometer noise can't hold a pinned ball
  "awake"; M8 tunes this.
- **Size:** the design's 250 px cat is drawn at scale L (1.3×), centered on the design's head center
  (233, 159).
- **Sensor events** (buttons, shake) reach the LVGL task through a queue that a 20 ms LVGL timer
  drains, together with the latest tilt. The sensors task never waits for the LVGL lock, so a slow
  frame cannot swallow a quick second button press.

### 3.11 Magic paws, Yarn roll and Meow piano details (2026-09-24)

The design leaves these open for M5. Everything else follows the design.

- **Magic paws: her drawing is kept.**
  - The drawing stays in memory when the app closes, by Home or by the 60 s idle timeout. It is back
    when she reopens the app, until she shakes it clean or the toy restarts.
  - It is one 466 × 466 RGB565 picture (434 KB of PSRAM), allocated on the first open and never
    freed. This is the one exception to "`destroy` leaves no PSRAM behind" (§4.4).
- **Magic paws: how strokes are painted.**
  - Strokes are painted into that picture, not built from objects, so there is no limit on dots.
    The design's 600-dot and 120-print caps were for the prototype.
  - Rainbow dots are placed every 7 px along the stroke, including between touch samples, so fast
    strokes stay continuous. Paw prints are placed every 40 px the same way.
  - Each rainbow dot is painted with its 28 % halo just before it. Where strokes cross, the newer
    halo tints the older dots slightly (the design keeps every dot above every halo).
  - Paw prints are painted as five round blobs (pad and toes), turned along the stroke.
  - Dots and prints are placed only where the stroke is within r 212 of the center (their edges may
    reach a little further).
  - A sparkle tick plays every 8 dots, and a soft "pat" for each paw print, each at most 8 per
    second.
  - The empty canvas shows the design's twinkling rainbow-dot hint.
- **Magic paws: sweep.** A shake dims the drawing to 30 %. The cat (scale M) sweeps its broom
  sideways ±60 px, three swings in 1.6 s, to a broom swish. The broom itself does not rotate.
  "Čisto!" plays 0.9 s in, and at 1.7 s the canvas is clean and the cat is gone. Touches and shakes
  are ignored while it sweeps. Home or the idle timeout during the sweep still leaves the canvas
  clean.
- **Yarn roll: rolling.**
  - The yarn rolls by the §3.10 tilt rules, with its center kept within r 185 of the screen center.
    Its four roll frames advance with the distance rolled. A rumble loop plays while it rolls.
  - The cat (scale S, surprised face) follows 92 px behind the yarn on the side toward the screen
    center, eases into place, and looks at the yarn.
  - The tilt hint shows for the first 4 s after the app opens.
  - A shake puts the yarn back in the middle (not during the win).
- **Yarn roll: winning.**
  - The basket is at (330, 318), (136, 318), (330, 150) or (136, 150), moving to the next one each
    round. She wins when the yarn's center comes within 62 px of the point 10 px above the basket's
    center. The yarn stops there, and 0.32 s later the win scene replaces the play scene.
  - The win scene shows the full basket, the proud cat (scale M), 24 falling confetti pieces, the
    fanfare and "Bravo!". The next round starts 3.5 s later with the yarn back in the middle.
  - The confetti is built as designed first, spinning as it falls. If it measures below 24 fps, the
    fallback drops the spin.
- **Meow piano.**
  - Five cats sit on a ring of r 140, starting at the top, one per note C, D, E, G, A. Each cat is
    recolored in its note's colors as the design draws them: fur, outline, a light muzzle, no
    stripes. These palettes are exported at scale S only.
  - A tap plays that note's meow. The cat sings, bounces (up 22 px, down, a 6 px rebound, 400 ms),
    and its pad fills at 25 %. The center note takes its color. The cat returns to happy after
    700 ms, and a new tap replaces the previous meow.

### 3.12 Time and bedtime details (2026-09-24)

The design leaves these open for M6. Everything else follows the design and §3.8.

- **Night** runs from bedtime to wake time, across midnight. If the two are equal there is no night.
  The schedule is checked every second.
- **Bedtime starts:** at the bedtime minute any open app closes, without the Home "mew", and the
  bedtime lock shows. A power-on or restart inside the night plays the boot animation, then shows the
  lock. Changing the sleep times in the parent area takes effect when the parent leaves the area.
- **The lock's scene** is the design's Sleepy clock night scene: the moon, nine twinkling stars
  (3 s each, staggered by the design's delays, 30 → 100 %), the sleeping cat (scale M) and five hour
  stars along the bottom. The panel runs at 35 % brightness.
- **Hour stars:** a lit star for each hour left until wake time, rounded up, at most five. All five
  are lit while more than four hours are left, and the last one goes out at wake time.
- **On the lock:**
  - Home does nothing. PWR still turns the panel off and on.
  - A touch whispers "Pssst… spavaj" at 30 % of the playing volume (a gain in the mixer, so the
    parent's cap still applies), at most once every 5 s. With the whisper the cat shows its shh face
    for 2.2 s, as in the design.
  - On battery the panel turns off after 30 s, and a touch shows the scene again for 5 s. Deep
    sleep arrives in M7; until then the device stays on through the night.
  - On USB power the scene stays on all night, shifted ±4 px every 60 s.
  - The parent two-finger hold works, with its progress ring around the sleeping cat's head.
    Leaving the parent area returns to the lock while it is still night. If the parent moved the
    times so that it is no longer night, leaving goes to the menu, without the sunrise.
- **USB power** is the AXP2101's "VBUS good" status bit.
- **Sunrise** plays at wake time if the lock is showing: the sun rises 110 px over 4 s while the
  brightness goes from 10 to 100 %, then the rising four-note chime, a happy meow and "Dobro
  jutro!". The menu follows 60 s later, or on a tap once the sun is up, without the Home mew.
- **The Sleepy clock app** shows the day scene: the sun with slowly turning rays, the hills and the
  proud cat. A tap on the cat purrs, as in Pet the cat (purring face, the purr loop for 1.5 s). The
  app is never open at night, because the lock closes it.
- **The parent area in M6:**
  - The home screen has two of the design's four tiles, Volume and Sleep. Apps, Cat and the
    battery in the center arrive in M7. The text is still Montserrat.
  - Sleep times has three rows: Bedtime and Wake up in 15-minute steps, and Now in 1-minute steps.
    Holding ± repeats, and after 2 s the Now row steps by 10 minutes.
  - Now shows "--:--" while the time is invalid, and its first press starts from 12:00.

## 4. Architecture

### 4.1 Toolchain and dependencies

| Item | Choice |
| --- | --- |
| Framework | ESP-IDF v5.5.x (Waveshare validates v5.5.5), C |
| Board support | `waveshare/esp32_s3_touch_amoled_1_75` ^3.0.1: display (CO5300 QSPI), touch (CST9217), audio codecs (ES8311, ES7210), SD, LVGL port |
| Graphics | LVGL 9.4 |
| IMU | `waveshare/qmi8658` |
| Power | XPowersLib for the AXP2101, as in Waveshare example `01_AXP2101` |
| Time | The ESP32-S3's own clock with SNTP (`esp_netif_sntp`); the 1.75C has no PCF85063 (§3.2) |
| Filesystem | LittleFS (`joltwallet/littlefs`) on an internal flash partition holding every image and sound. The SD card is not used. |
| Host tests | CMake plus Unity for pure-logic modules, run on the Mac |

### 4.2 Flash layout (16 MB)

| Partition | Size | Contents |
| --- | --- | --- |
| nvs | 24 KB | Settings |
| phy_init | 4 KB | |
| factory | 4 MB | Firmware (no OTA in v1) |
| assets | ~11.9 MB | LittleFS: images (.bin, RGB565A8) and sounds (.wav) |

### 4.3 Source layout

```
cat-toy/
  main/
    app_main.c            boot sequence: platform init, settings, assets, launcher
    Kconfig.projbuild     Wi-Fi SSID and password (empty = no NTP)
  components/
    platform/             hardware services, no UI
      display.c/.h        brightness, panel on/off
      buttons.c/.h        BOOT and PWR events (debounced)
      touch_raw.c/.h      two-point reads for the parent gesture
      imu.c/.h            50 Hz sampling task -> shake and tilt events
      audio.c/.h          play clip, record clip, global volume cap
      pmu.c/.h            AXP2101 register access, shared by buttons and power
      power.c/.h          USB power (M6); battery %, charging state, deep sleep (M7)
      clock.c/.h          system time, NTP sync, time zone, validity
      settings.c/.h       NVS-backed settings
    logic/                pure C, no ESP-IDF includes, host-tested
      gestures.c/.h       tap, swipe, rub, two-finger hold classification
      motion.c/.h         shake and tilt detection from accel samples
      voice_fx.c/.h       resample, ring-mod, bitcrush, echo on int16 buffers
      schedule.c/.h       allowed hours, bedtime/wake transitions, stars left
      carousel.c/.h       page index, dot windowing (max 7 dots), wrap rules
      envelope.c/.h       audio level -> talk mouth frame A/B/C
    assets/
      assets.c/.h         mount LittleFS, load .bin into PSRAM, lv_image_dsc_t cache
      sounds.h            sound_id_t enum -> file path table
    cat/
      cat.c/.h            cat widget: parts, states, frame tables, costumes, look, talk
    ui/
      tokens.h            design constants: colors, sizes, durations, thresholds
      launcher.c/.h       carousel, idle dim, parent gesture entry
      app_manager.c/.h    open, close, Home, event routing
      bedtime.c/.h        bedtime lock and sunrise
      scenes.c/.h         the Sleepy clock's night and day scenes
      hold_ring.c/.h      the parent two-finger hold and its ring (menu and lock)
      overlays.c/.h       low battery, charging
      parent.c/.h         parent area screens
      first_run.c/.h      cat picker
    apps/
      app.h               the app_t contract
      registry.c          const app_t *const APPS[] = { &app_voices, ... };
      voices.c  pet.c  draw.c  yarn.c  clock.c  piano.c
  tools/
    export_images/        Node: design SVG -> per-part SVG -> PNG -> LVGL .bin
    build_sounds/         sox-based conversion and pitching, plus a synth script
  assets-src/
    sounds/phrases/       parent recordings (WAV, any rate)
    sounds/sfx/           CC0 recordings, with LICENSES.md
  assets/                 generated; gitignored; packed into the LittleFS image
  test/                   host tests for components/logic
  design/claude-design/   design snapshot (read-only reference)
```

### 4.4 The app contract

```c
typedef struct {
    const char *id;                         // "voices"
    const char *icon;                       // "app_voices.bin" (240 px)
    const char *icon_small;                 // "app_voices_96.bin" (parent grid)
    uint32_t    color;                      // 0xFF5FA2, used for dots and accents
    sound_id_t  name_sound;                 // SND_NAME_VOICES
    void (*create)(lv_obj_t *screen);       // build UI into an empty screen
    void (*destroy)(void);                  // stop audio and timers, free assets
    void (*on_shake)(void);                 // optional, NULL if unused
    void (*on_tilt)(float x_deg, float y_deg); // optional, 50 Hz while open
} app_t;
```

Rules for apps:
- **Owned resources:** an app owns only what it creates. `destroy` must leave no timers,
  animations, audio or PSRAM behind; a leak test checks this (section 7). The one exception is
  Magic paws' drawing, which is kept on purpose (§3.11).
- **No blocking:** apps never block the LVGL thread. Recording and effect processing run on the
  audio task, and results come back through `lv_async_call`.
- **No global state:** apps do not touch buttons or settings directly. The app manager handles Home.
  Settings are read through `settings.h` getters.

### 4.5 Threads and data flow

| Task | Core | Job |
| --- | --- | --- |
| LVGL | 1 | Owned by the board support package's LVGL port. All UI runs here. |
| Audio | 0 | Mixes two channels: `fx` (one-shot; a new sound replaces the playing one) and `loop` (at most one looping sound, such as purr, rumble or munching, started and stopped by the app). Also records up to 5 s and runs `voice_fx`. |
| Sensors | 0 | IMU at 50 Hz, buttons, battery every 5 s. Queues button and shake events and keeps the latest tilt; a 20 ms LVGL timer in `app_manager` drains them and routes them to the launcher or the current app (§3.10). |

- **Settings:** loaded once at boot. The parent area writes through `settings_set_*`, which
  persists to NVS immediately.
- **Stored settings:**

  | Setting | Default |
  | --- | --- |
  | Cat variant | Chosen on first run |
  | Volume cap | 100 % |
  | Bedtime | 19:30 |
  | Wake time | 07:00 |
  | Hidden-apps bitmask | None hidden |
  | First run done | false |

### 4.6 Memory budget (8 MB PSRAM)

| Use | Budget |
| --- | --- |
| LVGL draw buffers and frame buffers (board package defaults) | ≤ 1.0 MB |
| Chosen cat, all three scales (loaded at boot) | ~1.3 MB |
| Menu icons, 6 × 240 px (loaded at boot) | ~1.0 MB |
| Largest app's own assets (loaded on open, freed on close) | ≤ 1.5 MB |
| Magic paws' picture (allocated on first open, kept until restart, §3.11) | 434 KB |
| Audio: 5 s record buffer, effect output buffer, playback buffer | ~0.8 MB |
| Headroom | ≥ 2 MB |

### 4.7 Failure behaviour

The rule: failures degrade play gracefully, and they never raise the volume or lock her out.

| Failure | Behaviour |
| --- | --- |
| Missing image asset | Log an error and draw a magenta circle placeholder (visible in development, never crashes) |
| Missing sound | Log it and stay silent |
| Audio codec init fails | Everything runs silently; Silly voices shows the cat with the mic disabled (grey) |
| IMU init fails | Shake and tilt do nothing; the apps still work by touch |
| Time invalid (power lost, no NTP and no manual set) | Time features off (section 3.2) |
| Battery gauge unavailable | Hide battery screens; never force low-battery mode |
| NVS corrupt | Erase and use defaults, and run first-run again |

## 5. Screen and behaviour details

Everything in `Spec.dc.html` applies (geometry, touch targets, gestures, motion, burn-in), with
the changes in section 3. Constants from the design live in one header (`ui/tokens.h`): colors,
sizes, durations and gesture thresholds, named after the design's tokens.

## 6. Asset pipeline

### 6.1 Images

1. `tools/export_images` (Node) holds a port of `Cat.dc.html` and `AppIcon.dc.html`'s geometry. It
   emits one standalone SVG per part, variant and frame, following the naming in `Assets.dc.html`,
   plus the pupil parts from section 3.9.
2. `@resvg/resvg-js` rasterizes each SVG to PNG at S, M and L sizes. Sizes are rounded up to even pixels.
3. LVGL's `scripts/LVGLImage.py` converts each PNG to `.bin` in RGB565A8.
4. A contact-sheet HTML page is generated alongside, to compare the exports against `CatSheet.dc.html` by eye.

Scene and UI images from `Assets.dc.html` (bowls, moon, sun, hills, yarn, basket, buttons) are
drawn as SVG in the same tool, from the design's `Screen.dc.html` geometry.

### 6.2 Sounds

All sounds are stored as WAV, 24 kHz, 16-bit, mono, normalized to −3 dBFS peak with silence trimmed.
`tools/build_sounds` uses `sox`.

**Phrases**, recorded by the parent and pitched up 4 semitones (tunable) into the cat voice:

Bok! · Smiješni glasovi · Maži macu · Čarobne šapice · Klupko · Pospani sat · Mijau klavir ·
Idemo! · Hihihi! · Njam! · Njam njam! · Čisto! · Bravo! · Pssst… spavaj · Dobro jutro! · Gladna sam…

**Synthesized effects** (script, no licensing): blip up, blip down, pop, UI click, rising boot
chime, 4-note wake chime, sparkle tick, bell tick, beep-boop (robot sting), fanfare.

**CC0 recordings** (source and licence logged in `assets-src/sounds/sfx/LICENSES.md`): meow (3
variants), descending mew, stretch "mrrrow", purr loop, giggle, "wheee", boing, crunch, broom
swish, rolling rumble loop, whoosh, squeak, roar, yawn, munching loop, snore.

**Derived by the pipeline:**

| Output | Made from |
| --- | --- |
| Piano meows C5, D5, E5, G5, A5 | One meow, pitch-shifted |
| Each costume's "Bok!" preview | The "Bok!" phrase run through that costume's effect (the same `voice_fx` code, compiled for the host) |

## 7. Testing

- **Host unit tests** for `components/logic`:
  - Gesture thresholds: tap ≤ 20 px, swipe ≥ 60 px at 1.5× vertical within 400 ms, rub ≥ 150 px.
  - Two-finger hold: 5 s, ≤ 40 px movement per point, reset when a finger lifts.
  - Shake (> 2.2 g for 3 samples within 300 ms, then a 1.5 s lockout).
  - Tilt dead zone and speed curve.
  - Effect outputs: length ratios, echo tap positions and gains, no clipping overflow.
  - Schedule, including midnight-spanning hours and DST change days.
  - Carousel dot windowing.
  - Envelope → mouth frame.
- **On-device checks** for each milestone, recorded in `docs/device-checks.md`:
  - Menu swipe ≥ 24 fps (LVGL system monitor).
  - App open < 500 ms.
  - Boot to interactive menu < 4 s.
  - Home works from every screen.
  - Volume never exceeds the cap.
  - Free PSRAM after boot ≥ 2 MB.
  - Leak test: free heap and PSRAM are unchanged (±4 KB) after opening and closing each app 50 times.
- **Kid test:** after each playable milestone, 15 minutes of play with no adult help. Every time she
  asks for help, or gets stuck, is logged as a bug.

## 8. Safety and privacy

- A protective case is required before she gets the device: the -B enclosure or a printed one.
  The board and its lithium battery are never handed over bare.
- The volume cap defaults to 100 % (the codec's 0 dB; 60 % measured far too quiet on the device,
  2026-09-23), and the parent area can lower it. Check the real loudness at the kid test (M8).
- No cloud, no accounts. The network is used only for NTP at boot and once a day at wake time.
  Wi-Fi is off the rest of the time.
- Recordings are held in RAM only and discarded when Silly voices closes or the device sleeps.
- Wi-Fi credentials live in a gitignored `sdkconfig.secrets`, never in the repository. The generated
  `sdkconfig` is gitignored too, because it contains them after a build; the committed baseline is
  `sdkconfig.defaults`.

## 9. Milestones

Each milestone ends with a flashed, working device.

| # | Milestone | Done when |
| --- | --- | --- |
| M0 | Toolchain and board bring-up | ESP-IDF installed; the Waveshare LVGL demo runs; screen, touch, speaker, mics, IMU, BOOT and PWR all verified. Hardware checklist: variant, battery connected, speaker connected, button positions recorded. |
| M1 | Skeleton | Project builds; partitions and LittleFS assets flash; platform services; app registry; launcher carousel with placeholder icons and a static cat image; Home button; idle dim. |
| M2 | Asset pipeline and cat | All cat parts and icons exported; cat widget with every state, costume and look; launcher uses the real cat and icons; boot animation. |
| M3 | Silly voices | Record, the 4 effects, talking mouth, costume cycling. **First kid test.** |
| M4 | Pet the cat | Tap, rub, shake, tilt, fish. |
| M5 | Magic paws, Yarn roll, Meow piano | All three apps. |
| M6 | Time and bedtime | NTP and the manual clock, Sleepy clock, bedtime lock, sunrise, night light. |
| M7 | System and parent | Cat picker, low battery, charging, parent area, deep sleep, burn-in shifts. |
| M8 | Tuning | Kid-test fixes, volume and sensitivity tuning, battery life measurement. |

Implementation plans are written per milestone group. The first plan covers M0–M3, the shortest
path to something she can play with.

## 10. Out of scope for v1

AI chat, OTA updates, SD-card media, Bluetooth, a Wi-Fi setup screen (credentials are set at
build time), languages other than Croatian, and saving recordings.
