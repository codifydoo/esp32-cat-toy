# M4 Pet the Cat Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The second app, Pet the cat. Tapping the big cat makes it purr and rubbing makes it giggle. Shaking the toy makes it dizzy. Tilting rolls a ball that the cat's eyes follow, and the fish button feeds it.

**Architecture:**
- **Host-tested logic (`components/logic`):**
  - rub detection in `gestures.c`;
  - a new `motion.c` with shake detection, grip-relative tilt angles, the tilt speed curve and a ball step.
- **Sensors task:** it now reads the IMU at 50 Hz. It never waits for the LVGL lock: button and shake events go into a FreeRTOS queue, and the latest tilt sits behind a spinlock.
- **App manager:** a 20 ms LVGL timer drains the queue, handles buttons as before, and sends shake and tilt to the open app. At app open it takes the current grip as level.
- **The app:** `components/ui/apps/pet.c`, one file plus its registry line (the registry already lists `app_pet`; the placeholder line goes).

**Tech Stack:** ESP-IDF v5.5.5 (C11), LVGL 9.4, `waveshare/esp32_s3_touch_amoled_1_75c` 3.0.0, `waveshare/qmi8658` (IMU), CMake + Unity host tests (now under UBSan), Node + resvg for images, sox for sounds.

**Spec:** `docs/superpowers/specs/2026-09-22-cat-toy-design.md`, mainly:
- §3.9: the pupils;
- §3.10: Pet the cat details, the decisions this plan adds;
- §4.4: the app contract;
- §4.5: threads;
- §4.7: IMU failure;
- §7: shake and tilt thresholds.

The design is the UI source of truth:
- `design/claude-design/Screen.dc.html`, the `pet` view (layout, hearts, ball, fish button);
- `Main.dc.html` `act('petTap' | 'rub' | 'fish')`, `shake()` and `tilt()` (timings and sounds);
- `Spec.dc.html` "Gestures and sensors", "Motion" and "Sound map".

## Global Constraints

- **Toolchain:** ESP-IDF v5.5.5, target `esp32s3`.
  - Run every `idf.py` command in a shell that has sourced `~/esp/esp-idf/export.sh`. The port is `/dev/cu.usbmodem101`.
  - After adding a `.c` file to a component, reconfigure that build directory (`idf.py reconfigure`, or `-B <dir> ... reconfigure`): sources are globbed at configure time.
- **Build variants:** each is `idf.py -B build-X -D SDKCONFIG=build-X/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.X" build`. They are `diag`, `soak` and `perf`, plus `gallery`, which uses `"sdkconfig.defaults;sdkconfig.gallery;sdkconfig.perf"`. The normal build is `idf.py build`.
- **Board quirks** (`docs/device-checks.md`):
  - opening the serial port resets the board;
  - a second reset within ~1 s of the first hangs early boot about 1 time in 20.

  Always flash with `tools/flash_and_log.sh`. After Task 1 it waits 3 s and listens passively. Flashing rewrites the 12 MB assets partition (~60 s).
- **Gestures and sensors (spec §7 and design `Spec.dc.html`), verbatim:**
  - "Tap: down and up within 20 px of movement. Fires on release."
  - "Rub: Pressed on the cat and 150 px or more of total finger travel. Giggle fires once per press."
  - "Shake: Acceleration above 2.2 g for 3 samples within 300 ms, then a 1.5 s lockout."
  - "Tilt: 8° dead zone. Roll speed grows with angle up to 25°, capped at 240 px/s."
- **Spec §3.10 (added for M4), summarized:**
  - level = the grip when the app opens;
  - the ball starts at (330, 110), stays within r 190 and hides 3 s after the tilt ends;
  - one bell tick per 60 px, only while no reaction plays;
  - the pupils follow the ball only on the plain face;
  - reactions: purr 1.8 s (loop 1.5 s), giggle 1.8 s, dizzy 2 s, eat 2.4 s, and a new one replaces the current one;
  - rub counts steps of 10 px or more;
  - rolling and shaking count as activity; a ball resting at the rim does not;
  - the cat is drawn at scale L, centered on (233, 159).
- **Pet screen geometry (design, `Screen.dc.html` pet view):**
  - the cat hit area is a 230 px circle centered at (233, 155);
  - the fish button is a 150 px target centered at (233, 373). Its circle is r 68, fill `#3FB6FF`, 10 px stroke `#0F5A85`, and drops to 50 % while eating;
  - the ball is 48 px, fill `#FFD84A`, stroke `#9A7A00`;
  - hearts are 34 px `#FF7FB0`, top-left at (96, 70), (336, 60), (350, 190) and (80, 200), delayed 0, 350, 700 and 1000 ms. They loop over 1.2 s, rising from +10 to −44 px, fading in over the first 30 % and out over the rest.
- **Sounds (design "Sound map"):**
  - Tap: purr loop 1.5 s.
  - Rub: giggle, then "Hihihi!".
  - Shake: "wheee" and boing.
  - Tilt: tiny bell ticks.
  - Fish: crunch crunch, then "Njam!".
  - Every sound goes through `audio.h`, so the volume cap applies (spec success criterion 5).
- **Home:** BOOT returns to the menu from every screen within 300 ms. `app_manager` handles it and calls `audio_stop_all()` after `destroy()`.
- **`app_t` contract** (`components/ui/include/app.h`):
  - `destroy()` leaves no timers, animations or audio behind;
  - every callback the app registered checks that the app is still open and no-ops otherwise.
- **Text:** child screens have no text.
- **Host tests:** `sh test/run.sh` must pass before any commit that touches `components/logic/`.
- **Commits:** `feat:`, `fix:`, `test:`, `chore:`, `docs:` prefixes, no attribution trailers.
- **Human steps** are marked **Human step**. Ask with AskUserQuestion and put the full instructions inside the question text: the user may not see the message around it. Never guess a result that needs eyes or hands on the board.

## Review Focus

1. **A finger resting still on the cat.** A 5-year-old often presses and holds, and touch jitter must never add up to a giggle. The same press must still count as a tap when it lifts. Task 2 pins jitter from the worst starting point, for 400 samples.
2. **A toy left tilted on a table or sofa** must not keep the app open and bright forever (burn-in, battery). A ball pinned against the rim reports no movement, so the app does not count it as activity. Task 4 pins "pushed into the rim moves 0". Task 9 checks the 60 s return on the device.
3. **A single bump** (a knock, setting the toy down, one hard tap) must neither roll the ball nor make the cat dizzy. Task 4 pins that one 30° sample stays inside the dead zone. Task 3 pins that two strong samples, or three spread over more than 300 ms, are not a shake.
4. **Home or the 60 s timeout mid-reaction** must leave nothing running (the purr loop, the reaction timers, the heart animations). Task 7's `destroy()` cancels them. Task 8's soak shakes every app that has `on_shake` just before Home, 50 times, with heap deltas within ±4 KB. Task 9 checks on the device that BOOT stops the purr at once.
5. **Shake and tilt while the screen is dark (PWR) or on the menu** must do nothing: no sound, no wake. Task 5 gates both on `idle_level() == 0` and an open app. Task 9 checks it on the device.

---

### Task 1: Passive serial listener and the accelerometer axes

The IMU's axes on this board have never been mapped to the screen. Task 5 needs that mapping, and it needs eyes and hands on the board, so it comes first. The listener fix protects every later flash from the double-reset boot hang.

**Files:**
- Modify: `tools/serial_log.py`
- Modify: `tools/flash_and_log.sh`
- Modify: `docs/device-checks.md` (append)

**Interfaces:**
- Consumes: nothing.
- Produces: the mapping from raw QMI8658 axes to screen axes, recorded under "## Accelerometer axes (M4)" in `docs/device-checks.md` as two lines: `screen x = <expr>` and `screen y = <expr>`, where `<expr>` is one of `ax`, `-ax`, `ay`, `-ay`. Task 5 copies them into `SCREEN_X` and `SCREEN_Y`.

- [ ] **Step 1: Make the serial listener passive**

Replace `open_port` in `tools/serial_log.py` with:

```python
def open_port(port, deadline):
    while True:
        try:
            s = serial.Serial()
            s.port = port
            s.baudrate = 115200
            s.timeout = 0.2
            # Set before open(): driving DTR/RTS on open resets this board (reset reason 11), and a
            # second reset within ~1 s of the flasher's can hang early boot (docs/device-checks.md).
            s.dtr = False
            s.rts = False
            s.open()
            return s
        except serial.SerialException:
            if time.time() > deadline:
                raise
            time.sleep(0.2)
```

- [ ] **Step 2: Wait 3 s after flashing before listening**

In `tools/flash_and_log.sh`, replace the last line with:

```sh
# Let the flasher's own reset finish booting before the port opens (see serial_log.py).
sleep 3
python "$(dirname "$0")/serial_log.py" "$PORT" "$SECS"
```

- [ ] **Step 3: Build the diagnostics variant**

Run:
```bash
. ~/esp/esp-idf/export.sh
idf.py -B build-diag -D SDKCONFIG=build-diag/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.diag" build
```
Expected: `Project build complete`. If `assets/` is missing, run `tools/build_assets.sh` first.

- [ ] **Step 4: Flash and start logging in the background**

Run in the background (Bash `run_in_background`), 150 s of log:
```bash
mkdir -p build-logs
tools/flash_and_log.sh 150 -B build-diag -D SDKCONFIG=build-diag/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.diag" > build-logs/axes.log 2>&1
```
Wait until `build-logs/axes.log` shows `accel x=` lines. The diagnostics screen logs the accelerometer once per second.

- [ ] **Step 5: Human step, the three poses**

Ask (AskUserQuestion, header "IMU axes"):

> "The board is running the diagnostics screen and logging the accelerometer once per second. Hold the board with the USB port at the bottom and the screen facing up, then do these three poses, about 8 seconds each, in this order:
> 1. Flat on the table.
> 2. Keep the left edge on the table and lift the right edge until the board leans at roughly 45°.
> 3. Keep the top edge on the table and lift the bottom (USB) edge to roughly 45°.
>
> Then lay it flat again. Did you complete all three poses?"

Options: "Done, all three" / "Something went wrong (I'll describe it)".

- [ ] **Step 6: Read the mapping from the log**

Run: `grep "accel x=" build-logs/axes.log`

Expected: three plateaus.
- **Flat:** `z ≈ +1`, `x ≈ 0`, `y ≈ 0`.
- **Right edge up:** one of `x`/`y` reads about ±0.7 while `z ≈ +0.7`.
- **Bottom edge up:** the other axis reads about ±0.7.

The accelerometer reads +1 g along whichever direction points up. Raising the right edge makes screen x point partly up, so screen x is the raw axis that moved in pose 2, with its sign:

| Pose 2 (right edge up) shows | screen x = |
| --- | --- |
| x ≈ +0.7 | `ax` |
| x ≈ −0.7 | `-ax` |
| y ≈ +0.7 | `ay` |
| y ≈ −0.7 | `-ay` |

Screen y points toward the bottom (USB) edge, so pose 3 gives screen y by the same table. If both poses moved the same raw axis, or `z` went negative, the poses were done differently from the instructions: repeat Steps 4–5.

- [ ] **Step 7: Record and commit**

Append to `docs/device-checks.md`, with the observed values filled in:

```markdown
## Accelerometer axes (M4) — 2026-09-24

Diagnostics build, board held with the USB port at the bottom; the accelerometer reads +1 g along
whichever direction points up.

| Pose | x | y | z |
| --- | --- | --- | --- |
| Flat, face up | … | … | … |
| Right edge raised ~45° | … | … | … |
| Bottom (USB) edge raised ~45° | … | … | … |

screen x = <expr>
screen y = <expr>
```

```bash
git add tools/serial_log.py tools/flash_and_log.sh docs/device-checks.md
git commit -m "chore: passive serial listener and the accelerometer axes"
```

---

### Task 2: Rub gesture, saturating distances, UBSan host tests

**Files:**
- Modify: `test/CMakeLists.txt`
- Modify: `components/logic/include/gestures.h`
- Modify: `components/logic/gestures.c`
- Test: `test/test_gestures.c`

**Interfaces:**
- Consumes: the existing `gesture_track_t`, `gesture_begin/move/end`, `GESTURE_TAP`.
- Produces:
  ```c
  #define GESTURE_RUB_PX      150u
  #define GESTURE_RUB_STEP_PX 10
  typedef struct { gesture_track_t tap; int16_t ax, ay; uint32_t travel; bool fired; } rub_track_t;
  void rub_begin(rub_track_t *r, int16_t x, int16_t y, uint32_t t_ms);
  bool rub_move(rub_track_t *r, int16_t x, int16_t y);                 /* true once per press */
  bool rub_end(rub_track_t *r, int16_t x, int16_t y, uint32_t t_ms);   /* true if it was a tap */
  ```

- [ ] **Step 1: Run the host tests under UBSan**

In `test/CMakeLists.txt`, after the line `add_compile_options(-Wall -Wextra -Werror)`, add:

```cmake
# Undefined behaviour (e.g. signed overflow) aborts the test instead of passing by luck.
add_compile_options(-fsanitize=undefined -fno-sanitize-recover=undefined)
add_link_options(-fsanitize=undefined)
```

Run: `sh test/run.sh`
Expected: all existing tests still PASS. If one now fails with a `runtime error:` line, that module has real undefined behaviour. Fix it in this task, using superpowers:systematic-debugging, and add a line about it to the commit message. Also run `tools/build_sounds/build_sounds.sh`, since `fxcli` is now built with UBSan: it must finish with `Built 61 sounds`.

- [ ] **Step 2: Write the failing tests**

Append above `int main(void)` in `test/test_gestures.c`:

```c
static void test_far_apart_points_do_not_overflow(void)
{
    /* int16 coordinates can be 65535 apart, and 65535 squared overflows int32 (UBSan aborts). */
    gesture_track_t g;
    gesture_begin(&g, -32768, -32768, 0);
    TEST_ASSERT_EQUAL(GESTURE_NONE, gesture_end(&g, 32767, 32767, 100));
}

static void test_hold_with_far_apart_points_does_not_overflow(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p;
    two(&h, -32768, -32768, 32767, 32767, 0, &p);
    /* the same two points reported in the other order: not a move */
    TEST_ASSERT_EQUAL(HOLD_WAITING, two(&h, 32767, 32767, -32768, -32768, 100, &p));
}

/* A finger rubbing along y = 150, starting at x = 200, reported every 2 px. */
typedef struct {
    rub_track_t r;
    int16_t x;
    int fired;
} rubber_t;

static void rub_start(rubber_t *f)
{
    f->x = 200;
    f->fired = 0;
    rub_begin(&f->r, 200, 150, 0);
}

/* One stroke of len px to the right (dir 1) or left (dir -1). */
static void stroke(rubber_t *f, int dir, int len)
{
    for (int i = 0; i < len / 2; i++) {
        f->x = (int16_t)(f->x + 2 * dir);
        f->fired += rub_move(&f->r, f->x, 150);
    }
}

static void test_a_still_press_is_a_tap(void)
{
    rub_track_t r;
    rub_begin(&r, 233, 150, 0);
    TEST_ASSERT_FALSE(rub_move(&r, 236, 152));
    TEST_ASSERT_TRUE(rub_end(&r, 236, 152, 120));
}

static void test_rub_fires_once_after_150px_of_travel(void)
{
    rubber_t f;
    rub_start(&f);
    stroke(&f, 1, 40);
    stroke(&f, -1, 40);
    stroke(&f, 1, 40); /* 120 px */
    TEST_ASSERT_EQUAL(0, f.fired);
    stroke(&f, -1, 40); /* 160 px */
    TEST_ASSERT_EQUAL(1, f.fired);
    stroke(&f, 1, 40);
    stroke(&f, -1, 40);
    TEST_ASSERT_EQUAL(1, f.fired);                    /* once per press */
    TEST_ASSERT_FALSE(rub_end(&f.r, f.x, 150, 3000)); /* and a rub is never also a tap */
}

static void test_short_quick_strokes_still_rub(void)
{
    /* 20 px scratches: each counts 20 px in two 10 px steps */
    rubber_t f;
    rub_start(&f);
    for (int i = 0; i < 12; i++) {
        stroke(&f, i % 2 ? -1 : 1, 20);
    }
    TEST_ASSERT_EQUAL(1, f.fired);
}

static void test_a_short_stroke_is_neither_rub_nor_tap(void)
{
    rubber_t f;
    rub_start(&f);
    stroke(&f, 1, 100);
    TEST_ASSERT_EQUAL(0, f.fired);
    TEST_ASSERT_FALSE(rub_end(&f.r, f.x, 150, 500));
}

static void test_a_still_finger_never_rubs(void)
{
    /* Touch jitter of up to 3 px under a finger that does not move, starting from the worst
     * corner: two opposite corners are 8.5 px apart, still under the 10 px step. */
    static const int8_t J[][2] = {{3, -3}, {-3, 3}, {3, 3}, {-3, -3}, {0, 3}, {3, 0}, {-3, 0}, {0, -3}};
    rub_track_t r;
    rub_begin(&r, 236, 147, 0);
    int fired = 0;
    for (int i = 0; i < 400; i++) {
        fired += rub_move(&r, (int16_t)(233 + J[i % 8][0]), (int16_t)(150 + J[i % 8][1]));
    }
    TEST_ASSERT_EQUAL(0, fired);
    TEST_ASSERT_TRUE(rub_end(&r, 233, 150, 8000)); /* a long still press is a tap */
}

static void test_each_press_can_rub_again(void)
{
    rubber_t f;
    rub_start(&f);
    stroke(&f, 1, 160);
    TEST_ASSERT_EQUAL(1, f.fired);
    rub_end(&f.r, f.x, 150, 1000);
    rub_start(&f);
    stroke(&f, 1, 160);
    TEST_ASSERT_EQUAL(1, f.fired);
}

static void test_rub_without_begin_does_nothing(void)
{
    rub_track_t r = {0};
    TEST_ASSERT_FALSE(rub_move(&r, 300, 300));
    TEST_ASSERT_FALSE(rub_end(&r, 300, 300, 10));
}
```

And add to `main()` before `return UNITY_END();`:

```c
    RUN_TEST(test_far_apart_points_do_not_overflow);
    RUN_TEST(test_hold_with_far_apart_points_does_not_overflow);
    RUN_TEST(test_a_still_press_is_a_tap);
    RUN_TEST(test_rub_fires_once_after_150px_of_travel);
    RUN_TEST(test_short_quick_strokes_still_rub);
    RUN_TEST(test_a_short_stroke_is_neither_rub_nor_tap);
    RUN_TEST(test_a_still_finger_never_rubs);
    RUN_TEST(test_each_press_can_rub_again);
    RUN_TEST(test_rub_without_begin_does_nothing);
```

- [ ] **Step 3: Run to verify they fail**

Run: `sh test/run.sh -R gestures`
Expected: the build FAILS with `rub_track_t` / `rub_begin` undeclared.

- [ ] **Step 4: Declare the rub tracker**

Append to `components/logic/include/gestures.h`:

```c
/* Rub (design "Gestures and sensors"): pressed on the cat and 150 px or more of total finger
 * travel; fires once per press. Travel counts in steps of at least 10 px (spec §3.10), so touch
 * jitter under a finger held still never adds up to a rub. */
#define GESTURE_RUB_PX      150u
#define GESTURE_RUB_STEP_PX 10

typedef struct {
    gesture_track_t tap; /* the press as a whole, for the tap test */
    int16_t ax, ay;      /* where the last counted step ended */
    uint32_t travel;     /* px */
    bool fired;
} rub_track_t;

void rub_begin(rub_track_t *r, int16_t x, int16_t y, uint32_t t_ms);
/* True once per press, on the sample that brings the travel to GESTURE_RUB_PX. */
bool rub_move(rub_track_t *r, int16_t x, int16_t y);
/* True if the press was a tap (moved at most GESTURE_TAP_MAX_PX and never rubbed). */
bool rub_end(rub_track_t *r, int16_t x, int16_t y, uint32_t t_ms);
```

- [ ] **Step 5: Implement it, with saturating distances**

In `components/logic/gestures.c`:

1. Add `#include <math.h>` below `#include <stdlib.h>`.
2. Move the squared-distance helper to the top of the file, below the includes, as:

```c
/* Squared distance, saturating: int16 points can be 65535 apart, and 65535 squared overflows int32. */
static uint32_t dist2(int32_t dx, int32_t dy)
{
    uint64_t d2 = (uint64_t)((int64_t)dx * dx) + (uint64_t)((int64_t)dy * dy);
    return d2 > UINT32_MAX ? UINT32_MAX : (uint32_t)d2;
}
```

3. Replace the body of `track()` with:

```c
    uint32_t d2 = dist2(x - g->x0, y - g->y0);
    if (d2 > g->max_dist2) {
        g->max_dist2 = d2;
    }
```

4. Delete the old `static uint32_t dist2(int16_t ax, int16_t ay, int16_t bx, int16_t by)` below `hold_reset()`. In `hold_update()`, replace the two lines that compute `same` and `swapped` with:

```c
    uint32_t same = max2(dist2(x[0] - h->x0[0], y[0] - h->y0[0]), dist2(x[1] - h->x0[1], y[1] - h->y0[1]));
    uint32_t swapped = max2(dist2(x[0] - h->x0[1], y[0] - h->y0[1]), dist2(x[1] - h->x0[0], y[1] - h->y0[0]));
```

5. Append the rub tracker:

```c
void rub_begin(rub_track_t *r, int16_t x, int16_t y, uint32_t t_ms)
{
    gesture_begin(&r->tap, x, y, t_ms);
    r->ax = x;
    r->ay = y;
    r->travel = 0;
    r->fired = false;
}

bool rub_move(rub_track_t *r, int16_t x, int16_t y)
{
    if (!r->tap.active) {
        return false;
    }
    gesture_move(&r->tap, x, y);
    uint32_t d2 = dist2(x - r->ax, y - r->ay);
    if (d2 < (uint32_t)(GESTURE_RUB_STEP_PX * GESTURE_RUB_STEP_PX)) {
        return false; /* jitter, or a step still in progress */
    }
    r->travel += (uint32_t)lroundf(sqrtf((float)d2));
    r->ax = x;
    r->ay = y;
    if (!r->fired && r->travel >= GESTURE_RUB_PX) {
        r->fired = true;
        return true;
    }
    return false;
}

bool rub_end(rub_track_t *r, int16_t x, int16_t y, uint32_t t_ms)
{
    if (!r->tap.active) {
        return false;
    }
    return gesture_end(&r->tap, x, y, t_ms) == GESTURE_TAP && !r->fired;
}
```

- [ ] **Step 6: Run the tests**

Run: `sh test/run.sh`
Expected: every test PASSES, `test_gestures` included, and no `runtime error:` lines.

- [ ] **Step 7: Commit**

```bash
git add test/CMakeLists.txt test/test_gestures.c components/logic/include/gestures.h components/logic/gestures.c
git commit -m "feat: rub gesture; saturating touch distances; host tests under UBSan"
```

---

### Task 3: Shake detector

**Files:**
- Create: `components/logic/include/motion.h`
- Create: `components/logic/motion.c`
- Test: `test/test_motion.c`

**Interfaces:**
- Consumes: nothing.
- Produces:
  ```c
  #define SHAKE_G 2.2f
  #define SHAKE_SAMPLES 3
  #define SHAKE_WINDOW_MS 300u
  #define SHAKE_LOCKOUT_MS 1500u
  typedef struct { uint32_t hit_ms[SHAKE_SAMPLES]; uint8_t hits; bool locked; uint32_t locked_ms; } shake_t;
  void shake_reset(shake_t *s);
  bool shake_update(shake_t *s, float x, float y, float z, uint32_t t_ms); /* g; true once per shake */
  ```

- [ ] **Step 1: Write the failing tests**

Create `test/test_motion.c`:

```c
#include <math.h>
#include "motion.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

/* ---- shake ---- */

static bool hit(shake_t *s, uint32_t t) { return shake_update(s, 0.0f, 0.0f, 2.5f, t); }
static bool calm(shake_t *s, uint32_t t) { return shake_update(s, 0.0f, 0.0f, 1.0f, t); }

static void test_three_strong_samples_within_300ms_are_a_shake(void)
{
    shake_t s;
    shake_reset(&s);
    TEST_ASSERT_FALSE(hit(&s, 0));
    TEST_ASSERT_FALSE(hit(&s, 20));
    TEST_ASSERT_TRUE(hit(&s, 40));
}

static void test_a_jolt_of_two_strong_samples_is_not_a_shake(void)
{
    shake_t s;
    shake_reset(&s);
    TEST_ASSERT_FALSE(hit(&s, 0));
    TEST_ASSERT_FALSE(hit(&s, 20));
    for (uint32_t t = 40; t < 2000; t += 20) {
        TEST_ASSERT_FALSE(calm(&s, t));
    }
}

static void test_samples_spread_over_more_than_300ms_are_not_a_shake(void)
{
    shake_t s;
    shake_reset(&s);
    TEST_ASSERT_FALSE(hit(&s, 0));
    TEST_ASSERT_FALSE(hit(&s, 200));
    TEST_ASSERT_FALSE(hit(&s, 301)); /* 301 ms after the first */
    TEST_ASSERT_TRUE(hit(&s, 500));  /* 200, 301 and 500 fit in 300 ms */
}

static void test_exactly_2_2g_does_not_count(void)
{
    shake_t s;
    shake_reset(&s);
    for (uint32_t t = 0; t < 200; t += 20) {
        TEST_ASSERT_FALSE(shake_update(&s, 0.0f, 0.0f, 2.2f, t));
    }
}

static void test_all_three_axes_count(void)
{
    shake_t s;
    shake_reset(&s);
    /* 1.3 g on each axis is 2.25 g in all */
    TEST_ASSERT_FALSE(shake_update(&s, 1.3f, -1.3f, 1.3f, 0));
    TEST_ASSERT_FALSE(shake_update(&s, -1.3f, 1.3f, -1.3f, 20));
    TEST_ASSERT_TRUE(shake_update(&s, 1.3f, 1.3f, 1.3f, 40));
}

static void test_lockout_of_1500ms_after_a_shake(void)
{
    shake_t s;
    shake_reset(&s);
    hit(&s, 0);
    hit(&s, 20);
    TEST_ASSERT_TRUE(hit(&s, 40));
    for (uint32_t t = 60; t < 1540; t += 20) {
        TEST_ASSERT_FALSE(hit(&s, t)); /* still shaking, but locked out */
    }
    TEST_ASSERT_FALSE(hit(&s, 1540)); /* 1500 ms later: counts again, from zero */
    TEST_ASSERT_FALSE(hit(&s, 1560));
    TEST_ASSERT_TRUE(hit(&s, 1580));
}

static void test_shake_survives_the_millisecond_counter_wrapping(void)
{
    shake_t s;
    shake_reset(&s);
    hit(&s, 0xFFFFFFF0u);
    hit(&s, 0x00000004u);
    TEST_ASSERT_TRUE(hit(&s, 0x00000018u));  /* 40 ms after the first */
    TEST_ASSERT_FALSE(hit(&s, 0x00000100u)); /* locked out */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_three_strong_samples_within_300ms_are_a_shake);
    RUN_TEST(test_a_jolt_of_two_strong_samples_is_not_a_shake);
    RUN_TEST(test_samples_spread_over_more_than_300ms_are_not_a_shake);
    RUN_TEST(test_exactly_2_2g_does_not_count);
    RUN_TEST(test_all_three_axes_count);
    RUN_TEST(test_lockout_of_1500ms_after_a_shake);
    RUN_TEST(test_shake_survives_the_millisecond_counter_wrapping);
    return UNITY_END();
}
```

- [ ] **Step 2: Run to verify they fail**

Run: `sh test/run.sh -R motion`
Expected: build FAILS, `motion.h` not found.

- [ ] **Step 3: Write the header**

Create `components/logic/include/motion.h`:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Shake (spec §7): acceleration above 2.2 g for 3 samples within 300 ms, then a 1.5 s lockout. */
#define SHAKE_G          2.2f
#define SHAKE_SAMPLES    3
#define SHAKE_WINDOW_MS  300u
#define SHAKE_LOCKOUT_MS 1500u

typedef struct {
    uint32_t hit_ms[SHAKE_SAMPLES]; /* times of the latest samples above SHAKE_G, oldest first */
    uint8_t hits;
    bool locked;
    uint32_t locked_ms;
} shake_t;

void shake_reset(shake_t *s);
/* Feed one accelerometer sample in g (any axes). True once per shake. */
bool shake_update(shake_t *s, float x, float y, float z, uint32_t t_ms);
```

- [ ] **Step 4: Implement it**

Create `components/logic/motion.c`:

```c
#include "motion.h"
#include <math.h>

void shake_reset(shake_t *s) { *s = (shake_t){0}; }

bool shake_update(shake_t *s, float x, float y, float z, uint32_t t_ms)
{
    if (s->locked) {
        if (t_ms - s->locked_ms < SHAKE_LOCKOUT_MS) { /* unsigned: correct across the wrap */
            return false;
        }
        s->locked = false;
    }
    if (x * x + y * y + z * z <= SHAKE_G * SHAKE_G) {
        return false;
    }
    if (s->hits == SHAKE_SAMPLES) { /* keep the newest ones: drop the oldest */
        for (int i = 1; i < SHAKE_SAMPLES; i++) {
            s->hit_ms[i - 1] = s->hit_ms[i];
        }
        s->hits--;
    }
    s->hit_ms[s->hits++] = t_ms;
    if (s->hits == SHAKE_SAMPLES && t_ms - s->hit_ms[0] <= SHAKE_WINDOW_MS) {
        s->hits = 0;
        s->locked = true;
        s->locked_ms = t_ms;
        return true;
    }
    return false;
}
```

- [ ] **Step 5: Run the tests**

Run: `sh test/run.sh`
Expected: all PASS (CMake picks up the new files through its globs).

- [ ] **Step 6: Commit**

```bash
git add components/logic/include/motion.h components/logic/motion.c test/test_motion.c
git commit -m "feat: shake detector"
```

---

### Task 4: Grip-relative tilt, speed curve and ball roll

**Files:**
- Modify: `components/logic/include/motion.h`
- Modify: `components/logic/motion.c`
- Test: `test/test_motion.c`

**Interfaces:**
- Consumes: Task 3's `motion.h`.
- Produces:
  ```c
  #define TILT_DEAD_DEG 8.0f
  #define TILT_FULL_DEG 25.0f
  #define TILT_MAX_PX_S 240.0f
  #define TILT_SMOOTHING 0.25f
  #define ROLL_MAX_DT_MS 100u
  typedef struct { float up[3]; float level[3]; bool have; bool recenter; } tilt_t;
  void tilt_reset(tilt_t *t);
  void tilt_feed(tilt_t *t, float x, float y, float z);
  void tilt_recenter(tilt_t *t);
  bool tilt_angles(const tilt_t *t, float *x_deg, float *y_deg);
  bool tilt_is_level(float x_deg, float y_deg);
  void tilt_speed(float x_deg, float y_deg, float *vx, float *vy);
  float roll_step(float *x, float *y, float x_deg, float y_deg, uint32_t dt_ms, float cx, float cy, float r);
  ```
  Axes are the screen's: x right, y toward the bottom (USB) edge, z out of the glass. The input vector is the accelerometer reading, which is +1 g "up" (z = +1 lying flat face up). `x_deg > 0` means the right edge is lower than at the grip, so a ball rolls right. `y_deg > 0` means the bottom edge is lower, so it rolls toward the bottom.

- [ ] **Step 1: Write the failing tests**

In `test/test_motion.c`, add above `int main(void)`:

```c
/* ---- tilt ---- */

#define TOL 0.05f

static void settle(tilt_t *t, float x, float y, float z)
{
    for (int i = 0; i < 80; i++) {
        tilt_feed(t, x, y, z);
    }
}

/* The accelerometer's reading for a toy tipped back by pitch (top edge up, as when she holds it
 * toward her face) and then turned by roll about its own screen y axis (right edge down). */
static void up_vector(float pitch_deg, float roll_deg, float v[3])
{
    const float p = pitch_deg * 3.14159265f / 180.0f, r = roll_deg * 3.14159265f / 180.0f;
    v[0] = -sinf(r) * cosf(p);
    v[1] = -sinf(p);
    v[2] = cosf(r) * cosf(p);
}

static void hold_at(tilt_t *t, float pitch_deg, float roll_deg)
{
    float v[3];
    up_vector(pitch_deg, roll_deg, v);
    settle(t, v[0], v[1], v[2]);
}

static void read_angles(const tilt_t *t, float *x, float *y) { TEST_ASSERT_TRUE(tilt_angles(t, x, y)); }

static void test_no_reading_no_angles(void)
{
    tilt_t t;
    tilt_reset(&t);
    float x, y;
    TEST_ASSERT_FALSE(tilt_angles(&t, &x, &y));
}

static void test_without_a_grip_flat_is_level(void)
{
    tilt_t t;
    tilt_reset(&t);
    float x, y;
    hold_at(&t, 0, 10); /* right edge 10° down */
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 10.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, y);
    hold_at(&t, 10, 0); /* top edge 10° up: the bottom edge is lower */
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 10.0f, y);
}

static void test_the_grip_at_recenter_is_level(void)
{
    tilt_t t;
    tilt_reset(&t);
    float x, y;
    hold_at(&t, 40, 0); /* tipped toward her face */
    tilt_recenter(&t);
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, y);
    hold_at(&t, 50, 0); /* tipped 10° further */
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 10.0f, y);
    hold_at(&t, 0, 0); /* laid flat: the top edge is now 40° lower than at the grip */
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, -40.0f, y);
}

static void test_turning_at_the_grip_rolls_sideways(void)
{
    /* 15° about the toy's own tipped axis moves "down" by 11.4° (that axis is not horizontal),
     * and a little along y: the angles measure how far down moved, like a marble would. */
    tilt_t t;
    tilt_reset(&t);
    float x, y;
    hold_at(&t, 40, 0);
    tilt_recenter(&t);
    hold_at(&t, 40, -15); /* left edge down */
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, -11.44f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.98f, y);
}

static void test_recenter_before_any_reading_uses_the_first_one(void)
{
    tilt_t t;
    tilt_reset(&t);
    tilt_recenter(&t);
    float v[3], x, y;
    up_vector(40, 0, v);
    tilt_feed(&t, v[0], v[1], v[2]);
    read_angles(&t, &x, &y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, y);
}

static void test_one_bump_stays_inside_the_dead_zone(void)
{
    /* A single 30° sample (a knock, setting it down) is smoothed to 7.4°, under the 8° dead zone. */
    tilt_t t;
    tilt_reset(&t);
    float v[3], x, y;
    hold_at(&t, 0, 0);
    up_vector(0, 30, v);
    tilt_feed(&t, v[0], v[1], v[2]);
    read_angles(&t, &x, &y);
    TEST_ASSERT_TRUE(tilt_is_level(x, y));
}

static void test_a_face_down_grip_gives_finite_angles(void)
{
    tilt_t t;
    tilt_reset(&t);
    float x, y;
    settle(&t, 0.0f, 0.0f, -1.0f);
    tilt_recenter(&t);
    settle(&t, 0.1f, -0.1f, -0.99f);
    read_angles(&t, &x, &y);
    TEST_ASSERT_TRUE(isfinite(x) && isfinite(y));
}

static void test_dead_zone_and_speed_curve(void)
{
    float vx, vy;
    tilt_speed(8.0f, 0.0f, &vx, &vy);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, vx);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, vy);
    tilt_speed(16.5f, 0.0f, &vx, &vy); /* halfway from 8° to 25° */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 120.0f, vx);
    tilt_speed(25.0f, 0.0f, &vx, &vy);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 240.0f, vx);
    tilt_speed(0.0f, -60.0f, &vx, &vy); /* capped, toward the top */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, vx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -240.0f, vy);
}

static void test_speed_follows_the_tilt_direction(void)
{
    float vx, vy;
    tilt_speed(17.67767f, 17.67767f, &vx, &vy); /* 25° toward the bottom right */
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 169.71f, vx);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 169.71f, vy);
    TEST_ASSERT_TRUE(tilt_is_level(5.0f, 6.0f));  /* 7.8°: the dead zone is round */
    TEST_ASSERT_FALSE(tilt_is_level(6.0f, 6.0f)); /* 8.5° */
}

static void test_roll_moves_by_speed_times_time(void)
{
    float x = 233.0f, y = 233.0f;
    float moved = roll_step(&x, &y, 25.0f, 0.0f, 100, 233.0f, 233.0f, 190.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.0f, moved);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 257.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 233.0f, y);
}

static void test_level_does_not_roll(void)
{
    float x = 300.0f, y = 200.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, roll_step(&x, &y, 5.0f, -5.0f, 20, 233.0f, 233.0f, 190.0f));
    TEST_ASSERT_EQUAL_FLOAT(300.0f, x);
    TEST_ASSERT_EQUAL_FLOAT(200.0f, y);
}

static void test_the_ball_stays_inside_the_rim(void)
{
    float x = 233.0f + 185.0f, y = 233.0f;
    float moved = roll_step(&x, &y, 25.0f, 0.0f, 100, 233.0f, 233.0f, 190.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 423.0f, x);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 5.0f, moved);
    /* pushed straight into the rim it stays put, and that is not movement (so not activity) */
    moved = roll_step(&x, &y, 25.0f, 0.0f, 100, 233.0f, 233.0f, 190.0f);
    TEST_ASSERT_TRUE(moved < 0.01f);
}

static void test_the_ball_slides_along_the_rim(void)
{
    float x = 423.0f, y = 233.0f;
    roll_step(&x, &y, 0.0f, 25.0f, 100, 233.0f, 233.0f, 190.0f); /* tilted toward the bottom */
    TEST_ASSERT_TRUE(y > 250.0f);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 190.0f, hypotf(x - 233.0f, y - 233.0f));
}

static void test_a_stall_rolls_at_most_100ms(void)
{
    float x = 233.0f, y = 233.0f;
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 24.0f, roll_step(&x, &y, 25.0f, 0.0f, 5000, 233.0f, 233.0f, 190.0f));
}
```

Add to `main()` before `return UNITY_END();`:

```c
    RUN_TEST(test_no_reading_no_angles);
    RUN_TEST(test_without_a_grip_flat_is_level);
    RUN_TEST(test_the_grip_at_recenter_is_level);
    RUN_TEST(test_turning_at_the_grip_rolls_sideways);
    RUN_TEST(test_recenter_before_any_reading_uses_the_first_one);
    RUN_TEST(test_one_bump_stays_inside_the_dead_zone);
    RUN_TEST(test_a_face_down_grip_gives_finite_angles);
    RUN_TEST(test_dead_zone_and_speed_curve);
    RUN_TEST(test_speed_follows_the_tilt_direction);
    RUN_TEST(test_roll_moves_by_speed_times_time);
    RUN_TEST(test_level_does_not_roll);
    RUN_TEST(test_the_ball_stays_inside_the_rim);
    RUN_TEST(test_the_ball_slides_along_the_rim);
    RUN_TEST(test_a_stall_rolls_at_most_100ms);
```

The expected numbers were checked with an independent Python model of the same rotation: grip 40°, roll −15° gives x −11.44, y +0.98; grip 40° then flat gives y −40.

- [ ] **Step 2: Run to verify they fail**

Run: `sh test/run.sh -R motion`
Expected: build FAILS, `tilt_t` undeclared.

- [ ] **Step 3: Declare the tilt API**

Append to `components/logic/include/motion.h`:

```c
/* Tilt (spec §7 and §3.10). Axes are the screen's: x to the right, y toward the bottom (USB) edge, z
 * out of the glass. Feed the accelerometer reading, which is +1 g "up": z = +1 lying flat face up.
 * Level is the grip at tilt_recenter() (flat until then), and the angles say how far "down" has
 * moved from it. */
#define TILT_DEAD_DEG  8.0f
#define TILT_FULL_DEG  25.0f
#define TILT_MAX_PX_S  240.0f
#define TILT_SMOOTHING 0.25f /* weight of each new 50 Hz sample: one bump alone stays under 8° */
#define ROLL_MAX_DT_MS 100u  /* a longer gap (a stall) rolls as if 100 ms passed */

typedef struct {
    float up[3];    /* low-passed reading */
    float level[3]; /* the reading that counts as level */
    bool have;      /* at least one sample fed */
    bool recenter;  /* the next sample becomes level */
} tilt_t;

void tilt_reset(tilt_t *t); /* level = flat, no samples yet */
void tilt_feed(tilt_t *t, float x, float y, float z);
void tilt_recenter(tilt_t *t); /* the current grip becomes level (the next sample, if none yet) */
/* x_deg > 0: the right edge is lower than at the grip, so a ball rolls right; y_deg > 0: the bottom
 * edge is lower, so it rolls toward the bottom. False before the first sample. */
bool tilt_angles(const tilt_t *t, float *x_deg, float *y_deg);
bool tilt_is_level(float x_deg, float y_deg); /* inside the round 8° dead zone */
void tilt_speed(float x_deg, float y_deg, float *vx, float *vy); /* px/s along the screen axes */
/* Rolls a ball centered at (*x, *y) for dt_ms at the tilt's speed, keeping its center within r of
 * (cx, cy). Returns how far it moved, in px (0 when level or pinned against the rim). */
float roll_step(float *x, float *y, float x_deg, float y_deg, uint32_t dt_ms, float cx, float cy, float r);
```

- [ ] **Step 4: Implement it**

Append to `components/logic/motion.c`:

```c
#define RAD_TO_DEG 57.2957795f

void tilt_reset(tilt_t *t)
{
    *t = (tilt_t){.level = {0.0f, 0.0f, 1.0f}};
}

void tilt_feed(tilt_t *t, float x, float y, float z)
{
    const float in[3] = {x, y, z};
    for (int i = 0; i < 3; i++) {
        t->up[i] = t->have ? t->up[i] + TILT_SMOOTHING * (in[i] - t->up[i]) : in[i];
    }
    t->have = true;
    if (t->recenter) {
        tilt_recenter(t);
    }
}

void tilt_recenter(tilt_t *t)
{
    if (!t->have) {
        t->recenter = true;
        return;
    }
    for (int i = 0; i < 3; i++) {
        t->level[i] = t->up[i];
    }
    t->recenter = false;
}

/* Turns u by the rotation that carries the level reading onto +z (Rodrigues' formula about the axis
 * level x z), so a toy held at its grip reads straight up. */
static void to_level_frame(const float level[3], const float u[3], float out[3])
{
    const float len = sqrtf(level[0] * level[0] + level[1] * level[1] + level[2] * level[2]);
    const float nx = len > 1e-6f ? level[0] / len : 0.0f;
    const float ny = len > 1e-6f ? level[1] / len : 0.0f;
    const float c = len > 1e-6f ? level[2] / len : 1.0f;
    const float s = sqrtf(nx * nx + ny * ny);
    if (s < 1e-4f) {
        /* the grip is flat (no turn needed) or face down (half a turn about x) */
        out[0] = u[0];
        out[1] = c > 0.0f ? u[1] : -u[1];
        out[2] = c > 0.0f ? u[2] : -u[2];
        return;
    }
    const float kx = ny / s, ky = -nx / s; /* unit axis (level x z) / s, in the screen plane */
    const float dot = kx * u[0] + ky * u[1];
    out[0] = u[0] * c + ky * u[2] * s + kx * dot * (1.0f - c);
    out[1] = u[1] * c - kx * u[2] * s + ky * dot * (1.0f - c);
    out[2] = u[2] * c + (kx * u[1] - ky * u[0]) * s;
}

bool tilt_angles(const tilt_t *t, float *x_deg, float *y_deg)
{
    if (!t->have) {
        return false;
    }
    float r[3];
    to_level_frame(t->level, t->up, r);
    *x_deg = atan2f(-r[0], r[2]) * RAD_TO_DEG;
    *y_deg = atan2f(-r[1], r[2]) * RAD_TO_DEG;
    return true;
}

bool tilt_is_level(float x_deg, float y_deg)
{
    return x_deg * x_deg + y_deg * y_deg <= TILT_DEAD_DEG * TILT_DEAD_DEG;
}

void tilt_speed(float x_deg, float y_deg, float *vx, float *vy)
{
    const float m = sqrtf(x_deg * x_deg + y_deg * y_deg);
    if (m <= TILT_DEAD_DEG) {
        *vx = 0.0f;
        *vy = 0.0f;
        return;
    }
    const float f = (m - TILT_DEAD_DEG) / (TILT_FULL_DEG - TILT_DEAD_DEG);
    const float speed = TILT_MAX_PX_S * (f > 1.0f ? 1.0f : f);
    *vx = speed * x_deg / m;
    *vy = speed * y_deg / m;
}

float roll_step(float *x, float *y, float x_deg, float y_deg, uint32_t dt_ms, float cx, float cy, float r)
{
    if (dt_ms > ROLL_MAX_DT_MS) {
        dt_ms = ROLL_MAX_DT_MS;
    }
    float vx, vy;
    tilt_speed(x_deg, y_deg, &vx, &vy);
    float nx = *x + vx * (float)dt_ms / 1000.0f;
    float ny = *y + vy * (float)dt_ms / 1000.0f;
    const float ex = nx - cx, ey = ny - cy, d = sqrtf(ex * ex + ey * ey);
    if (d > r) {
        nx = cx + ex / d * r;
        ny = cy + ey / d * r;
    }
    const float mx = nx - *x, my = ny - *y;
    *x = nx;
    *y = ny;
    return sqrtf(mx * mx + my * my);
}
```

- [ ] **Step 5: Run the tests**

Run: `sh test/run.sh`
Expected: all PASS.

- [ ] **Step 6: Commit**

```bash
git add components/logic/include/motion.h components/logic/motion.c test/test_motion.c
git commit -m "feat: grip-relative tilt, speed curve and ball roll"
```

---

### Task 5: Sensor events through a queue; shake and tilt to the open app

**Files:**
- Modify: `components/platform/include/sensors.h` (rewrite)
- Modify: `components/platform/sensors.c` (rewrite)
- Modify: `components/ui/include/app.h` (the `on_shake`/`on_tilt` comments)
- Modify: `components/ui/include/app_manager.h`
- Modify: `components/ui/app_manager.c`
- Modify: `main/app_main.c`
- Modify: `main/diag.c`
- Modify: `main/gallery.c`

**Interfaces:**
- Consumes:
  - Task 3/4 `shake_t`, `shake_reset`, `shake_update`, `tilt_t`, `tilt_reset`, `tilt_feed`, `tilt_recenter`, `tilt_angles`;
  - Task 1's axis mapping;
  - `imu_init()` / `imu_read_g()` from `imu.h`;
  - `buttons_init()` / `buttons_poll()`;
  - `idle_level()` from `idle.h`.
- Produces:
  ```c
  typedef enum { SENSOR_EV_HOME, SENSOR_EV_POWER, SENSOR_EV_SHAKE } sensor_ev_t;
  esp_err_t sensors_start(void);
  bool sensors_next(sensor_ev_t *ev);
  bool sensors_tilt(float *x_deg, float *y_deg);
  void sensors_tilt_recenter(void);
  ```
  - The app manager calls `app->on_shake()` and `app->on_tilt(x_deg, y_deg)` on the LVGL task, only while an app is open and `idle_level() == 0`.
  - It logs `button HOME` / `button POWER` for each press (the device checks count them).
  - `app_manager_on_button()` is removed.

- [ ] **Step 1: Rewrite the sensors header**

Replace `components/platform/include/sensors.h` with:

```c
#pragma once
#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    SENSOR_EV_HOME,  /* BOOT pressed */
    SENSOR_EV_POWER, /* PWR short press */
    SENSOR_EV_SHAKE, /* spec §7 shake (motion.h) */
} sensor_ev_t;

/* Starts the 50 Hz sensors task: the buttons, and the IMU when imu_init() succeeded first. The task
 * only queues events and never waits for the LVGL lock, so a slow frame cannot swallow a quick
 * second button press (spec §3.10). */
esp_err_t sensors_start(void);
/* The next queued event, without waiting; false when there is none (or before sensors_start). */
bool sensors_next(sensor_ev_t *ev);
/* Grip-relative tilt in degrees (motion.h tilt_angles). False without IMU readings. Any task. */
bool sensors_tilt(float *x_deg, float *y_deg);
/* The toy's current angle becomes level (spec §3.10: the grip when an app opens). Any task. */
void sensors_tilt_recenter(void);
```

- [ ] **Step 2: Rewrite the sensors task**

Replace `components/platform/sensors.c` with the code below. In `SCREEN_X` and `SCREEN_Y`, write Task 1's recorded expressions. For example, if Task 1 recorded `screen x = -ay` and `screen y = -ax`, the lines read `#define SCREEN_X(ax, ay) (-(ay))` and `#define SCREEN_Y(ax, ay) (-(ax))`.

```c
#include "sensors.h"
#include "buttons.h"
#include "esp_check.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "imu.h"
#include "motion.h"

#define SENSORS_PERIOD_MS 20 /* 50 Hz: button debounce, shake and tilt */
#define EVENT_QUEUE_LEN   8

/* The QMI8658's axes in screen terms, measured on the device (docs/device-checks.md,
 * "Accelerometer axes (M4)"): screen x points right, screen y toward the bottom (USB) edge, and each
 * reads +1 g when it points up. Raw z is already out of the glass (+1 g flat, face up). */
#define SCREEN_X(ax, ay) (TASK_1_SCREEN_X_EXPRESSION)
#define SCREEN_Y(ax, ay) (TASK_1_SCREEN_Y_EXPRESSION)

static const char *TAG = "sensors";
static QueueHandle_t s_events;
static portMUX_TYPE s_tilt_lock = portMUX_INITIALIZER_UNLOCKED;
static tilt_t s_tilt;   /* shared with sensors_tilt*(): only under s_tilt_lock */
static shake_t s_shake; /* sensors task only */

static void post(sensor_ev_t ev)
{
    if (xQueueSend(s_events, &ev, 0) != pdTRUE) {
        ESP_LOGW(TAG, "event queue full, dropped event %d", (int)ev);
    }
}

static void sample_imu(void)
{
    float ax, ay, az;
    if (!imu_read_g(&ax, &ay, &az)) {
        return; /* no IMU: shake and tilt do nothing (spec §4.7) */
    }
    const float x = SCREEN_X(ax, ay), y = SCREEN_Y(ax, ay), z = az;
    if (shake_update(&s_shake, x, y, z, (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount()))) {
        post(SENSOR_EV_SHAKE);
    }
    taskENTER_CRITICAL(&s_tilt_lock);
    tilt_feed(&s_tilt, x, y, z);
    taskEXIT_CRITICAL(&s_tilt_lock);
}

static void sensors_task(void *arg)
{
    (void)arg;
    for (;;) {
        /* Re-based every loop: after a slow I2C read (a PMU timeout is 50 ms) the next poll waits a
         * full period instead of firing back-to-back, which the debounce's 20 ms spacing relies on. */
        TickType_t last = xTaskGetTickCount();
        uint8_t ev = buttons_poll();
        if (ev & BUTTON_EV_HOME) {
            post(SENSOR_EV_HOME);
        }
        if (ev & BUTTON_EV_POWER) {
            post(SENSOR_EV_POWER);
        }
        sample_imu();
        vTaskDelayUntil(&last, pdMS_TO_TICKS(SENSORS_PERIOD_MS));
    }
}

esp_err_t sensors_start(void)
{
    ESP_RETURN_ON_ERROR(buttons_init(), TAG, "buttons");
    tilt_reset(&s_tilt);
    shake_reset(&s_shake);
    s_events = xQueueCreate(EVENT_QUEUE_LEN, sizeof(sensor_ev_t));
    ESP_RETURN_ON_FALSE(s_events, ESP_ERR_NO_MEM, TAG, "queue");
    BaseType_t ok = xTaskCreatePinnedToCore(sensors_task, "sensors", 8192, NULL, 4, NULL, 0);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

bool sensors_next(sensor_ev_t *ev)
{
    return s_events && xQueueReceive(s_events, ev, 0) == pdTRUE;
}

bool sensors_tilt(float *x_deg, float *y_deg)
{
    taskENTER_CRITICAL(&s_tilt_lock);
    tilt_t t = s_tilt;
    taskEXIT_CRITICAL(&s_tilt_lock);
    return tilt_angles(&t, x_deg, y_deg);
}

void sensors_tilt_recenter(void)
{
    taskENTER_CRITICAL(&s_tilt_lock);
    tilt_recenter(&s_tilt);
    taskEXIT_CRITICAL(&s_tilt_lock);
}
```

`TASK_1_SCREEN_X_EXPRESSION` and `TASK_1_SCREEN_Y_EXPRESSION` are not literal text: the build must not contain them. Step 2's first paragraph says what replaces them.

- [ ] **Step 3: Update the app contract comments**

In `components/ui/include/app.h`, replace the two lines

```c
    void (*on_shake)(void);           /* optional, NULL if unused */
    void (*on_tilt)(float x_deg, float y_deg); /* optional, NULL if unused */
```

with:

```c
    /* Optional, NULL if unused. Both run on the LVGL task, only while this app is open and the
     * screen is awake (spec §3.10). */
    void (*on_shake)(void); /* once per shake; the app manager counts it as activity */
    /* 50 Hz. Degrees from the grip the toy had when the app opened: x_deg > 0 means the right edge
     * is lower (a ball rolls right), y_deg > 0 the bottom edge (it rolls toward the bottom). Tilt
     * alone is not activity: call lv_display_trigger_activity(NULL) when it visibly moves
     * something, so play by tilting alone is not closed by the 60 s idle timeout. */
    void (*on_tilt)(float x_deg, float y_deg);
```

- [ ] **Step 4: Route events in the app manager**

In `components/ui/include/app_manager.h`:
- replace `#include "buttons.h"` with nothing (delete the line);
- delete the `app_manager_on_button` declaration and the comment above it.

In `components/ui/app_manager.c`:

1. Add these includes after `#include "audio.h"`: `#include "buttons.h"`, `#include "esp_log.h"` and `#include "sensors.h"`. Below the includes, add:

```c
#define SENSOR_POLL_MS 20 /* matches the sensors task's 50 Hz */

static const char *TAG = "app";
```

2. In `app_manager_init()`, after `idle_init();`, add:

```c
    lv_timer_create(poll_sensors, SENSOR_POLL_MS, NULL);
```

`poll_sensors` is defined further down, so declare it above `app_manager_init()`:

```c
static void poll_sensors(lv_timer_t *t);
```

3. In `app_manager_open()`, directly before `app->create(s_screen);`, add:

```c
    sensors_tilt_recenter(); /* the grip she opens it with is level (spec §3.10) */
```

4. Change `handle_button` from an `lv_async_call` callback to a plain function:
   - replace its signature and first line
     ```c
     static void handle_button(void *user_data)
     {
         button_id_t id = (button_id_t)(uintptr_t)user_data;
     ```
     with
     ```c
     static void handle_button(button_id_t id)
     {
     ```
   - replace the comment above it with `/* Runs on the LVGL task (poll_sensors below). */`.

5. Replace the whole `app_manager_on_button()` function at the end of the file with:

```c
/* Runs on the LVGL task: drains the sensors task's queue and passes the tilt to the open app. */
static void poll_sensors(lv_timer_t *t)
{
    (void)t;
    sensor_ev_t ev;
    while (sensors_next(&ev)) {
        if (ev == SENSOR_EV_SHAKE) {
            if (s_current && s_current->on_shake && idle_level() == 0) {
                lv_display_trigger_activity(NULL); /* a shake is play (spec §3.10) */
                s_current->on_shake();
            }
            continue;
        }
        const button_id_t id = ev == SENSOR_EV_HOME ? BUTTON_HOME : BUTTON_POWER;
        ESP_LOGI(TAG, "button %s", id == BUTTON_HOME ? "HOME" : "POWER");
        handle_button(id);
    }
    float x, y;
    if (s_current && s_current->on_tilt && idle_level() == 0 && sensors_tilt(&x, &y)) {
        s_current->on_tilt(x, y);
    }
}
```

- [ ] **Step 5: Start the IMU and the queue in `app_main`**

In `main/app_main.c`:
- add `#include "imu.h"` after `#include "esp_system.h"`;
- delete the `#if !CONFIG_CAT_DEV_DIAG && !CONFIG_CAT_DEV_GALLERY` block that defines `on_button` (through its `#endif`);
- replace

```c
    const sensors_listener_t listener = {.on_button = on_button};
    if (sensors_start(&listener) != ESP_OK) {
        ESP_LOGE(TAG, "buttons unavailable");
    }
```

with:

```c
    if (imu_init() != ESP_OK) {
        ESP_LOGE(TAG, "IMU unavailable: shake and tilt do nothing");
    }
    if (sensors_start() != ESP_OK) {
        ESP_LOGE(TAG, "sensors unavailable: no buttons, shake or tilt");
    }
```

- [ ] **Step 6: Drain the queue in the diagnostics screen**

In `main/diag.c`:
- replace the whole `on_button()` function with the `on_event()` below;
- in `tick()`, insert the drain loop as its first statements, after `(void)t;`;
- extend the once-per-second log with the flat-relative tilt (diag never recenters, so level is flat), for M8 tuning.

```c
static void on_event(sensor_ev_t ev) /* LVGL task (tick below), so no lock is needed */
{
    if (ev == SENSOR_EV_SHAKE) {
        ESP_LOGI(TAG, "shake");
        audio_play(SND_BLIP_UP);
        return;
    }
    if (ev == SENSOR_EV_HOME) {
        s_home++;
    } else {
        s_power++;
    }
    ESP_LOGI(TAG, "button %s", ev == SENSOR_EV_HOME ? "HOME" : "POWER");
    audio_play(ev == SENSOR_EV_HOME ? SND_BLIP_UP : SND_BLIP_DOWN);
    lv_label_set_text_fmt(s_buttons, "HOME %lu  POWER %lu", (unsigned long)s_home, (unsigned long)s_power);
}
```

```c
    sensor_ev_t ev;
    while (sensors_next(&ev)) {
        on_event(ev);
    }
```

Replace the existing `if (++s_ticks % 5 == 0) { ... }` block in `tick()` with:

```c
    if (++s_ticks % 5 == 0) {
        float tx = 0.0f, ty = 0.0f;
        sensors_tilt(&tx, &ty);
        ESP_LOGI(TAG, "accel x=%.2f y=%.2f z=%.2f tilt x=%.1f y=%.1f psram free=%u", (double)x, (double)y, (double)z,
                 (double)tx, (double)ty, (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    }
```

In `diag_start()`, replace

```c
    const sensors_listener_t buttons = {.on_button = on_button};
    if (sensors_start(&buttons) != ESP_OK) {
```

with

```c
    if (sensors_start() != ESP_OK) {
```

- [ ] **Step 7: Drain the queue in the gallery**

In `main/gallery.c`, replace the `on_button()` function with:

```c
static void poll_buttons(lv_timer_t *t) /* LVGL task, so no lock is needed */
{
    (void)t;
    sensor_ev_t ev;
    while (sensors_next(&ev)) {
        if (ev == SENSOR_EV_HOME) {
            s_costume = (cat_costume_t)((s_costume + 1) % CAT_COSTUME_COUNT);
            cat_set_costume(s_cat, s_costume);
            refresh_label();
        } else if (ev == SENSOR_EV_POWER) {
            s_combo = (uint8_t)((s_combo + 1) % 9);
            rebuild();
        }
    }
}
```

In `gallery_start()`:
- add `lv_timer_create(poll_buttons, 50, NULL);` directly before `display_unlock();`;
- replace the last two lines (`const sensors_listener_t listener = ...;` and `sensors_start(&listener);`) with `sensors_start();`.

- [ ] **Step 8: Build every variant that changed**

Run:
```bash
idf.py build
idf.py -B build-diag -D SDKCONFIG=build-diag/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.diag" build
idf.py -B build-gallery -D SDKCONFIG=build-gallery/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.gallery;sdkconfig.perf" build
```
Expected: all three `Project build complete` with no warnings from our files. Also run `grep -rn "TASK_1_SCREEN\|sensors_listener_t\|app_manager_on_button" components main`, which must print nothing.

- [ ] **Step 9: Commit**

```bash
git add components/platform/include/sensors.h components/platform/sensors.c components/ui/include/app.h components/ui/include/app_manager.h components/ui/app_manager.c main/app_main.c main/diag.c main/gallery.c
git commit -m "feat: sensor events through a queue; shake and tilt reach the open app"
```

---

### Task 6: Pet assets: fish, ball, heart, and the reaction sounds

**Files:**
- Modify: `tools/export_images/ui_images.mjs`
- Modify: `tools/export_images/export.mjs` (optional `size`)
- Modify: `components/assets/assets_manifest.c` (regenerated)
- Modify: `tools/build_sounds/sounds.txt`
- Modify: `components/assets/include/sounds.h`
- Modify: `components/assets/sounds.c`

**Interfaces:**
- Consumes: nothing.
- Produces:
  - images `ui/fish` (cropped; `assets_ui_pos("ui/fish", ...)` gives its offset inside the 150 px button box), `ui/ball` (48 × 48) and `ui/heart` (34 × 34);
  - sound ids `SND_PURR` (a loop), `SND_BELL_TICK`, `SND_PET_RUB`, `SND_PET_DIZZY` and `SND_PET_EAT`.

- [ ] **Step 1: Let a UI image render at a size other than its viewBox**

In `tools/export_images/export.mjs`, in the `for (const u of UI_IMAGES)` loop, replace

```js
  let img = render(svgDoc(u.viewBox, u.svg), u.viewBox[2]);
```

with

```js
  let img = render(svgDoc(u.viewBox, u.svg), u.size ?? u.viewBox[2]);
```

- [ ] **Step 2: Add the three images (design `Screen.dc.html` pet view)**

Append to the `UI_IMAGES` array in `tools/export_images/ui_images.mjs`, before the closing `];`:

```js
  {
    name: 'ui/fish',
    viewBox: [0, 0, 150, 150],
    crop: true,
    svg:
      '<path d="M30 75 Q64 42 100 75 Q64 108 30 75 Z" fill="#FFF6E6" stroke="#0F5A85" stroke-width="6" stroke-linejoin="round"/>' +
      '<path d="M98 75 L122 54 L122 96 Z" fill="#FFF6E6" stroke="#0F5A85" stroke-width="6" stroke-linejoin="round"/>' +
      '<circle cx="50" cy="70" r="6" fill="#0F5A85"/>',
  },
  {
    name: 'ui/ball',
    viewBox: [0, 0, 48, 48],
    crop: false,
    svg:
      '<circle cx="24" cy="24" r="20" fill="#FFD84A" stroke="#9A7A00" stroke-width="5"/>' +
      '<path d="M14 18 Q24 8 34 18" fill="none" stroke="#FFF6B0" stroke-width="4" stroke-linecap="round"/>',
  },
  {
    name: 'ui/heart',
    viewBox: [0, 0, 30, 30],
    size: 34,
    crop: false,
    svg: '<path d="M15 27 C4 19 1 12 4 7 C7 2 13 3 15 8 C17 3 23 2 26 7 C29 12 26 19 15 27 Z" fill="#FF7FB0"/>',
  },
```

- [ ] **Step 3: Add the combined reaction sounds**

In `tools/build_sounds/sounds.txt`, after the line `open          concat  pop+idemo`, add:

```
pet_rub       concat  giggle+hihihi
pet_dizzy     concat  wheee+boing
pet_eat       concat  crunch+crunch+njam
```

The audio task's `fx` channel plays one sound at a time, and a new sound replaces the playing one. So "giggle, then Hihihi!" is one file, the same way `open` is pop + "Idemo!".

- [ ] **Step 4: Add the sound ids**

In `components/assets/include/sounds.h`, before `SND_COUNT`, add:

```c
    SND_PURR, /* a loop: audio_loop_start() */
    SND_BELL_TICK,
    SND_PET_RUB,
    SND_PET_DIZZY,
    SND_PET_EAT,
```

In `components/assets/sounds.c`, add to `FILES` after `[SND_COSTUME_CAVE] = "costume_cave",`:

```c
    [SND_PURR] = "purr",
    [SND_BELL_TICK] = "bell_tick",
    [SND_PET_RUB] = "pet_rub",
    [SND_PET_DIZZY] = "pet_dizzy",
    [SND_PET_EAT] = "pet_eat",
```

- [ ] **Step 5: Regenerate and check the assets**

Run:
```bash
tools/build_assets.sh
ls -l assets/img/ui/fish.bin assets/img/ui/ball.bin assets/img/ui/heart.bin assets/snd/pet_rub.wav assets/snd/pet_dizzy.wav assets/snd/pet_eat.wav assets/snd/purr.wav assets/snd/bell_tick.wav
grep -n '"ui/fish"' components/assets/assets_manifest.c
for f in pet_rub pet_dizzy pet_eat purr bell_tick; do printf '%s ' $f; soxi -D assets/snd/$f.wav; done
```
Expected:
- all eight files exist;
- the manifest has a `{"ui/fish", x, y}` row with x and y around 28 and 50;
- each sound is between 0.05 s and 4 s long.

- [ ] **Step 6: Build**

Run: `idf.py build`
Expected: `Project build complete`.

- [ ] **Step 7: Commit**

`assets/` itself is gitignored. The manifest is generated but committed.

```bash
git add tools/export_images/ui_images.mjs tools/export_images/export.mjs components/assets/assets_manifest.c tools/build_sounds/sounds.txt components/assets/include/sounds.h components/assets/sounds.c
git commit -m "feat: pet assets: fish, ball and heart images, reaction sounds"
```

---

### Task 7: Pet the cat app

**Files:**
- Create: `components/ui/apps/pet.c`
- Modify: `components/ui/apps/placeholder.c` (remove the `app_pet` line)
- Modify: `components/ui/include/tokens.h`

**Interfaces:**
- Consumes:
  - Task 2 `rub_track_t`, `rub_begin`, `rub_move`, `rub_end`;
  - Task 4 `tilt_is_level`, `roll_step`;
  - Task 5 `on_shake`/`on_tilt` delivery;
  - Task 6 images and sound ids;
  - `cat_create`, `cat_set_origin`, `cat_set_state`, `cat_set_look`;
  - `ui_circle`, `ui_image`, `ui_set_hidden`, `assets_ui_pos`;
  - `audio_play`, `audio_loop_start`, `audio_loop_stop`, `audio_stop_all`.
- Produces: `const app_t app_pet`, already listed in `APP_LIST` in `components/ui/apps/registry.c`.

- [ ] **Step 1: Add the pet constants**

Append to `components/ui/include/tokens.h`:

```c
/* Pet the cat (Screen.dc.html pet view; spec §3.10). The design's 250 px cat (1.25x) is drawn at
 * scale L (1.3x), centered on the design's head center (233, 159). */
#define PET_CAT_ORIGIN_X  103
#define PET_CAT_ORIGIN_Y  19
#define PET_EYES_X        233 /* between the eyes: design (100, 104) at 1.3x */
#define PET_EYES_Y        154
#define PET_HIT_CX        233 /* the whole head is the touch area */
#define PET_HIT_CY        155
#define PET_HIT_SIZE      230
#define PET_FISH_CX       233
#define PET_FISH_CY       373
#define PET_FISH_SIZE     150 /* touch target */
#define PET_FISH_RING     146 /* the drawn circle: r 68 with its 10 px stroke centered on it */
#define PET_FISH_BORDER   10
#define PET_FISH_EAT_OPA  128 /* 50 % while eating */
#define COLOR_FISH        0x3FB6FF
#define COLOR_FISH_RING   0x0F5A85
#define PET_BALL_SIZE     48
#define PET_BALL_START_X  330
#define PET_BALL_START_Y  110
#define PET_BALL_RANGE    190  /* the ball's center stays this close to the screen center */
#define PET_BALL_HIDE_MS  3000 /* back to the plain face 3 s after the last tilt */
#define PET_ROLL_MIN_PX   0.5f /* less than this per step is not rolling (not activity, no tick) */
#define PET_TICK_PX       60   /* one bell tick per 60 px rolled */
#define PET_HEART_RISE    44
#define PET_HEART_MS      1200
#define PET_PURR_SOUND_MS 1500
#define PET_PURR_MS       1800
#define PET_GIGGLE_MS     1800
#define PET_DIZZY_MS      2000
#define PET_EAT_MS        2400
```

- [ ] **Step 2: Write the app**

Create `components/ui/apps/pet.c`:

```c
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "app.h"
#include "assets.h"
#include "audio.h"
#include "cat.h"
#include "gestures.h"
#include "motion.h"
#include "tokens.h"
#include "ui_util.h"

/* Pet the cat (design: Screen.dc.html pet view; spec §3.10). Tap = purr, rub = giggle,
 * shake = dizzy, tilt = a ball the cat's eyes follow, fish = eat. */

typedef enum { R_NONE, R_PURR, R_GIGGLE, R_DIZZY, R_EAT } react_t;

typedef struct {
    cat_state_t state;
    uint32_t ms; /* how long the cat keeps this face */
    bool hearts;
    sound_id_t sound; /* fx sound; the purr is a loop instead */
} reaction_t;

static const reaction_t REACTIONS[] = {
    [R_PURR] = {CAT_PURRING, PET_PURR_MS, true, SND_PURR},
    [R_GIGGLE] = {CAT_LAUGHING, PET_GIGGLE_MS, true, SND_PET_RUB},
    [R_DIZZY] = {CAT_DIZZY, PET_DIZZY_MS, false, SND_PET_DIZZY},
    [R_EAT] = {CAT_EATING, PET_EAT_MS, false, SND_PET_EAT},
};

/* Hearts around the cat (design: top-left corners and start delays). */
static const struct {
    int16_t x, y;
    uint16_t delay_ms;
} HEARTS[] = {{96, 70, 0}, {336, 60, 350}, {350, 190, 700}, {80, 200, 1000}};
#define HEART_COUNT (sizeof(HEARTS) / sizeof(HEARTS[0]))

static lv_obj_t *s_cat; /* NULL once destroy() has run: every callback checks it first (app.h) */
static lv_obj_t *s_ball, *s_fish_ring;
static lv_obj_t *s_hearts[HEART_COUNT];
static lv_timer_t *s_react_timer, *s_purr_timer;
static react_t s_react;
static rub_track_t s_rub;
static float s_ball_x, s_ball_y; /* ball center, screen px */
static bool s_ball_shown;
static uint32_t s_last_tilt_ms; /* last on_tilt outside the dead zone */
static uint32_t s_last_roll_ms; /* last on_tilt call */
static float s_tick_px;         /* rolled since the last bell tick */

/* ---- hearts ---- */

/* One float-up cycle, v = 0..1000 (design keyframes: from +10 px to -44 px, fading in over the
 * first 30 % and out over the rest). */
static void heart_step(void *obj, int32_t v)
{
    lv_obj_set_style_translate_y(obj, 10 - (PET_HEART_RISE + 10) * v / 1000, 0);
    int32_t opa = v < 300 ? v * LV_OPA_COVER / 300 : (1000 - v) * LV_OPA_COVER / 700;
    lv_obj_set_style_opa(obj, (lv_opa_t)opa, 0);
}

static void hearts_show(bool show)
{
    for (size_t i = 0; i < HEART_COUNT; i++) {
        lv_obj_t *h = s_hearts[i];
        lv_anim_delete(h, heart_step);
        ui_set_hidden(h, !show);
        if (!show) {
            continue;
        }
        lv_obj_set_style_opa(h, LV_OPA_TRANSP, 0); /* invisible until its delay has passed */
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, h);
        lv_anim_set_exec_cb(&a, heart_step);
        lv_anim_set_values(&a, 0, 1000);
        lv_anim_set_duration(&a, PET_HEART_MS);
        lv_anim_set_delay(&a, HEARTS[i].delay_ms);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_out);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    }
}

/* ---- the cat's face ---- */

/* Pupils toward the ball, only on the plain face (spec §3.10). */
static void look_at_ball(void)
{
    if (!s_ball_shown || s_react != R_NONE) {
        return;
    }
    const float dx = s_ball_x - PET_EYES_X, dy = s_ball_y - PET_EYES_Y;
    const float len = sqrtf(dx * dx + dy * dy);
    if (len < 1.0f) {
        return; /* right between the eyes: keep the last look */
    }
    cat_set_look(s_cat, dx / len, dy / len);
}

static void plain_face(void)
{
    s_react = R_NONE;
    hearts_show(false);
    lv_obj_set_style_opa(s_fish_ring, LV_OPA_COVER, 0);
    cat_set_state(s_cat, CAT_HAPPY); /* also drops any look */
    look_at_ball();
}

static void react_done(lv_timer_t *t)
{
    (void)t;
    s_react_timer = NULL; /* one-shot timers delete themselves */
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    plain_face();
}

static void purr_done(lv_timer_t *t)
{
    (void)t;
    s_purr_timer = NULL;
    if (!s_cat) {
        return;
    }
    audio_loop_stop();
}

static lv_timer_t *one_shot(lv_timer_cb_t cb, uint32_t ms)
{
    lv_timer_t *t = lv_timer_create(cb, ms, NULL);
    lv_timer_set_repeat_count(t, 1);
    return t;
}

static void cancel_timers(void)
{
    if (s_react_timer) {
        lv_timer_delete(s_react_timer);
        s_react_timer = NULL;
    }
    if (s_purr_timer) {
        lv_timer_delete(s_purr_timer);
        s_purr_timer = NULL;
    }
}

/* A new reaction replaces whatever the cat was doing, sound included (spec §3.10). */
static void react(react_t r)
{
    const reaction_t *re = &REACTIONS[r];
    cancel_timers();
    audio_stop_all();
    s_react = r;
    cat_set_state(s_cat, re->state);
    hearts_show(re->hearts);
    lv_obj_set_style_opa(s_fish_ring, r == R_EAT ? PET_FISH_EAT_OPA : LV_OPA_COVER, 0);
    if (r == R_PURR) {
        audio_loop_start(re->sound);
        s_purr_timer = one_shot(purr_done, PET_PURR_SOUND_MS);
    } else {
        audio_play(re->sound);
    }
    s_react_timer = one_shot(react_done, re->ms);
}

/* ---- touch ---- */

static void cat_touch(lv_event_t *e)
{
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    lv_indev_t *indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    const int16_t x = (int16_t)p.x, y = (int16_t)p.y;
    const uint32_t now = lv_tick_get();
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED: rub_begin(&s_rub, x, y, now); break;
    case LV_EVENT_PRESSING:
        if (rub_move(&s_rub, x, y)) {
            react(R_GIGGLE);
        }
        break;
    case LV_EVENT_RELEASED:
        if (rub_end(&s_rub, x, y, now)) {
            react(R_PURR);
        }
        break;
    default: break;
    }
}

static void fish_tapped(lv_event_t *e)
{
    (void)e;
    if (!s_cat) {
        return;
    }
    react(R_EAT);
}

/* ---- sensors: app_manager calls these on the LVGL task while the screen is awake ---- */

static void pet_shake(void)
{
    if (!s_cat) {
        return;
    }
    react(R_DIZZY);
}

static void pet_tilt(float x_deg, float y_deg)
{
    if (!s_cat) {
        return;
    }
    const uint32_t dt = lv_tick_elaps(s_last_roll_ms);
    s_last_roll_ms = lv_tick_get();
    if (tilt_is_level(x_deg, y_deg)) {
        if (s_ball_shown && lv_tick_elaps(s_last_tilt_ms) >= PET_BALL_HIDE_MS) {
            s_ball_shown = false;
            ui_set_hidden(s_ball, true);
            if (s_react == R_NONE) {
                cat_set_state(s_cat, CAT_HAPPY); /* eyes straight ahead again */
            }
        }
        return;
    }
    s_last_tilt_ms = s_last_roll_ms;
    if (!s_ball_shown) {
        s_ball_shown = true;
        ui_set_hidden(s_ball, false);
    }
    const float moved =
        roll_step(&s_ball_x, &s_ball_y, x_deg, y_deg, dt, SCREEN_CENTER, SCREEN_CENTER, PET_BALL_RANGE);
    lv_obj_set_pos(s_ball, (int32_t)lroundf(s_ball_x) - PET_BALL_SIZE / 2,
                   (int32_t)lroundf(s_ball_y) - PET_BALL_SIZE / 2);
    if (moved >= PET_ROLL_MIN_PX) {
        lv_display_trigger_activity(NULL); /* rolling is play; resting against the rim is not */
        s_tick_px += moved;
        if (s_tick_px >= PET_TICK_PX) {
            s_tick_px -= PET_TICK_PX;
            if (s_react == R_NONE) {
                audio_play(SND_BELL_TICK);
            }
        }
    }
    look_at_ball();
}

/* ---- screen ---- */

static void build(lv_obj_t *scr)
{
    s_cat = cat_create(scr, CAT_SCALE_L);
    cat_set_origin(s_cat, PET_CAT_ORIGIN_X, PET_CAT_ORIGIN_Y);

    for (size_t i = 0; i < HEART_COUNT; i++) {
        s_hearts[i] = ui_image(scr, "ui/heart", HEARTS[i].x, HEARTS[i].y);
        ui_set_hidden(s_hearts[i], true);
    }

    /* Above the cat, below the fish (design order); not clickable, so taps reach the cat. */
    s_ball = ui_image(scr, "ui/ball", PET_BALL_START_X - PET_BALL_SIZE / 2, PET_BALL_START_Y - PET_BALL_SIZE / 2);
    ui_set_hidden(s_ball, true);

    lv_obj_t *hit = ui_circle(scr, PET_HIT_CX, PET_HIT_CY, PET_HIT_SIZE, 0);
    lv_obj_set_style_bg_opa(hit, LV_OPA_TRANSP, 0);
    lv_obj_add_flag(hit, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hit, cat_touch, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(hit, cat_touch, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(hit, cat_touch, LV_EVENT_RELEASED, NULL);

    s_fish_ring = ui_circle(scr, PET_FISH_CX, PET_FISH_CY, PET_FISH_RING, COLOR_FISH);
    lv_obj_set_style_border_width(s_fish_ring, PET_FISH_BORDER, 0);
    lv_obj_set_style_border_color(s_fish_ring, lv_color_hex(COLOR_FISH_RING), 0);
    lv_obj_add_flag(s_fish_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_fish_ring, (PET_FISH_SIZE - PET_FISH_RING) / 2); /* the 150 px target */
    lv_obj_add_event_cb(s_fish_ring, fish_tapped, LV_EVENT_CLICKED, NULL);
    int16_t x, y;
    assets_ui_pos("ui/fish", &x, &y);
    /* not clickable: taps reach the ring */
    ui_image(scr, "ui/fish", PET_FISH_CX - PET_FISH_SIZE / 2 + x, PET_FISH_CY - PET_FISH_SIZE / 2 + y);
}

static void pet_create(lv_obj_t *screen)
{
    s_react = R_NONE;
    s_rub = (rub_track_t){0};
    s_ball_x = PET_BALL_START_X;
    s_ball_y = PET_BALL_START_Y;
    s_ball_shown = false;
    s_tick_px = 0.0f;
    s_last_roll_ms = s_last_tilt_ms = lv_tick_get();
    build(screen);
}

static void pet_destroy(void)
{
    cancel_timers();
    for (size_t i = 0; i < HEART_COUNT; i++) {
        lv_anim_delete(s_hearts[i], heart_step);
        s_hearts[i] = NULL;
    }
    s_cat = NULL;
    s_ball = NULL;
    s_fish_ring = NULL;
    /* the purr loop and any reaction sound: app_manager calls audio_stop_all() after this */
}

const app_t app_pet = {
    .id = "pet",
    .icon = "apps/pet",
    .icon_small = "apps/pet_96",
    .color = COLOR_PET,
    .name_sound = SND_NAME_PET,
    .swipe_sound = SND_SWIPE_PET,
    .react = CAT_HAPPY,
    .create = pet_create,
    .destroy = pet_destroy,
    .on_shake = pet_shake,
    .on_tilt = pet_tilt,
};
```

- [ ] **Step 3: Retire the placeholder**

In `components/ui/apps/placeholder.c`:
- delete the line `PLACEHOLDER(app_pet, "pet", COLOR_PET, SND_NAME_PET, SND_SWIPE_PET, CAT_HAPPY)`;
- change the comment above the list to `/* Remove a line when that app's real file lands (voices: Task 17, pet: M4). */`.

- [ ] **Step 4: Build**

Run:
```bash
idf.py reconfigure build
```
Expected: `Project build complete`, no warnings from `pet.c`. Reconfigure is needed because `pet.c` is new.

- [ ] **Step 5: Commit**

```bash
git add components/ui/apps/pet.c components/ui/apps/placeholder.c components/ui/include/tokens.h
git commit -m "feat: Pet the cat: tap to purr, rub to giggle, shake, tilt the ball, feed the fish"
```

---

### Task 8: Soak covers shake and tilt

**Files:**
- Modify: `main/soak.c`

**Interfaces:**
- Consumes: `app_t.on_shake`, `app_t.on_tilt` (Task 5 contract), `app_pet` (Task 7).
- Produces: a soak round that shakes every app with `on_shake` right after opening it, tilts it mid-way, then presses Home while the reaction and the ball are still running.

- [ ] **Step 1: Exercise the sensor paths in each step**

Replace `step()` in `main/soak.c` with:

```c
static void step(const app_t *app)
{
    display_lock(-1);
    lv_display_trigger_activity(NULL); /* keep idle dimming out of the measurement */
    app_manager_open(app);
    if (app->on_shake) {
        app->on_shake(); /* a reaction that Home cuts short must still clean up after itself */
    }
    display_unlock();
    vTaskDelay(pdMS_TO_TICKS(SOAK_STEP_MS / 2));
    display_lock(-1);
    if (app->on_tilt) {
        app->on_tilt(20.0f, 20.0f); /* shows and rolls the ball */
    }
    display_unlock();
    vTaskDelay(pdMS_TO_TICKS(SOAK_STEP_MS / 2));
    display_lock(-1);
    app_manager_home();
    display_unlock();
    vTaskDelay(pdMS_TO_TICKS(SOAK_STEP_MS));
}
```

- [ ] **Step 2: Build the soak variant**

Run:
```bash
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" build
```
Expected: `Project build complete`.

- [ ] **Step 3: Commit**

```bash
git add main/soak.c
git commit -m "test: soak shakes and tilts each app before Home"
```

---

### Task 9: Device session

Every step here needs the board. Run each flash in the background with `run_in_background`, wait for the app's first log lines, then ask. Keep the build logs under `build-logs/` (gitignored by `build-*/`).

**Files:**
- Modify: `docs/device-checks.md` (append)
- Possibly modify: files named in Step 7 (fallbacks), each with its own `fix:` commit

- [ ] **Step 1: Flash the normal build and log 15 minutes**

Run in the background:
```bash
idf.py build && tools/flash_and_log.sh 900 > build-logs/pet.log 2>&1
```
Wait for `button` handling to be live: the log shows `reset reason` or the menu is up. Because of the 3 s wait, early boot lines may be missing, which is fine.

- [ ] **Step 2: Human step, touch reactions**

Ask (AskUserQuestion, header "Pet: touch"). The question text:

> "M4 check 1 of 3. The new build is on the board. On the menu, swipe to the orange 'Pet the cat' icon and tap it. Please check:
> (a) The screen appears within about half a second of tapping the icon. A big cat fills the top and a blue round button with a white fish sits below it. No yellow ball yet.
> (b) Tap the cat once. It purrs for about 1.5 s with a happy face, 4 pink hearts float up around it, and after about 2 s the plain face is back.
> (c) Rub the cat back and forth with one finger. It giggles, then says 'Hihihi', with a laughing face and hearts.
> (d) Press one finger still on the cat for 5 s. Nothing happens while you hold; when you lift, it purrs (that counts as a tap).
> (e) Tap the fish. Crunch crunch, then 'Njam!'. The cat holds a fish and munches, and the blue circle goes half-transparent for about 2.5 s.
> (f) Tap the fish several times quickly. Each tap restarts the eating; nothing gets stuck.
> Which of these passed?"

multiSelect, options: "(a)-(b) layout and purr", "(c)-(d) rub and still finger", "(e)-(f) fish", "Something looked or sounded wrong (I'll describe)".

- [ ] **Step 3: Human step, shake and tilt**

Ask (AskUserQuestion, header "Pet: motion"):

> "M4 check 2 of 3. Still in Pet the cat. Hold the toy the way she would, tipped toward your face, then press BOOT and reopen Pet the cat so that grip counts as level. Please check:
> (a) Shake it hard once. 'Wheee' and a boing; the cat goes dizzy (spiral eyes, stars, wobbling sideways) for about 2 s. Shake again right away: no second dizzy until about 1.5 s later.
> (b) Walk around holding it normally, and knock it lightly on the table once. The cat should NOT get dizzy and the ball should NOT appear.
> (c) Tilt it to the right from your grip. A yellow ball appears and rolls right, with tiny bell ticks, and the cat's eyes follow it. Then tilt left, toward you (bottom edge down) and away (top edge down): the ball rolls that way each time.
> (d) The ball never leaves the screen; pushed against the edge it slides along it.
> (e) Hold still at your grip for about 3 s. The ball disappears and the eyes look straight ahead.
> (f) Tilt while the cat purrs: the ball still rolls, but no bell ticks until the purr ends.
> Which of these passed?"

multiSelect, options: "(a)-(b) shake and bumps", "(c)-(d) ball direction and edge", "(e)-(f) ball hides, ticks wait", "Something was wrong (I'll describe; say which direction the ball went)".

If (c) fails with the ball going the wrong way, Task 1's mapping is wrong for that axis:
1. Flip or swap `SCREEN_X`/`SCREEN_Y` in `components/platform/sensors.c` accordingly.
2. Commit as `fix: accelerometer axis mapping`, correct the Task 1 table in `docs/device-checks.md`, re-flash and repeat this step.

- [ ] **Step 4: Human step, timeouts, dark screen and Home**

Ask (AskUserQuestion, header "Pet: system"):

> "M4 check 3 of 3.
> (a) In Pet the cat, play ONLY by tilting (roll the ball around, no touching) for 90 s. The app should stay open the whole time.
> (b) Then lean the toy against something so it rests tilted (ball against the edge) and don't touch it. After about 60 s it goes back to the menu by itself.
> (c) Open Pet the cat, press PWR (screen goes dark), then shake it and tilt it. It stays dark and silent. Tap the screen to wake it. Then press BOOT and, on the menu, shake it hard: nothing happens.
> (d) Press BOOT while the cat purrs, while it is dizzy, while it eats and while the ball rolls. Each time the menu is back at once and the purring stops immediately.
> (e) On the menu, do 5 quick double presses of BOOT (two presses in about half a second).
> Which of these passed?"

multiSelect, options: "(a)-(b) tilt play and timeout", "(c) dark screen ignores motion", "(d) Home from every state", "Something was wrong (I'll describe)".

For (e):
1. Run `grep -c "button HOME" build-logs/pet.log` before and after the double presses. The count must grow by exactly 10.
2. If it grows by fewer, the queue did not fix the missed quick second press. Record that as open in `docs/device-checks.md` and continue; the finding goes under Found.

Also check the log for `missing image`, `missing sound`, `Guru Meditation`, `abort()` and `rst:`. None may appear, except `rst:` lines from the planned flashes.

- [ ] **Step 5: FPS on the perf build**

First stop Step 1's background logger (TaskStop on its task id): it holds the serial port. Do the same before every later flash in this task.

Run in the background:
```bash
idf.py -B build-perf -D SDKCONFIG=build-perf/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.perf" build && tools/flash_and_log.sh 240 -B build-perf -D SDKCONFIG=build-perf/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.perf" > build-logs/pet-perf.log 2>&1
```

Ask (AskUserQuestion, header "Pet: FPS"):

> "FPS build is on the board (it logs frame rates). Open Pet the cat, then for about 10 s each: rub the cat continuously; tap the cat repeatedly (hearts); shake it (dizzy wobble); tilt so the ball rolls around over the cat; tap the fish. Say Done when finished, and whether anything looked choppy."

Options: "Done, all smooth" / "Done, something looked choppy (I'll say which)".

Then read the LVGL sysmon lines in `build-logs/pet-perf.log` (e.g. `grep -i fps build-logs/pet-perf.log`). Each activity's samples must be ≥ 24 FPS.

- [ ] **Step 6: Soak**

Run in the background (about 6 minutes):
```bash
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" build && tools/flash_and_log.sh 420 -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" > build-logs/pet-soak.log 2>&1
```
Expected:
- `grep SOAK build-logs/pet-soak.log` ends with `SOAK: PASS: internal delta=… psram delta=… over 50 rounds`;
- a single boot, with no `Guru Meditation`, `abort()` or `assert`.

- [ ] **Step 7: Fallbacks, only if a check failed**

- **FPS < 24 while rubbing only:** the extra CST9217 report read (~2.5 ms per touch sample) runs in every app. Gate it to the menu:
  - add `void touch_raw_set_report(bool on)` in `components/platform/touch_raw.c`/`.h`, which skips the extra report read when off;
  - call it with `true` in `launcher_show()` and with `false` in `app_manager_open()`;
  - re-measure;
  - commit as `fix: read the two-finger report only on the menu`.
- **FPS < 24 during dizzy at scale L:** there is no spec fallback left (the wobble already is the fallback). Record the numbers and ask the user whether to draw the pet cat at scale M instead.

  M centered on the same head center has origin (133, 51) and eyes at (233, 155), with the hit area unchanged. Change `PET_CAT_ORIGIN_X/Y`, `PET_EYES_X/Y` and `CAT_SCALE_L` → `CAT_SCALE_M` in `pet.c`.
- **Any other failure:** use superpowers:systematic-debugging, fix it, re-flash, and repeat the failed step only.

- [ ] **Step 8: Re-flash the normal build**

Run: `tools/flash_and_log.sh 10`, so the board is left on the normal firmware.

- [ ] **Step 9: Human step, kid test (spec §7: after each playable milestone)**

Ask (AskUserQuestion, header "Kid test"):

> "M4 is playable. Spec §7 asks for a kid test after each playable milestone: 15 minutes of play with no adult help, noting every time she asks for help or gets stuck. Do you want to run it now (I'll wait and then record your notes in docs/kid-tests.md) or later (the PR goes up now and the kid test stays open)?"

Options: "Later, open the PR now" / "Now, I'll report back". If now, record the notes in `docs/kid-tests.md` in the same form as the M3 session, and turn each stuck moment into a finding for M8.

- [ ] **Step 10: Record and commit**

Append to `docs/device-checks.md`, filled in from Steps 2–7:

```markdown
## Pet the cat (M4) — 2026-09-24

| Check | Result |
| --- | --- |
| Layout: big cat, fish button, no ball | … |
| Tap: purr ~1.5 s, hearts, back to plain face | … |
| Rub: giggle + "Hihihi", laughing face; a still finger never rubs | … |
| Fish: crunch crunch "Njam!", eating, ring at 50 %; rapid taps | … |
| Shake: "wheee" + boing, dizzy 2 s, 1.5 s lockout; walking and a light knock do nothing | … |
| Tilt from her grip: ball rolls the right way in all four directions, ticks, eyes follow, stays inside the rim | … |
| Level for 3 s hides the ball; no ticks during a reaction | … |
| Tilt-only play 90 s stays open; left tilted, back to the menu after ~60 s | … |
| PWR off: shake and tilt stay dark and silent | … |
| BOOT from purr, dizzy, eating, rolling: menu at once, purr stops | … |
| 5 quick double BOOT presses: 10 `button HOME` lines | … |
| FPS (perf build): rub / hearts / dizzy / rolling / fish | … |
| Soak | … |
| Log | … |
```

```bash
git add docs/device-checks.md
git commit -m "docs: Pet the cat device results"
```

---

### Task 10: Whole-branch review and pull request

**Files:**
- Modify: `README.md` (only the note below)

- [ ] **Step 1: README note on the listener**

In `README.md`, directly after the Build code block, add:

```markdown
`tools/flash_and_log.sh` waits 3 s after flashing and then listens without touching DTR/RTS:
opening the port the usual way resets this board, and a second reset within about a second of the
flasher's can hang early boot (`docs/device-checks.md`). The first ~3 s of boot log are therefore
not shown.
```

Commit: `git add README.md && git commit -m "docs: passive serial listener in the README"`

- [ ] **Step 2: Full verification**

Run:
```bash
sh test/run.sh
idf.py build
idf.py -B build-diag -D SDKCONFIG=build-diag/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.diag" build
idf.py -B build-gallery -D SDKCONFIG=build-gallery/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.gallery;sdkconfig.perf" build
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" build
idf.py -B build-perf -D SDKCONFIG=build-perf/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.perf" build
```
Expected: all tests PASS, five `Project build complete`.

- [ ] **Step 3: Whole-branch review**

Use superpowers:requesting-code-review on `main..HEAD`, against this plan and spec §3.10. Fix Critical and Important findings with superpowers:receiving-code-review. Re-run Step 2 after any code change, and re-flash any variant whose device result a fix could change.

- [ ] **Step 4: Bring in main if it moved**

Run `git fetch origin && git log --oneline HEAD..origin/main`. If `main` gained commits (e.g. PR #3, which appends to `docs/device-checks.md`), run `git merge origin/main`. Resolve the end-of-file conflict in `docs/device-checks.md` by keeping both sections in date order, then re-run `sh test/run.sh && idf.py build`.

- [ ] **Step 5: Push and open the pull request**

Use superpowers:finishing-a-development-branch. The user merges PRs; open one against `main`:

```bash
git push -u origin davor/m4-pet-the-cat
gh pr create --base main --title "M4: Pet the cat" --body-file <file with the summary>
```

The PR body covers:
- what she can do;
- the spec §3.10 decisions (level = grip at open);
- the sensor queue, and whether it fixed the missed double BOOT press;
- the device results table;
- anything left open.

No attribution lines.
