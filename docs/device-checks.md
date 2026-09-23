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
- [ ] Dizzy state FPS check: if FPS < 24, the `translate_x` fallback in `apply()` should engage
      (this is a device-conditional fallback already coded; just confirm it triggers correctly).
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
- [ ] Flash `build-perf`; do 10 swipes plus 1 app open; confirm FPS >= 24. If not, set
      `ICON_SCALE_OPEN` to 307 (device-conditional fallback already coded) and retest.
- [ ] Record "## Menu FPS (Task 15)".
- [ ] Re-flash the normal build afterward.

### Task 16: idle, dimming, and boot (M1 gate)

- [ ] From Task 2's recorded BOOT clock position (or PWR's, if `CAT_HOME_ON_POWER` will be
      enabled), compute `HOME_HINT_ANGLE_DEG = hour*30-90` and set it in the sdkconfig. If BOOT is
      not reachable through the case (Task 2 finding), append `CONFIG_CAT_HOME_ON_POWER=y` to
      `sdkconfig.defaults` and do a full clean build.
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
