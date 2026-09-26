# Device checks

Results of on-device checks, newest last. Every task that flashes the board appends here.

## Pending device session (code complete 2026-09-23, board not yet connected)

All firmware tasks (M0-M3, Tasks 1-18) are code complete and build clean on the host. No board has
been connected during this work. The checklist below lists every on-device or human check still
owed, in plan order. Work through it once the board is available, then record results in a
"## <Task name> (Task N)" section per task, as the plan directs.

### Task 2: board bring-up

- [ ] Connect the board to the Mac with a USB-C data cable; confirm exactly one `/dev/cu.usbmodem*`
      path appears.
- [ ] Build and flash Waveshare's `02_lvgl_demo_v9` example; confirm the round screen animates the
      LVGL benchmark.
- [ ] Answer and record: board variant (standard / -B case / -G GPS); battery connected and its
      capacity; speaker connected to the 2-pin connector; BOOT and PWR button positions on a clock
      face; whether BOOT can be pressed through the case.
- [ ] Append the "## Hardware (Task 2)" table to this file with those answers (see the plan's Task
      2 Step 4 for the exact table) and commit. If BOOT is not reachable through the case, note
      that spec §3.6 applies (Task 16's Home moves to a short PWR press instead).

### Task 3: skeleton smoke test

- [ ] Flash the skeleton build (commit `a970581`).
- [ ] Confirm the asset listing shows `hello.txt` and PSRAM free > 7,000,000 bytes.
- [ ] Confirm an orange circle draws on screen.
- [ ] Record "## Skeleton (Task 3)".

### Task 8: costume voice clips (Mac-only, no flash)

- [ ] `afplay` the 4 costume sting clips. If the mouse or lion voice sounds wrong, suspect
      `fxcli`'s resampling/pitch code first.

### Task 9: asset export design compare (Mac-only, no flash)

- [ ] Compare the exported preview against the character sheet.
- [ ] Record "## Asset export (Task 9)".

### Task 10: assets smoke test

- [ ] Flash the smoke build (commit `6966667`).
- [ ] Confirm the log line `SMOKE: sounds ok=22 missing=0; placeholder for missing image: yes`.
- [ ] Visual check: head at top, pink mic at bottom, magenta placeholder dot at left, and colors
      are not swapped.
- [ ] Record "## Assets (Task 10)".

### Task 11: buttons smoke test

- [ ] Flash the button smoke build (commit `f0380a8`).
- [ ] Confirm the log line `SMOKE: volume cap 60%`.
- [ ] Press BOOT 3 times and PWR once briefly; confirm exactly 3 HOME events and 1 POWER event are
      logged.
- [ ] Record "## Buttons (Task 11)".

### Task 12: audio smoke test

- [ ] Flash (commit `7256713`, which carries the Task 12 smoke build).
- [ ] Confirm the log line `audio: ready: 24000 Hz mono, volume 60%`.
- [ ] Confirm boot plays a meow and a chime; BOOT plays a meow; PWR plays a blip; volume is
      comfortable.
- [ ] Missing-sound check: move `meow_1.wav` out of the way, rebuild, flash, press BOOT twice;
      confirm exactly one "missing sound" line logs, no crash, and playback is silent (not a
      crash). Restore the file afterward.
- [ ] Record "## Audio (Task 12)".

### Task 13: diagnostics screen (M0 gate)

- [ ] Flash `build-diag`.
- [ ] Confirm the log line `DIAG: ready` and accelerometer lines logging once per second.
- [ ] Touch direction check: top of screen reports y < 100, right side reports x > 360. If not,
      flip `mirror_y`/`mirror_x` in `components/platform/display.c`.
- [ ] Accelerometer check: flat gives z ~= +-1 g; tilting right changes x/y as expected (record the
      signs observed, needed for M4).
- [ ] Button check: BOOT x2, PWR x1 -> exactly HOME=2, POWER=1 counts, with blips.
- [ ] Record a clip saying "Bok, maco!"; confirm it plays back and `heard=1` in the log. If not
      heard, set `AUDIO_MIC_CHANNELS` to 2 in `components/platform/audio.c` and retest.
- [ ] Confirm the magenta placeholder dot still shows where expected.
- [ ] Record "## Diagnostics (Task 13)".

### Task 14: cat gallery (M2 gate)

- [ ] Flash `build-gallery` (with `build-perf` sdkconfig layered in).
- [ ] Confirm the log line `GALLERY: state=happy costume=none scale=M variant=0`, FPS lines
      logging, and no "missing image" lines.
- [ ] Human check: compare all 15 expressions against the character sheet; cycle costumes (BOOT)
      and variants/scales (PWR); confirm the happy state's bob/blink/tail animate over 10 s.
- [x] Dizzy state FPS check: if FPS < 24, replace the rotation line in `apply()`
      (`components/cat/cat.c`) with the spec §3.4 `translate_x` wobble. This is a manual edit, not
      a runtime switch; it was needed and is applied (commit a472015).
- [ ] Record "## Cat gallery (Task 14)".

### Task 15: menu (icon carousel)

- [ ] Flash the normal build; confirm no "missing image" lines.
- [ ] Menu walkthrough: cat sits over the pink mic, paw chevrons show, 6 dots show, the paw hint
      points at roughly 2 o'clock; swipe left/right including wraparound; tap the screen edges;
      tapping the cat says its name; tapping an icon zooms in to a placeholder screen, tapping the
      placeholder makes the cat laugh and meow, and BOOT plays a mew and returns to the same menu
      position.
- [ ] Record "## Menu (Task 15)".
- [ ] Rapid-input check with `serial_log` running for 60 s: double-tap an icon, fast repeated
      swipes, a swipe immediately followed by a tap, a tap plus BOOT during the open zoom, and
      mashing the screen edges. Confirm at most one app ever opens, the UI recovers, and there is
      no Guru Meditation / abort in the log.
- [ ] Flash `build-perf`; do 10 swipes plus 1 app open; confirm FPS >= 24. If the open zoom is
      below 24, change the `ICON_SCALE_OPEN` define in `components/ui/include/tokens.h` from 486 to
      307 (a manual edit, not a runtime switch) and retest.
- [ ] Record "## Menu FPS (Task 15)".
- [ ] Re-flash the normal build afterward.

### Task 16: idle, dimming, and boot (M1 gate)

- [ ] From Task 2's recorded BOOT clock position (or PWR's, if `CAT_HOME_ON_POWER` will be
      enabled), compute `HOME_HINT_ANGLE_DEG = hour*30-90` and set that `#define` in
      `components/ui/include/tokens.h` (it is not a Kconfig option). If BOOT is not reachable
      through the case (Task 2 finding), append `CONFIG_CAT_HOME_ON_POWER=y` to
      `sdkconfig.defaults`, then `rm sdkconfig && idf.py fullclean build` (an existing `sdkconfig`
      ignores new defaults).
- [ ] Flash and check: boot animation shows rings, a yawn, and a meow+chime, reaching the menu in
      about 2.6 s; holding BOOT during boot suppresses the mew and the app name; the screen dims
      after 20 s idle, sleeps after 60 s, and goes fully dark after 120 s; tapping the screen while
      dark only wakes it (no other action); pressing PWR inside an app goes dark, and tapping wakes
      it without the cat laughing; an app's own 60 s idle timeout returns to the menu; the paw hint
      points at the Home control.
- [ ] Record "## Idle and boot (Task 16)".

### Task 17: silly voices app (M3 gate)

- [ ] Flash the normal build; confirm no "missing image" lines.
- [ ] Screen check: pink dotted ring, mouse ears, pink mic; tapping the cat 4 times cycles
      lion/robot/miner+cave-arch/mouse, each with the matching ring color, sting sound, and "Bok"
      line.
- [ ] Record and play back a clip in all 4 voices; also test stopping a recording early.
- [ ] Edge-case check with `serial_log` running for 120 s: a silent take should play a meow instead
      of silence (repeat this several times, since it depends on mic timing); a loud take should
      not crackle; pressing Home while listening; pressing Home while playing; reopening the app
      afterward. Confirm no Guru Meditation, abort, assert, or `rst:` line appears.
- [ ] Log the audio task's free-stack high-water mark after the first playback.
- [ ] Record "## Silly voices (Task 17)".

### Task 18: soak test and kid session

- [ ] Flash `build-soak` and let it run about 340 s; confirm a `SOAK` baseline line, 5 round lines,
      and a final `SOAK: PASS` with both internal and PSRAM heap deltas within +-4096 bytes.
- [ ] Re-flash the normal build afterward.
- [ ] Fill in the "## M0-M3 summary (Task 18)" table in this file.
- [ ] Run a kid test session and record results in `docs/kid-tests.md`.

### Additional checks from the final review

- [ ] Log the free-stack high-water mark of the audio, sensors, soak, and LVGL tasks after the
      first recording and after the first soak round (not just audio, per Task 17's note above).
- [ ] Watch for a white flash at power-on before the boot screen appears (the black-screen fix in
      this change should prevent it -- confirm on real hardware).
- [ ] Time the Home action with a slow-motion video; spec requires under 300 ms from press to menu.
- [ ] On the first swipe to each app icon, note any stutter (icons currently load their image on
      first use rather than at boot). If a stutter is visible, preload the visible app icons during
      the boot animation instead (spec §4.6).
- [ ] Repeat the silent-recording check (Task 17) several times, since whether a take is judged
      "silent" depends on mic timing that can vary run to run.

### Hardware note

Holding BOOT (GPIO0, a strapping pin) while powering on enters the ROM's download mode instead of
booting the app -- the screen stays black. That is expected hardware behavior, not a firmware bug.

## Hardware (Task 2)

| Item | Result |
| --- | --- |
| Serial port | /dev/cu.usbmodem101 |
| Variant | -B (case) |
| Battery | connected (capacity not recorded) |
| Speaker | connected |
| BOOT position | TBD |
| PWR position | TBD |
| BOOT reachable through case | yes |
| Waveshare LVGL demo runs | yes |

## Diagnostics (Task 13) — 2026-09-23

| Check | Result |
| --- | --- |
| Touch direction (USB port down) | PASS: top tap y ≈ 30–110, right tap x ≈ 350–446; no mirror change needed |
| Accelerometer | PASS: flat reads x -0.03, y 0.03, z +0.97 g (z positive face up); tilt changes x/y (signs to confirm in M4) |
| PSRAM free after boot | PASS: 8,289,996 bytes (Task 3 needs > 7,000,000) |
| Missing-image placeholder | PASS: magenta dot shown, one `missing image` log line |
| BOOT (GPIO0) | PASS on 3 s holds (`button HOME`, GPIO0 reads 0). The first round of brief presses registered nothing; recheck short presses in the menu |
| PWR | FAIL (hardware): no TCA9554 answers at 0x20 (or 0x38). Waveshare's factory Button Test shows PWR `READ ERROR` / "CHECK INPUT CONNECTIONS" on this unit |
| Microphone | FAIL (hardware): recordings are exact digital zeros on both channels (mono and stereo reads). Waveshare's SpecAnalyzer (BSP 3.0.1) and the factory SpecAnalyzer (TDM mode) are flat too |
| Speaker | FAIL: silent with mono and stereo output although samples reach the codec (`esp_codec_dev_write` OK, peak 22950). Not yet tested with factory firmware (needs a microSD card) |

I2C scan (BSP bus, GPIO14/15): 0x18 ES8311, 0x34 AXP2101, 0x40 ES7210, 0x5A CST9217 touch, 0x6B QMI8658.
Missing versus the schematic: 0x20 TCA9554 (U5) and 0x51 PCF85063 RTC (U4).

Conclusion at the time: M0 is blocked on hardware. **Superseded:** the board is a 1.75C, not a
defective 1.75; see "Board identification and diagnostics rerun" below.

## Cat gallery (Task 14) — 2026-09-23

| Check | Result |
| --- | --- |
| 15 expressions vs the character sheet | PASS: all match (also covers the Task 9 design comparison) |
| Costumes (BOOT cycles mouse, lion, robot, miner, none) | PASS: all sit correctly; 5 of 5 normal BOOT clicks registered |
| Happy cat idle animation (bob, blink, tail) | PASS |
| Cat variants and scales (PWR cycles them) | BLOCKED: PWR does not work on this unit (see Diagnostics) |
| FPS, happy cat | 25–30 FPS |
| FPS, dizzy | 7 FPS with the ±7° rotation (CPU 99%) → applied the spec §3.4 fallback (sideways wobble): 26–30 FPS, looks smooth |
| Missing images | none |

M2 passes except the variant/scale check, which needs PWR.

## Menu (Task 15) — 2026-09-23

| Check | Result |
| --- | --- |
| Layout: cat over the pink mic, two paws, chevrons, six dots, paw hint | PASS |
| Swipe left/right, wraparound, edge taps, active dot follows | PASS |
| Tap the cat; tap the icon (zoom, then the big placeholder cat); tap that cat; BOOT back to the same app | PASS visually; the name, laugh, meow and mew sounds are BLOCKED (speaker, see Diagnostics) |
| Rapid input: double tap, fast swipes, swipe then tap, BOOT during the zoom, mashing the edges | PASS: at most one app opened and the menu always recovered; the 15 min log has no Guru Meditation, abort or reset |
| Missing images | none |

The five apps other than Silly voices open the placeholder big cat until M4–M5 build them.

## Menu FPS (Task 15) — 2026-09-23

The first measurement failed: 10 swipes ran at 10–23 FPS (mostly 10–14, 80–93 ms render, CPU 98%),
and the user saw them as choppy. A temporary auto-swipe benchmark (not committed) isolated the
cause: LVGL's software renderer reads the RGB565A8 images and writes the draw buffers in PSRAM
through the default 16 KB instruction / 32 KB data caches, compiled at `-Og`.

| Benchmark (swipe avg FPS / render ms) | Result |
| --- | --- |
| Original (`-Og`) | 11.7 / 68 |
| `-O2` | 13.7 / 57 |
| `-O2` + 32 KB I-cache + 64 KB D-cache with 64 B lines | 23.9 / 32 |
| + XIP from PSRAM | 24.7 / 30 |
| + LVGL style cache (all four, now in `sdkconfig.defaults`) | 24.4 / 29 |
| Open zoom, 1 s slow zoom: 1.9x / 1.2x / 1.2x without antialias | 10.9 / 16.9 / 19.7 FPS |

Two draw units (`LV_DRAW_SW_DRAW_UNIT_CNT=2`) need LVGL's OS mode and were not tried.

The open zoom stayed below 24 FPS, so the spec §3.4 fallback is applied: `ICON_SCALE_OPEN` 307
(fade plus scale 1.0 → 1.2).

| Check on the final `build-perf` | Result |
| --- | --- |
| 10 swipes | PASS: 19–30 FPS, 26.3 average (the lowest single samples fall on the settle bounce); user: "smooth now" |
| Open zoom (1.2x fallback) | 14–20 FPS for the one sample covering the 220 ms zoom; below 24, but the user saw it as fine |
| Silly voices listen screen (pulse ring + countdown) | 25–30 FPS (was 10–12 at 98% CPU) |
| Idle menu | 28–30 FPS at ~21% CPU (was 26 FPS at ~54%) |

## Board identification and diagnostics rerun (Task 13) — 2026-09-23

The board is a **Waveshare ESP32-S3-Touch-AMOLED-1.75C**, not the 1.75 the plan assumed:

- esptool reports 32 MB flash. The 1.75 factory image is 16 MB; the 1.75C factory image is 32 MB.
- Waveshare's 1.75C factory image (`ESP32-S3-Touch-AMOLED-1.75C-FactoryOnly-260113.bin` from
  `waveshareteam/ESP32-S3-Touch-AMOLED-1.75C`, git blob `db765377`) plays sound, its mic screen
  reacts to voice, and PWR works: a click powers on, a ~7 s hold powers off.
- The 1.75 BSP drives the codec MCLK on GPIO42; the 1.75C wires it to GPIO16. Without MCLK the
  ES8311 and ES7210 accept their I2C setup but pass no audio (the zeros and silence above). The
  1.75C also has no TCA9554 I/O expander and no PCF85063 RTC, and its LCD and touch resets are
  GPIO1 and GPIO2.

The firmware now uses `waveshare/esp32_s3_touch_amoled_1_75c`. PWR is the AXP2101 PWRON key,
read from the PMU's latched short-press flag.

| Check (diagnostics build on the 1.75C BSP) | Result |
| --- | --- |
| Speaker (blips on button presses) | PASS |
| Buttons BOOT x2, PWR x1 | PWR PASS (POWER 1). BOOT: the first two presses counted once (HOME 1, one log line); two more presses one second apart counted 2 of 2. A quick second BOOT press can be missed; cause not yet found |
| Record "Bok, maco!" and play back | PASS: `recorded 59520 samples, heard=1`, the user heard their voice |
| PSRAM free after boot | 7,615,828 bytes (XIP from PSRAM now holds the code; still > 7,000,000) |
| Accelerometer, touch, placeholder dot | unchanged, PASS |

M0 passes, apart from the quick-double-press BOOT finding.

## Audio (Task 12) — 2026-09-23, 1.75C BSP

| Check | Result |
| --- | --- |
| `audio: ready: 24000 Hz mono` | PASS (now logs `volume 100%`, see below) |
| Boot meow and chime; BOOT mew in the menu; PWR blip in diagnostics | PASS |
| Volume | At the 60 % cap the user found every sound too quiet. The codec curve is −50 dB at 0 % and 0 dB at 100 %, so 60 % played at −20 dB. The user asked for full volume: the cap now defaults to 100 % (0 dB), and the spec's 80 % ceiling is lifted. At 100 %: "good now" |
| Missing-sound check (`meow_1.wav` moved away) | not run yet |

## Idle and boot (Task 16) — 2026-09-23

| Check | Result |
| --- | --- |
| Boot: two rings, a yawning cat, meow and chime, menu after ~2.6 s | PASS |
| BOOT held through boot does not count as a press | Not run on the device (the instructions were unclear to the user). Covered by the host test `test_button_held_at_boot_does_not_fire` |
| Dimming: dim and sleepy at ~20 s, zz at ~60 s, dark at ~120 s | PASS |
| Tap while dark only wakes; the second tap opens the app | PASS |
| PWR inside an app: screen dark; the waking tap does not make the cat laugh | PASS (PWR through the AXP2101) |
| App left alone returns to the menu after ~60 s | PASS |
| Home hint | PASS: the paw at 2 o'clock points at BOOT, so `HOME_HINT_ANGLE_DEG` stays −30 |

M1 passes.

## Silly voices (Task 17) — 2026-09-23

| Check | Result |
| --- | --- |
| Screen: pink dotted ring, mouse ears, pink mic | PASS |
| Tapping the cat cycles lion (orange), robot (cyan), miner with cave arch (purple), mouse, each with its sting and "Bok!" | PASS |
| Record and play in all four voices | First run: all four worked but were barely audible. Arm's-length speech peaks at only 1,700–3,600 (−19 to −25 dBFS), and the output was capped at −20 dB. Recordings are now normalized to the assets' −3 dBFS peak (at most +24 dB) and the volume is 100 %. Retest: clearly audible, mouse included |
| Stop early (second tap) | PASS |
| Silent take plays a meow, never silence | PASS (peak 157, `heard=0`) |
| Loud take (shout close to the board) | PASS: clean, raw peaks 5,900–10,400 |
| BOOT while listening; BOOT while playing | PASS: the menu appears at once, sound stops |
| Reopen and record | First run: inaudible, because reopening resets to the quietest costume, the mouse (`heard=1`, so it did play). After normalization: clearly audible |
| Log | no Guru Meditation, abort, assert or unexpected reboot |

M3 passes.

## Soak (Task 18) — 2026-09-23, 1.75C BSP, volume 100 %

| Run | Result |
| --- | --- |
| 1 | Restarted by itself after round 20; no panic text, and the ROM's reset line was lost over USB. The logger then reopened the port, and opening the port is itself shown to reset this board (`reset reason 11`, USB), so the logger is the likely cause. The firmware now logs `reset reason N` at every boot |
| 2 | All 50 rounds, no restart, but `SOAK: FAIL: internal delta=-25592`. Readings swung between ~198 KB and ~223 KB from round to round: a ~25 KB buffer held for a few ms during a display flush or fade, not a leak (PSRAM was flat) |
| 3 | After flashing and a scripted reset, the screen stayed black and the chip did not answer the flasher until the user replugged USB. Not reproduced since; cause unknown |
| 4 | Soak now takes the highest of 10 readings 50 ms apart. **`SOAK: PASS: internal delta=-8 psram delta=0 over 50 rounds`**, one boot, no crash, logged with a listener that does not touch the reset lines |

## Parent volume (2026-09-23)

| Check | Result |
| --- | --- |
| Diagnostics: one finger, then two held, then lift | PASS after a fix: first the count never passed 1 (the Waveshare CST9217 driver hard-codes one point); with the report read, `touch points=1`, `2`, then `0`, on 3 holds |
| Two fingers held still on the menu: nothing for 1 s, ring fills around the cat, Max volume opens at 5 s | PASS |
| "−" three times (100 → 70 %, quieter clicks), "+" once (80 %), BOOT back to the menu with the mew at the new level | PASS |
| Menu afterwards: swipes, cat tap, app open/close, dim at ~20 s and tap to wake | PASS, sounds at the new level (the new display setup and brightness path behave as before) |
| One finger on the cat 10 s; two fingers 3 s then lift; two fingers with one sliding ~2 cm | PASS: none opened it |
| Letting go after a completed hold | PASS: no app name, no app opened behind it |
| Untouched Max volume screen | PASS: back to the menu after ~60 s |
| Volume survives a restart | PASS: `settings: volume cap 50%` after reset (the level last set) |
| Log | one boot, no Guru Meditation, abort or assert |

## Review fixes and a boot hang (2026-09-24)

| Check | Result |
| --- | --- |
| Palm on the dimmed menu (under 1 s) wakes it; quick two-finger tap on the icon opens the app | PASS (the hold now takes the touch only once its ring shows) |
| Two-finger hold, "−"/"+" and BOOT after the fixes | PASS |
| New volume curve (10 % = −20 dB, 100 % = 0 dB) | Built in; the user keeps 10 % and did not judge its loudness |

**Boot hang after a double reset.** Sometimes, after a reset followed by a second one within about a second (a flash plus a serial-port open, or a scripted reset), the bootloader finishes and the app prints nothing. The screen stays black, and the chip does not answer the flasher until USB is replugged. A scripted test (`double_reset.py`: reset, wait 0.15–1.2 s, reset, look for the app's first line) reproduces it:

| Firmware | Result |
| --- | --- |
| Tonight's settings (`-O2`, 32/64 KB caches, XIP from PSRAM, style cache) | hang at cycle 21 (1 in 21) |
| Same without XIP from PSRAM | hang at cycle 20 (1 in 20) |
| None of tonight's settings (`-Og`, default caches, no XIP, no style cache) | hang at cycle 22 (1 in 22) |

So it predates tonight's changes and does not depend on those settings. It needs two resets close together during early boot; a normal power-on is a full power cycle and was never affected. Suspect: the chip (ESP32-S3 rev v0.2 with AP octal PSRAM; compare esp-idf issue 18806). Open: test with `CONFIG_SPIRAM_MEMTEST` off and with Waveshare's own firmware under the same script.

## Serial listener and accelerometer axes (M4) — 2026-09-24

**Listener.** Setting `dtr=False` and `rts=False` before `open()` still reset the board: the log showed
a boot cut off by `rst:0x15 (USB_UART_CHIP_RESET)` 3 s after the flasher's own reset. The chip is held
in reset while RTS is high and DTR low. macOS raises both lines on open, and pyserial then clears DTR
before RTS, passing through that state. `tools/serial_log.py` now skips pyserial's DTR update on open
(`dsrdtr=True`, which on POSIX does nothing else), drops RTS, then drops DTR. Result: it joined the
running diagnostics build mid-stream (first line at 75 s of uptime) with no reset.

**Axes.** Two rounds of pose instructions were unclear, so the dedicated check was dropped. Evidence
so far: flat, face up reads x −0.02, y 0.03, z +0.97; held in the hand facing the user, raw y read
−0.75 to −1.0 with z near 0, so raw y points toward the bottom (USB) edge like screen y. With raw z out
of the glass and a right-handed sensor, raw x then points left. The firmware starts with screen
x = −ax, screen y = ay. **Confirmed** in the Pet the cat walkthrough below: the ball rolls downhill
in all four directions.

## Pet the cat (M4) — 2026-09-24

Normal build 968e970, then the perf and soak builds of 476a3b5.

| Check | Result |
| --- | --- |
| Layout: big cat, fish button, no ball; opens within ~0.5 s | PASS |
| Tap: purr ~1.5 s, hearts, back to the plain face | PASS |
| Rub: giggle + "Hihihi", laughing face; a finger held still for 5 s never rubs (it purrs on release) | PASS |
| Fish: crunch crunch "Njam!", eating, ring at 50 %; rapid taps restart cleanly | PASS |
| Shake: "wheee" + boing, dizzy 2 s, 1.5 s lockout; walking and a light knock do nothing | PASS |
| Tilt from the grip: the ball rolls downhill in all four directions, with ticks and following eyes, and stays inside the rim (slides along it) | PASS (confirms the axis mapping) |
| Level for 3 s hides the ball; no ticks while the cat purrs | PASS |
| Tilt-only play for 90 s stays open; left tilted and untouched, back to the menu after ~60 s | PASS |
| PWR off: shake and tilt stay dark and silent; a shake on the menu does nothing | PASS |
| BOOT during purr, dizzy, eating and rolling: the menu is back at once and the purr stops | PASS |
| 5 quick double BOOT presses on the menu | PASS: 10 of 10 `button HOME` lines, 209–455 ms apart. The M0 finding (a quick second press can be missed) is not reproduced with the sensor queue |
| FPS (perf build) | PASS: rubbing, hearts, dizzy wobble, rolling ball and fish all 25–30 FPS; the user saw them as smooth. Below 24 only the boot animation right after the reset (10–22) and one sample each for the menu swipe settle (23) and the app-open zoom (14, the accepted §3.4 fallback) |
| Soak | PASS on the committed build: `SOAK: PASS: internal delta=-8 psram delta=0 over 50 rounds` (run 3, board untouched). Each step now shakes and tilts the open app before Home. Run 1 failed: free PSRAM held at 6,096,360 through round 40, then dropped once by 37,436 bytes by round 50. No button presses were logged, and the board may have been handled then. No sound or image file matches that size. An instrumented run 2 (untouched) logged every first-time sound or image load and free PSRAM after each of the 300 steps. PSRAM never changed after the warm-up and the run passed. Run 1's drop was not reproduced |
| Log | no missing image or sound, Guru Meditation, abort, assert or unexpected reset |

**Seen on the device:** opening Pet the cat with the toy lying flat and then picking it up makes the
ball appear at once, because flat became level at open (spec §3.10). Filed as issue #4 with fix
options, to decide after the kid test.

## Magic paws, Yarn roll and Meow piano (M5) — 2026-09-24

Normal build 55b0a34, then the perf builds of 55b0a34 and 5bb4e4a, the normal build of f3559a3
(final-review fixes), and the soak builds of 55b0a34, f3559a3 and 32e26f9.

| Check | Result |
| --- | --- |
| Menu: the surprised face (Magic paws icon) looks as before | PASS |
| Magic paws: layout, twinkling hint, rainbow trail with glow and sparkles, fast scribbles unbroken | PASS |
| Magic paws: paw brush (alternating, turned, pat), edge ring and button stay clean | PASS |
| Magic paws: drawing kept after Home; shake sweeps it (dim, broom, swish, "Čisto!", clean); Home mid-sweep leaves it clean | PASS |
| Magic paws (f3559a3): a stroke continues across the brush button; a press dragged off the button neither switches the brush nor paints; a tap still switches | PASS |
| Yarn roll: layout and 4 s hint; rolls downhill with rumble and turning frames; the cat follows and looks at the yarn | PASS |
| Yarn roll: win scene (full basket, proud cat, confetti, fanfare + "Bravo!"), three rounds through the basket spots | PASS |
| Yarn roll: shake recenters; resting against the rim stops the rumble and times out after ~60 s | PASS |
| Yarn roll (f3559a3): PWR while the yarn rolls silences the rumble at once; win and next round still work | PASS |
| Meow piano: five colored cats and pads; each tap sings, bounces, fills its pad and colors the note; fast taps never stick | PASS |
| BOOT mid-sweep, mid-roll, after the hit, during the confetti and mid-song: menu at once, silence | PASS |
| Tilt-only play in Yarn roll for 90 s stays open; Magic paws and Meow piano time out after ~60 s | PASS |
| PWR off in Yarn roll: tilt and shake stay dark and silent | PASS |
| FPS (perf build) | Magic paws scribbling 28-30 (rainbow and paws); sweep 24-30; Yarn rolling 29-30; app-open zoom 3-13 (the accepted §3.4 exception). Confetti with spin measured 13-17, so the spec §3.11 fallback dropped the spin (5bb4e4a): 24-26 over two full win scenes. **Open:** Meow piano measures 15-22 (a few samples to 9) while tapping cats quickly and 26-30 between taps; the user saw it as smooth and deferred it to M8. The first two perf logs recorded only the idle menu; the third captured the run |
| Soak | PASS on the committed build (32e26f9, silent): `SOAK: PASS: internal delta=-8 psram delta=0 over 50 rounds`. Earlier runs are in the investigation below |
| Log | no missing image or sound, Guru Meditation, abort, assert or unexpected reset |

**Soak investigation.** The first soak (55b0a34) failed on PSRAM −12,636 B: one drop between the
baseline and round 10, then flat. An instrumented run logging every image and sound load and the
free PSRAM after each of 300 steps stayed flat (PSRAM delta 0), but failed on internal RAM: its
baseline caught a rare high reading (243,671 B; run 1 had the same value once, at round 10), while every other reading sat
at 219-222 K with no trend. The final-review fixes made the soak warm up three rounds (the piano's
first open blocks the UI ~440 ms, so one round can miss animation frames that are then cached forever)
and compare median readings. The next run (f3559a3) still dropped exactly −12,636 B once, between
rounds 30 and 40. A silent run with heap hooks (a backtrace for any 12-13.5 KB PSRAM allocation after
the baseline) passed 120 rounds with PSRAM flat and no hook hit. **Open:** a one-time, non-growing
12,636 B PSRAM hold. It appeared in runs 1 and 3 (sound on, uninstrumented), not in run 2 (sound on,
instrumented) nor in the silent runs (120 and 50 rounds). Its source is not identified. Next instrument for M8: a table of live PSRAM allocations since the baseline, with
backtraces, printed at each checkpoint. At the user's request the soak now runs silent (32e26f9).

The kid test (spec §7) is still open for M3, M4 and M5.

## Cat gallery variants and scales (Task 14) — 2026-09-24

| Check | Result |
| --- | --- |
| PWR cycles the three cats (orange, grey, tuxedo) at M, L and S | PASS: all 9 look right, nothing clipped or missing (log shows every `scale=/variant=` combination) |
| Costumes at L and S (BOOT) | PASS: they sit right |

This closes the one M2 check that was blocked on PWR. M2 passes.

## Time and bedtime (M6) — 2026-09-24

Builds of 2bbf0a1: without Wi-Fi (`build-m6`, `SDKCONFIG_DEFAULTS="sdkconfig.defaults"`, so NTP could not reset the hand-set clock), soak, and perf. Also the Wi-Fi build (from `sdkconfig.secrets`), two instrumented NTP builds (not committed), and the NTP fix and 68eaf5c.

| Check | Result |
| --- | --- |
| A: menu two-finger hold (ring at 1 s, parent area at 5 s); PWR off/on; Home from apps with the mew | PASS |
| A: Sleepy clock: the sun rises, the rays turn, hills and the proud cat; a tap on the cat purrs, a tap on the sky does nothing | PASS |
| B: parent home ("Parents", Volume and Sleep tiles, "Home button exits"); Max volume with "‹ Parents" | PASS |
| B: Sleep times: Now shows "--:--" until the first press sets 12:00 (log); 1-minute steps, 10-minute after a 2 s hold (log); Bedtime and Wake up in 15-minute steps with repeat; 00:30 stays valid; the Sleep tile shows the saved bedtime | PASS |
| C1-C2: bedtime closes an app without the mew and shows the lock; bedtime over the menu; moving the times out of the night in the parent area goes to the menu without a sunrise | PASS (log: one `lock shown` per transition, `lock dropped` on the move) |
| C3: bedtime while the parent area is open shows the lock only on leaving | PASS, user-reported (not in the log) |
| D: on the lock: Home ignored; the whisper quiet but audible, shh face, 5 s gap; PWR off/on; USB night light past 30 s; on battery dark after 30 s and 5 s per touch; the hold ring around the cat, back to the lock after the parent area | PASS, user-reported (the log holds only the PWR and Home presses) |
| E: sunrise: the sun rises while the screen brightens, then the chime, the meow and "Dobro jutro!"; a tap only after the rise; Home at once; the menu by itself after 60 s | PASS, user-reported (not in the log) |
| F: a reset inside the night: the boot animation, then the lock | PASS (log: `lock shown` 5.2 s after boot) |
| FPS (perf build) | The lock: 30 FPS, render 7-8 ms, CPU 36-38 % (the twinkles redraw continuously; on battery the scene is hidden while dark). The Sleepy clock's turning rays: **not measured**, the user declined further hands-on tests |
| Soak | PASS (silent, 2bbf0a1): `SOAK: PASS: internal delta=-1032 psram delta=0 over 50 rounds`, with the real Sleepy clock |
| NTP | PASS after two fixes (6b23e71, 68eaf5c): the Wi-Fi build gets an IP 5.06 s after boot, the NTP answer 68 ms later, and the correct local time. A made-up network gives up at 5.2 s and the lock still shows. The first Wi-Fi build failed; see below |
| Log | no Guru Meditation, abort, assert or unexpected reset |

**Log gap.** The serial log holds none of the events of C3, D and E (no clock sets, no Home presses, no sunrise lines), and the user's answers came faster than those steps take. The user confirmed twice that everything behaved as described. Those rows are the user's report, not backed by the log. The listener was running throughout. One console drop is expected: unplugging USB in D6 cuts it off.

**NTP.** The first Wi-Fi build (2bbf0a1) joined the network (WPA2, RSSI −77 dBm) and got an IP at 5.4 s, but had no NTP answer by the 10 s deadline. With a temporary 40 s budget the answer came 30.2 s after the client started. lwIP waits a random 0-5 s before the first request and repeats an unanswered request only after 15 s, and here the first request goes unanswered. **Fix 6b23e71:** no startup delay (`CONFIG_LWIP_SNTP_STARTUP_DELAY` off), and `esp_netif_sntp_start()` resends every 2 s until the deadline. The debug build then synced 2 s after the start.

A second fault showed only in builds whose config was generated fresh from `sdkconfig.defaults`. `CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP` (Task 3) raises the Wi-Fi static RX buffers from 10 to 16 and the block-ack window from 6 to 16. Every fresh-config Wi-Fi run then dropped display frames while Wi-Fi was up: `Failed to allocate priv TX buffer`, then `Draw bitmap failed: ESP_ERR_NO_MEM`, 33-200 times per sync. The display needs a 46.6 KB internal DMA bounce buffer per flush. The long-lived `sdkconfig` kept 10/6 and never failed. **Fix 68eaf5c:** pin 10/6 in `sdkconfig.defaults`. A fresh config then logged 0 failures.

**Not measured:**
- Heap across repeated syncs: the before/after deltas at boot are dominated by the boot itself, and the daily sync at steady state was not run. This matters more with the retry: an invalid clock and an unreachable network mean up to 288 Wi-Fi start/release cycles a day.
- Audio during a sync: Wi-Fi shares core 0 with the audio task, and nobody listened for glitches.
- The daily sync 10 minutes before wake, and the retry every 5 minutes while the clock is not valid: neither was run on the device (the user declined further hands-on tests).

**Drift.** Not measured. This session's corrections (+213 ms, −488 ms) span resets and hand-set times, so they do not isolate the oscillator. While the chip runs, the clock follows the 40 MHz crystal (`CONFIG_ESP_TIME_FUNCS_USE_ESP_TIMER`). The daily sync (10 minutes before wake) logs `NTP: corrected by X ms, Y s after the previous sync`, which gives the drift directly on a toy left running on Wi-Fi. This is open for M7, where deep sleep runs on the RTC slow clock, which drifts far more.

**Final-review fixes.** After these checks, the whole-branch review changed the following, none of it re-checked by hand on the device (the user declined further hands-on tests); only host tests and the two builds (`build-m6`, `build`) cover it: the daily sync moved from the wake-up minute to 10 minutes before it; a failed sync while the clock is not valid retries every 5 minutes; a dropped Wi-Fi connection reconnects within one attempt instead of giving up; the lock's whisper moved from press to release; nightfall now cuts a touch in flight on the menu before showing the lock; PWR during the sunrise in `CONFIG_CAT_HOME_ON_POWER` builds ends it into the menu (lit) instead of turning the screen dark; and the Wi-Fi driver's log level is now WARN.

**Boot check after the fixes.** Not run: the board was not connected when the fixes landed (2026-09-25). The final firmware still has to be flashed; it then shows the boot sync and whether any `wifi:connected with` line is left in the log.

The kid test (spec §7) is still open for M3, M4, M5 and M6.

## Power diagnostics (M7) — 2026-09-25

The `powerdiag` build (4cc63e5 with the fix 885852c), on USB with the battery connected, logged with `tools/serial_log.py -t` (host timestamps; the logger survives the port dropping in deep sleep). A second, uncommitted variant ran the touch wake alone twice (the second time holding the touch reset, GPIO2, high through the sleep).

| Check | Result |
| --- | --- |
| Fuel gauge | Already on at start (`0x18` = `0x0a`, bit 3 set). 34 %, VBAT 3948 mV, VBUS good, battery present, charging (status 2 direction 1, charger state 2: constant current) |
| Touch INT (GPIO11) awake | 269 edges during round 0's touches: the CST9217 pulses INT while touched, without the host reading it |
| Touch wakes deep sleep | Yes: both touch tests woke by `ext1 pins 0x800` about a second after the screen went dark. Holding GPIO2 made no difference, so the plan needs no hold. The full diagnostic's round 2 woke only by its 5-minute fallback: the user was not sure they touched it. The same configuration there slept the full 300 s, so the chip does not wake by itself |
| BOOT wakes deep sleep | Yes: `ext1 pins 0x1`, at once |
| PWR on a free RTC pin | No: none of GPIO 3, 13, 17, 18, 21 followed 3 PWR presses. GPIO13 toggles on its own every 0.2-0.7 s (likely the panel's tearing-effect line). PWR cannot wake the chip; a long press still powers off |
| Timer wake | Yes: round 1 (timer only, no pins) woke by the timer |
| RTC drift in deep sleep | The 180 s timer took about 179.0 s of real time: the `sleeping` line at host 23:40:52.09 plus 0.1 s, the wake from the host time of the log lines minus their boot timestamps, 23:43:51.14. The RTC slow clock runs about **0.6 % fast, about 21 s per hour** (±0.1 s timing error, so ±0.06 %) |
| Panel and speaker while asleep | Not reported (the user did not note them) |

**What the drift means.** 21 s per hour is under the plan's 30 s/h limit: across an 11.5-hour night the timer fires about 4 minutes early, well inside the 10-minute lead, so the lead stays. The clock itself runs fast by the same amount while the chip sleeps. With Wi-Fi, the NTP sync at every deep-sleep wake (each wake is a boot) corrects it. Without Wi-Fi the clock gains about 4 minutes per night of deep sleep, so bedtime creeps earlier until the parent sets Now again. A steadier RTC source (the internal 8MD256 clock, at some cost in sleep current) is a candidate for M8's battery work.

## System and parent (M7) — 2026-09-26

Firmware 8231f53 (after the final review's fixes). Builds:
- `build-m7`: the normal build, no Wi-Fi;
- `build-drain`: USB counts as battery, plus a fake battery;
- `build-ntppower`: the `power` build plus the Wi-Fi secrets, used for group E and the drift;
- `build-perf`;
- `build-soak`.

Logs are in `build-logs/m7-*.log`, host-timestamped with `tools/serial_log.py -t`. They survive the port dropping in deep sleep and when USB is unplugged.

| Check | Result |
| --- | --- |
| A. First run | PASS (user): the picker after the boot animation with Orange's thick ring and tint; Grey chosen (proud face, meow), then the menu with the grey cat and grey paws; the icon breathes. The log of this step was lost (the first logger did not start). The flag survives a restart: the next flash's boot logs `settings: ... cat 1 ... first run done` and shows no picker. Home on the picker: not run (the user kept their settings, so NVS was not erased) |
| B. The parent area | PASS (user): the four tiles (Volume, Sleep, Apps "6 of 6", Cat "Grey"), the battery circle, "Home button exits", the Fredoka text. Show apps: a hidden app fades with a grey label and the crossed eye, the last app cannot be hidden, "1 of 6", and the menu shows only that app. The Cat page switches; the lock at night; a cat switch over the lock shows the new cat on the lock; Max volume and Sleep times work |
| B. Memory across cat choices | PASS (log): `free psram after the cat choice` 4895040 → 4905556 → 4895028 (the third is within 12 B of the first); each choice releases the other two variants (`released 35/9, 25/9, 9/25 images`) |
| C. Charging | PASS (user): no eating cat on the lock (the night light comes on) or in the parent area; on the menu the eating cat for 4 s; over Yarn roll the yarn stops and play goes on afterwards; a dim menu lights first; a finger rubbing Pet at the plug-in stops the purr, and it does not restart. The screens' own log lines fall inside the ~7 s the USB port needs to come back after a plug-in, so they are not in the log |
| C. Leaving the night by moving the times | PASS (user): setting Bedtime 19:30 / Wake 07:00 from the parent area over the lock lands on the menu, with no sunrise |
| D. Low battery (drain) | 10 % at 21.5 s: the hungry cat (log + user). **5 %: not shown**. The log has no screen at ~70 s, and the pending screen waits for a fully lit panel (`idle_level() == 0`), so the app had most likely dimmed. The spec now says "dimmed or off". 2 % at 100.9 s: `battery 2 %: sleeping until it is charged`, the hungry cat, `deep sleep: timer 0 s, for the battery`. A touch: `woke by a pin (slept for the battery)` → `start: hungry cat` → sleep again (log + user). The user: "everything is fine" |
| E. Deep sleep by day | PASS (log + user): every touch, BOOT and PWR-off wake logs `woke by a pin` → `start: boot animation`; `deep sleep: timer …` 20 s after the panel goes off; fully dark and silent while asleep; Magic paws' drawing is gone after a sleep; BOOT and touch work normally after a wake |
| E. The night | PASS (log): the lock at 09:08:21, dark, then `deep sleep: timer 647 s` (wake 09:30 − 10 min). At 09:19:54 `woke by the timer` → `start: lock, dark` (no boot animation) → NTP; the sunrise at 09:30:00, over at 09:31:04. **The touch-lit wake at night (`start: lock, lit`) was not run** |
| E. Draw failures at a wake | **1 of 3 day wakes** logged 28 `Draw bitmap failed: ESP_ERR_NO_MEM` (`Failed to allocate priv TX buffer`) 3.0-4.4 s after boot. Wi-Fi starts while the boot animation flushes full frames, and each flush needs a ~25 KB (up to 46 KB) internal DMA copy of the PSRAM draw buffer. This is the M6 contention (the RX buffers are pinned, and `rx ba win: 6` is in effect), now hit at every deep-sleep wake. It is visible as a skipped boot-animation frame; the user noticed nothing. A separate follow-up is on `davor/boot-wifi-flush` (the SPI panel IO without `psram_dma_direct`) |
| Drift (NTP) | **1.04 % fast, ~37 s per hour**: 647 s by the RTC took ~640.4 s (`NTP: corrected by -6640 ms` after the timer wake). Task 1 measured 0.6 % over 180 s. Short day sleeps (~2 min) corrected by −42 to −131 ms |
| F. The icon breathes | PASS (user, group A): it swells and shrinks slightly on the awake menu |
| FPS (perf) | The breathing menu: 27.7 FPS mean (20-30), 12 of 94 samples below 24 (the frames that redraw the scaled icon), CPU mean 40 % (21 % for the still menu in M5). It stops once the menu dims (20 s). The boot animation: 10-15 FPS, as before |
| Soak | PASS: internal −8 B, PSRAM 0 over 50 rounds (`m7-soak2.log`; every round reads internal 216083, PSRAM 4709828) |
| Dev flags | The normal build's binary holds none of the `dev build:` warnings (`grep -c` = 0); the drain and power builds hold 3 and 2 |
| Log | No Guru Meditation, abort, assert or unexpected reset |

**What the drift means.** 37 s per hour is over the plan's 30 s/h trigger for a longer lead. The trigger guards against a late timer, and both measurements run fast: the chip cools in deep sleep, so the timer fires early. Across an 11.5-hour night that is ~7 minutes early. With Wi-Fi, the sync at that wake corrects the clock, and the sunrise came on the minute. The lead stays at 10 minutes and the guard at 15. Without Wi-Fi the clock gains ~4-7 minutes per night; spec §3.2 says so.

**Soak, first run.** The first soak on 6e8feee failed on its last reading only (internal −20352; rounds 10-50 flat at 213111). Two traced runs (`m7-soak-trace1/2.log`) showed free internal RAM cycling between ~235.6, ~217 and ~190 KB on the idle menu and always returning. The breathing icon redraws about a third of the time, each flush holds ~25 KB, and the scaled icon holds ~18.5 KB. Under the display lock the dips vanish. It is not a leak. The soak now pauses the breathing while it measures (10ff444).

**Port handling.** Opening the port right after an esptool RTS reset left the Mac without the port until the user replugged USB. `esptool chip_id` did not reset the chip either (its uptime went on). `tools/flash_and_log.sh`'s 3 s wait avoids both. A board asleep for the battery was reflashed by catching it awake after a touch; the first attempt lost the port mid-connect and only reset it.

**Not tested on the device:**
- Home on the first-run picker, which needs NVS erased (the user declined).
- The touch-lit wake at night.
- The 5 % screen over an app (see D).
- USB plugged in during the 2 % screen. In the drain build USB counts as battery, so it cannot be tried there.
- The no-sunrise-after-a-correction fix at bedtime, which needs the clock ahead at a touch wake just before bedtime.
- The deep-sleep current, with the GPIO46 hold (which holds every digital pad; for M8).
- The quiet hungry cat at night.

The kid test (spec §7) is still open for M3-M7.

## Frame drops at the boot sync (after M7) — 2026-09-26

Branch `davor/boot-wifi-flush` from bf704a0. The fix is 738eb6c, and 4eb464f adds an auto-wake dev build. Logs are in `build-logs/` of that worktree (`dmadiag-base`, `fixdiag`, `fixdiag2-boot`, `autowake-base`, `autowake-fix`, `ntppower-fix2`, `soak-fix`, `perf-fix`). They are Wi-Fi logs: grep them only with `connected with|ssid|bssid` excluded.

**Cause.** With the LVGL draw buffers in PSRAM, `spi_master` copies each SPI transaction of a flush into a new internal DMA buffer (`setup_dma_priv_buffer`). It holds that buffer until the next flush.
- A transaction is at most 32 KB (`SPI_LL_DMA_MAX_BIT_LEN`), so a full 466 × 50 band needs 32,768 + 13,832 bytes. Earlier notes (M6, M7) give the copy as one 46.6 KB buffer.
- The boot rings flush full bands from 2.4 s to about 4.5 s, and Wi-Fi starts at about 2.6 s.

An instrumented build (not committed) added a failed-allocation hook and a 50 ms sampler of `heap_caps_get_largest_free_block(MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL)`. It gave these numbers on a touch wake of the Wi-Fi power build:

| Moment (ms since boot) | Largest internal DMA block (B) | Free internal (B) |
| --- | --- | --- |
| Start | 180,224 | 245,915 |
| Before `esp_wifi_init` (1966) | 73,728 | 118,439 |
| After `esp_wifi_init` (1973) | **31,744** | 69,235 |
| At each failed 32,768 B copy (2581-3238) | 31,744 | 111,675-114,455 |
| After Wi-Fi is released (3624) | 43,008, later 77,824 | 145,711 |

Wi-Fi's allocations split the last block larger than 32 KB. The RAM is there but fragmented, and the block is 1 KB short, so small layout changes decide each wake: 1 of 3 wakes in M7, all 4 boots of the instrumented build, and 6 of 11 wakes on the auto-wake build below.

**First fix, rejected: direct PSRAM DMA.** `psram_dma_direct` on the panel IO lets the SPI DMA read PSRAM with no copy.
- The first flush logged `spi_master: DMA TX underflow detected`, then `recycle spi transactions failed`, and the display hung. esp_lcd's error path leaves its in-flight count wrong, so the next command waits forever.
- IDF's own test runs PSRAM DMA at `SPIRAM_SPEED / 4` on one line; the panel needs 40 MHz × 4 lines. Not committed.

**Fix (738eb6c).** The draw buffers are in internal RAM: two 24-row buffers (44,736 B), allocated at boot before Wi-Fi. The SPI DMA reads them in place, so no flush allocates anything.

**Auto-wake build (4eb464f).** A timer wake by day shows the sunrise, not the boot animation, so only a touch reproduced the drops. `sdkconfig.autowake` (with `power`) makes every deep sleep last 10 s and counts its timer wake as a touch. Each wake plays the boot animation and runs the boot NTP sync, and the menu's panel goes off 15 s after start. One cycle is about 48 s, with no hands.

| Check | Result |
| --- | --- |
| Wake check, old display code (bf704a0's `display.c`), Wi-Fi power build + `autowake` | 11 wakes: 6 dropped frames, 115 `Draw bitmap failed` in all (15-28 per wake), 0 underflows, NTP 11 of 11 (`autowake-base.log`). The check catches the fault |
| Wake check, fix (4eb464f), same build | **PASS: 13 deep-sleep wakes plus the cold boot, 0 `Draw bitmap failed`, 0 `Failed to allocate priv TX buffer`, 0 underflows, no error lines, NTP 14 of 14** (`autowake-fix.log`; every boot's ELF SHA256 starts `9caff86f4`, as the build's) |
| Touch wake, plain Wi-Fi power build (738eb6c) | 1 logged touch wake: 0 failures, NTP synced (`ntppower-fix2.log`). The user's 10 touch wakes before it were not logged: the logger had stopped. The user saw nothing wrong on screen |
| Internal RAM, fix against the old code (auto-wake builds) | Free internal after the sync: median 133,111 B (old 131,255). Lowest while Wi-Fi was up: 65,879 B (instrumented boot, marks and 50 ms samples). Free PSRAM at boot: 93,056 B more |
| Soak (`build-soak`, 4eb464f) | PASS: internal −8 B, PSRAM 0 over 50 rounds; every round reads internal 194,887, PSRAM 4,803,036. M7 (`m7-soak2.log`): −8, 0; 216,083 and 4,709,828. The draw buffers moved 44.7 KB from PSRAM to internal RAM. The other 23.5 KB of the difference fits a flush copy still held when M7 took its readings |
| FPS (`build-perf`, 4eb464f, `tools/flash_and_log.sh 30` as in M7) | The breathing menu: 27.8 FPS mean (17-30, 12 of 94 below 24), CPU 37 %, flush 1.7 ms. M7: 27.7 (20-30, 12 below 24), CPU 40 %, flush 2.5 ms. The 17 is the first sample after the boot animation (86 % CPU); M7's first menu sample was similar (22 FPS at 92 %). The boot animation: 16/12/10/9 FPS (M7 15/12/10/10) |
| Log | No Guru Meditation, abort, assert or unexpected reset in any run |

**Not measured:** a sync during play (the daily sync, or the 5-minute retry while the clock is not valid). No flush allocates any more, so a sync can no longer take a flush's memory.
