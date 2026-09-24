# M6 Time and Bedtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The toy knows the time and puts itself to bed. It gets the time over NTP, or the parent sets it by hand. At bedtime a dim night scene locks play until the sunrise at wake-up time. The Sleepy clock app shows the day scene.

**Architecture:**
- **Host-tested logic (`components/logic/schedule.c`):** is it night, the hour stars, minute steps and formatting. Everything works on local minutes of the day.
- **Platform (`components/platform`):**
  - `clock.c`: time zone, validity, the manual set, and a background NTP task that brings Wi-Fi up, syncs and releases Wi-Fi again;
  - `pmu.c`: shared AXP2101 access, used by `buttons.c` and the new `power.c` (USB power);
  - `settings.c`: bedtime and wake-up time;
  - `audio.c`: gains a quiet play for the whisper.
- **UI (`components/ui`):**
  - `scenes.c`: the design's night and day scenes;
  - `apps/clock.c`: the Sleepy clock, which replaces the last placeholder;
  - `hold_ring.c`: the parent two-finger hold, moved out of the launcher so the lock can use it too;
  - `bedtime.c`: the lock and the sunrise, checked every second. It sits between `app_manager` (where the menu would show) and `idle` (it takes over the panel);
  - `parent.c`: grows a home screen with two tiles and the Sleep times screen.

**Tech Stack:** ESP-IDF v5.5.5 (C11), LVGL 9.4, `esp_wifi` + `esp_netif_sntp`, `waveshare/esp32_s3_touch_amoled_1_75c` 3.0.0, CMake + Unity host tests under UBSan, Node + resvg for images, sox for sounds.

**Spec:** `docs/superpowers/specs/2026-09-22-cat-toy-design.md`, mainly:
- §3.2: the time source (NTP, manual Now, time zone, validity);
- §3.8 and §3.12: the lock, the sunrise, the Sleepy clock and the parent area in M6;
- §4.3: the file layout;
- §4.7: an invalid time switches the time features off;
- §7: the schedule tests, the leak soak;
- §8: the network is only for NTP, and the credentials stay out of the repository.

The design is the UI source of truth:
- `design/claude-design/Screen.dc.html`: the `clock` view (night, `shh`, morning), `phome`, `pvolume` and `ptimes`, and the `ICON` paths;
- `Main.dc.html`: `clockTap`, `setMorning`, the keyframes `twinkle`, `rise` and `spin`;
- `Screens.dc.html`: the notes on "Clock · night light", "Clock · tap at night", "Clock · wake-up time" and "Parent · sleep times".

## Global Constraints

- **Toolchain:** ESP-IDF v5.5.5, target `esp32s3`.
  - Run every `idf.py` command in a shell that has sourced `~/esp/esp-idf/export.sh`. The port is `/dev/cu.usbmodem101`.
  - After adding a `.c` file to a component, reconfigure each build directory you build (`idf.py reconfigure`, or `-B <dir> ... reconfigure`): sources are globbed at configure time.
- **Build variants:** each is `idf.py -B build-X -D SDKCONFIG=build-X/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.X" build`. They are `diag`, `soak` and `perf`, plus `gallery`, which uses `"sdkconfig.defaults;sdkconfig.gallery;sdkconfig.perf"`. The normal build is `idf.py build`.
- **Assets:**
  - `assets/` is generated and git-ignored. After changing anything under `tools/export_images/` or `tools/build_sounds/`, run `tools/build_assets.sh`.
  - The generated manifest `components/assets/assets_manifest.{c,h}` is committed.
  - The assets partition is 12,517,376 bytes and ~93 % full before M6. `idf.py build` fails if `assets/` does not fit.
- **Board quirks:** opening the serial port resets the board. Always flash with `tools/flash_and_log.sh`. Only the controller flashes (Task 9); implementers build only.
- **Secrets (spec §8):**
  - Wi-Fi credentials live only in the git-ignored `sdkconfig.secrets`.
  - Never read, print, log, copy or commit that file, the generated `sdkconfig`, or the password.
  - Never add credentials to any committed file, test or log line. `clock.c` must not log the SSID or the password.
- **Spec §3.2 and §3.12 (added for M6), summarized:**
  - **Time:**
    - NTP from `pool.ntp.org` at boot and at the wake-up minute, 10 s timeout, silent failure; Wi-Fi off and released afterwards;
    - time zone `CET-1CEST,M3.5.0,M10.5.0/3`;
    - valid = the clock reads 2026-01-01 or later; invalid switches the time features off;
    - a manual set keeps today's date, or uses 2026-01-02 when there is none.
  - **Night:**
    - bedtime to wake, across midnight; equal times mean no night; checked every second;
    - at bedtime an open app closes without the mew and the lock shows;
    - a start inside the night plays the boot animation, then the lock;
    - parent changes take effect when the parent leaves the area.
  - **The lock:**
    - the design's night scene at 35 % brightness: moon, nine stars twinkling 30 → 100 % over 3 s (the design's delays), the sleeping M cat, five hour stars;
    - hour stars = the hours left until wake, rounded up, at most 5;
    - Home does nothing; PWR turns the panel off and on;
    - a touch whispers "Pssst… spavaj" at 30 % of the playing volume, at most once per 5 s, with the shh face for 2.2 s;
    - battery: the panel turns off 30 s after the lock shows, and a touch shows it for 5 s;
    - USB (the AXP2101 "VBUS good" bit): lit all night, shifted ±4 px every 60 s;
    - the two-finger hold opens the parent area. Leaving it returns to the lock while it is night, or goes to the menu (no sunrise) if the times moved out of the night.
  - **The sunrise** (only if the lock is showing at wake time): the sun rises 110 px over 4 s while the brightness goes 10 → 100 %. Then the four-note chime, a happy meow and "Dobro jutro!". The menu follows 60 s later, or on a tap once the sun is up, without the mew.
  - **The Sleepy clock:** the day scene (sun with turning rays, hills, proud M cat). A tap on the cat purrs as in Pet the cat.
  - **The parent area in M6:**
    - the home screen has the Volume and Sleep tiles (Montserrat);
    - Sleep times has Bedtime and Wake up (15-minute steps) and Now (1-minute steps, 10 after a 2 s hold). Holding ± repeats;
    - Now shows "--:--" while the time is invalid, and its first press sets 12:00.
- **Design geometry (Screen.dc.html):**
  - **Night:**
    - the moon is an r 40 `#FFE58A` disc centered (118, 114), with an r 34 black disc centered (138, 100) cut out of it;
    - the stars are 22 px `#FFE58A` four-point stars at (300,70), (360,120), (390,196), (252,44), (196,96), (96,200), (70,268), (380,276) and (170,150), with twinkle delays 0, 800, 1600, 400, 1200, 2000, 600, 1400 and 2400 ms;
    - the sleeping cat is M at origin (133, 176);
    - hour star i (0-4) is 26 px, centered at (233 − 200·cos a, 233 + 200·sin a) with a = 90° + (i − 2)·12°. Lit is fill and stroke `#FFE58A`; unlit is only the stroke, `#6B6B8A`.
  - **Day:**
    - the 220 px sun box at (123, 2) holds 12 rays (r 74 → 98, 10 px round `#FFC83A` lines, turning once per 24 s) and an r 60 `#FFC83A` disc with a 6 px `#D98A00` stroke;
    - the box rises from +110 px over 4 s, ease-in-out;
    - the hills path `M0 404 Q116 346 233 396 Q350 346 466 404 L466 466 L0 466 Z` is `#3FCB6A` with a 6 px `#1D7A3A` stroke;
    - the proud cat is M at origin (133, 196).
  - **Parent:**
    - tiles are 140 px circles at (88, 88) and (238, 88): `#1E1D2A`, 3 px border `#3A3850`, a 36 px icon, the label (20 px) and the value (17 px, `#FFC76B`);
    - "Parents" is at (133, 46); "Home button exits" is at (113, 398);
    - the back button "‹ Parents" is 150 × 52 at (158, 34);
    - the time rows have ± buttons of 76 px at x 70 and 320, with 24 px `#FFC76B` row icons.

## Review Focus

1. **Bedtime arriving mid-transition.** An app's open fade or the menu's fade-in may be under way. The lock must show exactly once, with no mew, and the menu screen must never be deleted (it is permanent). Pinned by Task 7's rules: the tick only takes over the menu when it is active with no load pending, and only app screens are auto-deleted. Checked on the device in Task 9 (C).
2. **The lock's panel on battery vs USB.** Unplugging USB mid-night must turn the panel off 30 s later. Plugging it in must light the scene. PWR must toggle it either way. A touch on a dark lock must light it for 5 s and whisper. Task 7 code, device check Task 9 (D).
3. **An invalid clock** (power lost, no Wi-Fi). There must be no lock, and Now must show "--:--". The first press sets 12:00. A manual time between 00:00 and 00:59 must still count as valid (the 2 January date keeps the UTC reading in 2026). Task 3 and Task 8 code, device check Task 9 (B).
4. **Wi-Fi failing** (wrong password, no network). The sync must give up within 10 s, the UI must never stall, Wi-Fi must be released (heap back to where it was) and the next sync must still work. Task 3 code, device check Task 9 (I).
5. **The schedule across midnight, with equal times and on DST days.** The lock must follow the clock's state, not the arrival of an exact minute, so a skipped wake-up minute still ends the night. Host tests in Task 1.

---

## File structure

| File | Task | Responsibility |
| --- | --- | --- |
| `components/logic/schedule.c/.h`, `test/test_schedule.c` | 1 | Night, hour stars, minute steps, "HH:MM" |
| `components/platform/pmu.c/.h` | 2 | AXP2101 register access shared by buttons and power |
| `components/platform/power.c/.h` | 2 | USB power (VBUS good) |
| `components/platform/buttons.c` | 2 | PWR key through `pmu.h` |
| `components/platform/settings.c/.h` | 2 | Bedtime and wake-up in NVS |
| `components/platform/audio.c/.h` | 2 | `audio_play_quiet()` |
| `components/platform/clock.c/.h` | 3 | Time zone, validity, manual set, NTP task |
| `main/Kconfig.projbuild`, `CMakeLists.txt`, `sdkconfig.defaults`, `README.md` | 3 | Wi-Fi credentials from `sdkconfig.secrets` |
| `main/app_main.c` | 2, 3 | `power_init()`, `clock_init()`, the boot sync |
| `tools/export_images/ui_images.mjs`, `tools/build_sounds/*`, `components/assets/*` | 4 | Stars, hills, parent icons; the wake chime and the sunrise sound |
| `components/ui/scenes.c/.h` | 5 | Night and day scenes |
| `components/ui/apps/clock.c` (replaces `placeholder.c`) | 5 | Sleepy clock app |
| `components/ui/hold_ring.c/.h`, `launcher.c/.h` | 6 | The two-finger hold on any screen |
| `components/ui/bedtime.c/.h`, `app_manager.c/.h`, `idle.c/.h` | 7 | The lock and the sunrise |
| `components/ui/parent.c/.h` | 8 | Parent home, Max volume, Sleep times |
| `components/ui/include/tokens.h` | 5, 7, 8 | Design constants |
| `docs/device-checks.md` | 9 | M6 device results |

---

### Task 1: Schedule logic

**Files:**
- Create: `components/logic/include/schedule.h`
- Create: `components/logic/schedule.c`
- Test: `test/test_schedule.c`

**Interfaces:**
- Consumes: nothing.
- Produces (used by Tasks 2, 7 and 8):
  - `bool schedule_is_night(uint16_t now, uint16_t bed, uint16_t wake)`
  - `uint16_t schedule_minutes_until(uint16_t now, uint16_t target)`
  - `uint8_t schedule_stars_left(uint16_t now, uint16_t wake)`
  - `uint16_t schedule_step(uint16_t min, int32_t delta)`
  - `void schedule_format(uint16_t min, char out[6])`
  - `uint16_t schedule_now_step(uint32_t held_ms)`
  - the constants `SCHEDULE_DAY_MIN` (1440), `SCHEDULE_STARS` (5), `SCHEDULE_SLEEP_STEP` (15), `SCHEDULE_NOW_START` (720), `SCHEDULE_NOW_FAST_MS` (2000) and `SCHEDULE_NOW_FAST_STEP` (10).

- [ ] **Step 1: Write the failing test**

Create `test/test_schedule.c`:

```c
#include "schedule.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define HM(h, m) ((uint16_t)((h) * 60 + (m)))

static void test_the_default_night_runs_across_midnight(void)
{
    const uint16_t bed = HM(19, 30), wake = HM(7, 0);
    TEST_ASSERT_FALSE(schedule_is_night(HM(19, 29), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(19, 30), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(23, 59), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(0, 0), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(6, 59), bed, wake));
    TEST_ASSERT_FALSE(schedule_is_night(HM(7, 0), bed, wake));
    TEST_ASSERT_FALSE(schedule_is_night(HM(12, 0), bed, wake));
}

static void test_a_night_that_starts_after_midnight(void)
{
    const uint16_t bed = HM(0, 30), wake = HM(6, 0);
    TEST_ASSERT_FALSE(schedule_is_night(HM(23, 0), bed, wake));
    TEST_ASSERT_FALSE(schedule_is_night(HM(0, 29), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(0, 30), bed, wake));
    TEST_ASSERT_TRUE(schedule_is_night(HM(5, 59), bed, wake));
    TEST_ASSERT_FALSE(schedule_is_night(HM(6, 0), bed, wake));
}

static void test_equal_times_mean_no_night(void)
{
    for (uint16_t now = 0; now < SCHEDULE_DAY_MIN; now += 15) {
        TEST_ASSERT_FALSE(schedule_is_night(now, HM(7, 0), HM(7, 0)));
    }
}

/* DST (spec §7): the lock follows the clock's state, not the arrival of an exact minute. On the spring
 * change the clock jumps from 01:59 to 03:00, so a 02:30 wake-up minute never shows, and the night must
 * still end. On the autumn change 02:00-02:59 comes twice, so that hour is night again. */
static void test_dst_jumps_still_end_the_night(void)
{
    const uint16_t bed = HM(19, 30), wake = HM(2, 30);
    TEST_ASSERT_TRUE(schedule_is_night(HM(1, 59), bed, wake));
    TEST_ASSERT_FALSE(schedule_is_night(HM(3, 0), bed, wake)); /* spring: 01:59 -> 03:00 */
    TEST_ASSERT_TRUE(schedule_is_night(HM(2, 0), bed, wake));  /* autumn: 02:59 -> 02:00 again */
}

static void test_minutes_until_wraps_forward(void)
{
    TEST_ASSERT_EQUAL_UINT16(690, schedule_minutes_until(HM(19, 30), HM(7, 0)));
    TEST_ASSERT_EQUAL_UINT16(1, schedule_minutes_until(HM(6, 59), HM(7, 0)));
    TEST_ASSERT_EQUAL_UINT16(0, schedule_minutes_until(HM(7, 0), HM(7, 0)));
    TEST_ASSERT_EQUAL_UINT16(1439, schedule_minutes_until(HM(7, 1), HM(7, 0)));
}

static void test_hour_stars_round_up_and_stop_at_five(void)
{
    const uint16_t wake = HM(7, 0);
    TEST_ASSERT_EQUAL_UINT8(5, schedule_stars_left(HM(19, 30), wake)); /* 11 h 30 left */
    TEST_ASSERT_EQUAL_UINT8(5, schedule_stars_left(HM(2, 0), wake));   /* 5 h */
    TEST_ASSERT_EQUAL_UINT8(5, schedule_stars_left(HM(2, 1), wake));   /* 4 h 59 */
    TEST_ASSERT_EQUAL_UINT8(4, schedule_stars_left(HM(3, 0), wake));   /* 4 h */
    TEST_ASSERT_EQUAL_UINT8(1, schedule_stars_left(HM(6, 0), wake));   /* 1 h */
    TEST_ASSERT_EQUAL_UINT8(1, schedule_stars_left(HM(6, 59), wake));  /* 1 min */
    TEST_ASSERT_EQUAL_UINT8(0, schedule_stars_left(HM(7, 0), wake));   /* wake time */
}

static void test_steps_wrap_around_the_day(void)
{
    TEST_ASSERT_EQUAL_UINT16(HM(23, 45), schedule_step(HM(0, 0), -15));
    TEST_ASSERT_EQUAL_UINT16(HM(0, 0), schedule_step(HM(23, 45), 15));
    TEST_ASSERT_EQUAL_UINT16(HM(12, 1), schedule_step(HM(12, 0), 1));
    TEST_ASSERT_EQUAL_UINT16(HM(23, 55), schedule_step(HM(0, 5), -10));
    TEST_ASSERT_EQUAL_UINT16(HM(19, 30), schedule_step(HM(19, 30), 1440));
}

static void test_format_is_two_digit_hours_and_minutes(void)
{
    char s[6];
    schedule_format(0, s);
    TEST_ASSERT_EQUAL_STRING("00:00", s);
    schedule_format(HM(7, 5), s);
    TEST_ASSERT_EQUAL_STRING("07:05", s);
    schedule_format(HM(19, 30), s);
    TEST_ASSERT_EQUAL_STRING("19:30", s);
    schedule_format(HM(23, 59), s);
    TEST_ASSERT_EQUAL_STRING("23:59", s);
}

static void test_now_speeds_up_after_two_seconds(void)
{
    TEST_ASSERT_EQUAL_UINT16(1, schedule_now_step(0));
    TEST_ASSERT_EQUAL_UINT16(1, schedule_now_step(1999));
    TEST_ASSERT_EQUAL_UINT16(10, schedule_now_step(2000));
    TEST_ASSERT_EQUAL_UINT16(10, schedule_now_step(60000));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_default_night_runs_across_midnight);
    RUN_TEST(test_a_night_that_starts_after_midnight);
    RUN_TEST(test_equal_times_mean_no_night);
    RUN_TEST(test_dst_jumps_still_end_the_night);
    RUN_TEST(test_minutes_until_wraps_forward);
    RUN_TEST(test_hour_stars_round_up_and_stop_at_five);
    RUN_TEST(test_steps_wrap_around_the_day);
    RUN_TEST(test_format_is_two_digit_hours_and_minutes);
    RUN_TEST(test_now_speeds_up_after_two_seconds);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `sh test/run.sh -R test_schedule`
Expected: the build fails with `schedule.h: No such file or directory`.

- [ ] **Step 3: Write the header**

Create `components/logic/include/schedule.h`:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Bedtime and wake-up rules (spec §3.8, §3.12). Times are local minutes of the day, 0-1439. */
#define SCHEDULE_DAY_MIN       1440
#define SCHEDULE_STARS         5    /* hour stars on the lock */
#define SCHEDULE_SLEEP_STEP    15   /* Bedtime and Wake up move in 15-minute steps */
#define SCHEDULE_NOW_START     720  /* Now's first press while the clock is unset: 12:00 */
#define SCHEDULE_NOW_FAST_MS   2000 /* ± held this long: Now steps by 10 minutes */
#define SCHEDULE_NOW_FAST_STEP 10

/* Night runs from bed to wake, across midnight when bed > wake; bed == wake means no night. */
bool schedule_is_night(uint16_t now, uint16_t bed, uint16_t wake);
/* Minutes from now forward to target, 0-1439 (0 when they are equal). */
uint16_t schedule_minutes_until(uint16_t now, uint16_t target);
/* Lit hour stars: the hours left until wake, rounded up, at most SCHEDULE_STARS. */
uint8_t schedule_stars_left(uint16_t now, uint16_t wake);
/* min + delta minutes, wrapped into 0-1439. */
uint16_t schedule_step(uint16_t min, int32_t delta);
/* Writes "HH:MM" and a terminating NUL into out. */
void schedule_format(uint16_t min, char out[6]);
/* The Now row's step while ± has been held for held_ms: 1 minute, then 10 after 2 s. */
uint16_t schedule_now_step(uint32_t held_ms);
```

- [ ] **Step 4: Write the implementation**

Create `components/logic/schedule.c`:

```c
#include "schedule.h"

bool schedule_is_night(uint16_t now, uint16_t bed, uint16_t wake)
{
    if (bed == wake) {
        return false;
    }
    if (bed < wake) {
        return now >= bed && now < wake; /* a night that starts after midnight */
    }
    return now >= bed || now < wake; /* across midnight */
}

uint16_t schedule_minutes_until(uint16_t now, uint16_t target)
{
    return (uint16_t)(((uint32_t)target + SCHEDULE_DAY_MIN - now) % SCHEDULE_DAY_MIN);
}

uint8_t schedule_stars_left(uint16_t now, uint16_t wake)
{
    const uint32_t hours = ((uint32_t)schedule_minutes_until(now, wake) + 59) / 60;
    return (uint8_t)(hours > SCHEDULE_STARS ? SCHEDULE_STARS : hours);
}

uint16_t schedule_step(uint16_t min, int32_t delta)
{
    const int32_t v = ((int32_t)min + delta) % SCHEDULE_DAY_MIN;
    return (uint16_t)(v < 0 ? v + SCHEDULE_DAY_MIN : v);
}

void schedule_format(uint16_t min, char out[6])
{
    const unsigned m = min % SCHEDULE_DAY_MIN;
    const unsigned h = m / 60, mm = m % 60;
    out[0] = (char)('0' + h / 10);
    out[1] = (char)('0' + h % 10);
    out[2] = ':';
    out[3] = (char)('0' + mm / 10);
    out[4] = (char)('0' + mm % 10);
    out[5] = '\0';
}

uint16_t schedule_now_step(uint32_t held_ms)
{
    return held_ms >= SCHEDULE_NOW_FAST_MS ? SCHEDULE_NOW_FAST_STEP : 1;
}
```

- [ ] **Step 5: Run the tests to verify they pass**

Run: `sh test/run.sh`
Expected: all 13 suites pass (the 12 existing ones plus `test_schedule`), with no warnings.

- [ ] **Step 6: Build the firmware** (the logic component is globbed, so reconfigure first)

Run: `. ~/esp/esp-idf/export.sh >/dev/null 2>&1 && idf.py reconfigure >/dev/null && idf.py build 2>&1 | tail -3`
Expected: `Project build complete.`

- [ ] **Step 7: Commit**

```bash
git add components/logic/include/schedule.h components/logic/schedule.c test/test_schedule.c
git commit -m "feat: schedule rules for bedtime, wake-up and the hour stars"
```

---

### Task 2: Platform pieces for the lock (PMU, USB power, sleep times, quiet play)

**Files:**
- Create: `components/platform/include/pmu.h`, `components/platform/pmu.c`
- Create: `components/platform/include/power.h`, `components/platform/power.c`
- Modify: `components/platform/buttons.c` (the PMU code moves to `pmu.c`)
- Modify: `components/platform/include/settings.h`, `components/platform/settings.c`
- Modify: `components/platform/include/audio.h`, `components/platform/audio.c`
- Modify: `main/app_main.c`

**Interfaces:**
- Consumes: `SCHEDULE_DAY_MIN` from `schedule.h` (Task 1). The platform component already has `PRIV_REQUIRES logic`.
- Produces:
  - `bool pmu_init(void)`, `esp_err_t pmu_read(uint8_t reg, uint8_t *val)`, `esp_err_t pmu_write(uint8_t reg, uint8_t val)`
  - `void power_init(void)`, `bool power_on_usb(void)` (Task 7)
  - `uint16_t settings_bedtime(void)`, `void settings_set_bedtime(uint16_t min)`, `uint16_t settings_wake(void)`, `void settings_set_wake(uint16_t min)`, and the defaults `SETTINGS_BEDTIME_DEFAULT` (1170) and `SETTINGS_WAKE_DEFAULT` (420) (Tasks 7 and 8)
  - `void audio_play_quiet(sound_id_t id, uint8_t gain_pct)` (Task 7)

- [ ] **Step 1: Create the shared PMU access**

Create `components/platform/include/pmu.h`:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* The AXP2101 power manager on the board support package's I2C bus. pmu_init() adds it once; later
 * calls return the same answer. Reads and writes work from any task (the I2C driver locks the bus)
 * and fail with ESP_ERR_INVALID_STATE when the PMU is missing. */
bool pmu_init(void);
esp_err_t pmu_read(uint8_t reg, uint8_t *val);
esp_err_t pmu_write(uint8_t reg, uint8_t val);
```

Create `components/platform/pmu.c`:

```c
#include "pmu.h"
#include "bsp/esp-bsp.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define PMU_ADDR       0x34
#define PMU_TIMEOUT_MS 50

static const char *TAG = "pmu";
static i2c_master_dev_handle_t s_dev;
static bool s_tried;

bool pmu_init(void)
{
    if (s_tried) {
        return s_dev != NULL;
    }
    s_tried = true;
    i2c_master_bus_handle_t bus = bsp_i2c_get_handle();
    if (i2c_master_probe(bus, PMU_ADDR, PMU_TIMEOUT_MS) != ESP_OK) {
        ESP_LOGW(TAG, "PMU missing: no PWR button and no USB detection");
        return false;
    }
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PMU_ADDR,
        .scl_speed_hz = 100000,
    };
    if (i2c_master_bus_add_device(bus, &cfg, &s_dev) != ESP_OK) {
        ESP_LOGW(TAG, "PMU could not be added to the I2C bus");
        s_dev = NULL;
        return false;
    }
    return true;
}

esp_err_t pmu_read(uint8_t reg, uint8_t *val)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    return i2c_master_transmit_receive(s_dev, &reg, 1, val, 1, PMU_TIMEOUT_MS);
}

esp_err_t pmu_write(uint8_t reg, uint8_t val)
{
    if (!s_dev) {
        return ESP_ERR_INVALID_STATE;
    }
    const uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), PMU_TIMEOUT_MS);
}
```

- [ ] **Step 2: Move buttons.c onto pmu.h**

In `components/platform/buttons.c`:
- remove the includes `bsp/esp-bsp.h` and `driver/i2c_master.h`, and add `#include "pmu.h"`;
- remove `PMU_ADDR`, `PMU_TIMEOUT_MS`, `static i2c_master_dev_handle_t s_pmu;` and the two static `pmu_read()`/`pmu_write()` functions;
- replace them with `static bool s_pmu; /* the PWR key flag is enabled */`;
- replace `pmu_init()` with the version below, and call `pmu_key_init()` from `buttons_init()` where it called `pmu_init()`. `power_pressed()` stays as it is: it now calls the shared `pmu_read()`/`pmu_write()`, and `if (!s_pmu ...` still reads the bool.

```c
static void pmu_key_init(void)
{
    uint8_t en = 0;
    if (!pmu_init() || pmu_read(PMU_REG_INTEN2, &en) != ESP_OK ||
        pmu_write(PMU_REG_INTEN2, en | PMU_PKEY_SHORT) != ESP_OK) {
        ESP_LOGW(TAG, "PMU not responding; PWR button disabled");
        return;
    }
    s_pmu = true;
    /* Drop a press latched before boot (e.g. the click that powered the board on). */
    if (pmu_write(PMU_REG_INTSTS2, PMU_PKEY_SHORT) != ESP_OK) {
        ESP_LOGW(TAG, "PMU flag clear failed; a PWR press from before boot may count once");
    }
}
```

Keep the comment block about the PWRON key and the defines `PMU_REG_INTEN2`, `PMU_REG_INTSTS2`, `PMU_PKEY_SHORT` and `PMU_POLL_EVERY`.

- [ ] **Step 3: Add USB power detection**

Create `components/platform/include/power.h`:

```c
#pragma once
#include <stdbool.h>

/* Power state (spec §3.12). M7 adds the battery level, charging and deep sleep. */
void power_init(void);    /* call once, after display_init() (the I2C bus is up) */
bool power_on_usb(void);  /* the AXP2101 "VBUS good" bit; false when unknown. One I2C read. */
```

Create `components/platform/power.c`:

```c
#include "power.h"
#include "esp_log.h"
#include "pmu.h"

#define PMU_REG_STATUS1 0x00
#define PMU_VBUS_GOOD   (1u << 5) /* XPowersLib isVbusGood(): status 1, bit 5 */

static const char *TAG = "power";

void power_init(void)
{
    if (pmu_init()) {
        ESP_LOGI(TAG, "USB power: %s", power_on_usb() ? "yes" : "no");
    }
}

bool power_on_usb(void)
{
    uint8_t sts = 0;
    return pmu_read(PMU_REG_STATUS1, &sts) == ESP_OK && (sts & PMU_VBUS_GOOD) != 0;
}
```

- [ ] **Step 4: Store bedtime and wake-up**

In `components/platform/include/settings.h`, add below the volume defines:

```c
#define SETTINGS_BEDTIME_DEFAULT 1170 /* 19:30, minutes of the day (spec §4.5) */
#define SETTINGS_WAKE_DEFAULT    420  /* 07:00 */
```

and at the end:

```c
uint16_t settings_bedtime(void); /* minute of the day, 0-1439 */
void settings_set_bedtime(uint16_t min);
uint16_t settings_wake(void);
void settings_set_wake(uint16_t min);
```

In `components/platform/settings.c`:
- add `#include "schedule.h"`;
- add `static uint16_t s_bed = SETTINGS_BEDTIME_DEFAULT, s_wake = SETTINGS_WAKE_DEFAULT;` next to the other statics;
- add this helper above `settings_init()`:

```c
static uint16_t valid_min(uint16_t v, uint16_t fallback) { return v < SCHEDULE_DAY_MIN ? v : fallback; }
```

- in `settings_init()`, after the existing `nvs_get_*` calls, read the two keys and validate them; after the existing log line, log them:

```c
    nvs_get_u16(s_nvs, "bed", &s_bed);
    nvs_get_u16(s_nvs, "wake", &s_wake);
    s_bed = valid_min(s_bed, SETTINGS_BEDTIME_DEFAULT);
    s_wake = valid_min(s_wake, SETTINGS_WAKE_DEFAULT);
```

```c
    ESP_LOGI(TAG, "bedtime %02u:%02u, wake-up %02u:%02u", s_bed / 60, s_bed % 60, s_wake / 60, s_wake % 60);
```

- and add at the end of the file:

```c
uint16_t settings_bedtime(void) { return s_bed; }
void settings_set_bedtime(uint16_t min)
{
    s_bed = valid_min(min, SETTINGS_BEDTIME_DEFAULT);
    commit(nvs_set_u16(s_nvs, "bed", s_bed), "bed");
}

uint16_t settings_wake(void) { return s_wake; }
void settings_set_wake(uint16_t min)
{
    s_wake = valid_min(min, SETTINGS_WAKE_DEFAULT);
    commit(nvs_set_u16(s_nvs, "wake", s_wake), "wake");
}
```

- [ ] **Step 5: Add the quiet play**

In `components/platform/include/audio.h`, below `void audio_play(sound_id_t id);`:

```c
/* Like audio_play(), at gain_pct % (at most 100) of the current volume: a mixer gain, so the parent's
 * cap still applies (spec §3.12: the bedtime whisper at 30 %). */
void audio_play_quiet(sound_id_t id, uint8_t gain_pct);
```

In `components/platform/audio.c`:
- add a field to `cmd_t`, after `size_t n;`:

```c
    uint8_t gain; /* CMD_PLAY: % of the volume */
```

- add an audio-task-only static next to `s_fx_session`:

```c
static uint8_t s_fx_gain = 100; /* % of the volume for the playing fx clip */
```

- in `handle()`, `case CMD_PLAY`, set the gain with the clip:

```c
        if (clip && !s_rec) {
            fx_finish(true);
            s_fx = clip->pcm;
            s_fx_n = clip->n;
            s_fx_gain = c->gain;
        }
```

- in `case CMD_PLAY_PCM`, after `s_fx_session = c->session;`, add `s_fx_gain = 100;`;
- in `play_chunk()`, replace `v += s_fx[s_fx_pos++];` with:

```c
            v += (int32_t)s_fx[s_fx_pos++] * s_fx_gain / 100;
```

- replace `audio_play()` and add `audio_play_quiet()`:

```c
void audio_play(sound_id_t id) { send((cmd_t){.kind = CMD_PLAY, .arg = (uint32_t)id, .gain = 100}); }
void audio_play_quiet(sound_id_t id, uint8_t gain_pct)
{
    send((cmd_t){.kind = CMD_PLAY, .arg = (uint32_t)id, .gain = gain_pct > 100 ? 100 : gain_pct});
}
```

Every `CMD_PLAY` must set `.gain`: the only senders are these two functions. Check with `grep -n "CMD_PLAY," components/platform/audio.c`. The expected output is the `case` label, the enum entry and these two senders.

- [ ] **Step 6: Call power_init() at boot**

In `main/app_main.c`, add `#include "power.h"`. In the normal branch (the `#else` after `CONFIG_CAT_DEV_GALLERY`), add `power_init();` as the first line, before `cat_set_variant(...)`.

- [ ] **Step 7: Build**

Run:
```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1 && idf.py reconfigure >/dev/null && idf.py build 2>&1 | tail -3
idf.py build 2>&1 | grep -E "warning:" | grep -E "components/platform|main/" || echo "no warnings"
sh test/run.sh | tail -3
```
Expected: `Project build complete.`, `no warnings`, and all host tests pass.

- [ ] **Step 8: Commit**

```bash
git add components/platform main/app_main.c
git commit -m "feat: shared PMU access, USB power, sleep times and a quiet play"
```

---

### Task 3: The clock and NTP

**Files:**
- Create: `components/platform/include/clock.h`, `components/platform/clock.c`
- Modify: `components/platform/CMakeLists.txt`
- Modify: `main/Kconfig.projbuild`
- Modify: `CMakeLists.txt` (top level)
- Modify: `sdkconfig.defaults`
- Modify: `main/app_main.c`
- Modify: `README.md`
- Local, not committed: the generated `sdkconfig` files (Step 5)

**Interfaces:**
- Consumes: nothing from earlier tasks.
- Produces (Tasks 7 and 8):
  - `esp_err_t clock_init(void)`
  - `bool clock_valid(void)`
  - `uint16_t clock_now_min(void)`
  - `void clock_set_now_min(uint16_t min)`
  - `void clock_sync_start(void)`

- [ ] **Step 1: Write the header**

Create `components/platform/include/clock.h`:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

/* The time (spec §3.2). The 1.75C has no RTC chip: the ESP32-S3 keeps the time through restarts, not
 * through a power loss. */
#define CLOCK_TZ              "CET-1CEST,M3.5.0,M10.5.0/3" /* Europe/Zagreb */
#define CLOCK_VALID_FROM      1767225600                   /* 2026-01-01 00:00 UTC */
#define CLOCK_NTP_SERVER      "pool.ntp.org"
#define CLOCK_SYNC_TIMEOUT_MS 10000 /* Wi-Fi connect and NTP together */

/* Sets the time zone and, when Wi-Fi is configured, the network stack. Call after settings_init()
 * (Wi-Fi keeps its calibration in NVS). */
esp_err_t clock_init(void);
/* The clock reads 2026 or later: only an NTP sync or a manual set since power-on makes this true. */
bool clock_valid(void);
/* Local minute of the day, 0-1439. Meaningless while !clock_valid(). */
uint16_t clock_now_min(void);
/* Sets the local time of day to min (seconds 0). Keeps today's date, or uses 2026-01-02 when the clock
 * has no date yet: that date stays in 2026 in UTC even at 00:00, so the time counts as valid. */
void clock_set_now_min(uint16_t min);
/* Starts one NTP sync in the background when Wi-Fi is configured and no sync is running. Any task. */
void clock_sync_start(void);
```

- [ ] **Step 2: Write the implementation**

Create `components/platform/clock.c`:

```c
#include "clock.h"
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define SYNC_STACK   6144
#define BIT_GOT_IP   BIT0
#define BIT_NO_WIFI  BIT1

static const char *TAG = "clock";
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static bool s_running;          /* a sync task exists; under s_mux */
static bool s_net_ready;        /* Wi-Fi is configured and the network stack is up */
static EventGroupHandle_t s_events;
static int64_t s_last_sync_us;  /* esp_timer time of the last successful sync, 0 before */

static void log_time(const char *what)
{
    const time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    char buf[24];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    ESP_LOGI(TAG, "%s: %s%s", what, buf, clock_valid() ? "" : " (not valid)");
}

esp_err_t clock_init(void)
{
    setenv("TZ", CLOCK_TZ, 1);
    tzset();
    log_time("time at boot");
    if (strlen(CONFIG_CAT_WIFI_SSID) == 0) {
        ESP_LOGI(TAG, "no Wi-Fi configured: the parent sets the clock by hand");
        return ESP_OK;
    }
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif");
    const esp_err_t err = esp_event_loop_create_default();
    ESP_RETURN_ON_FALSE(err == ESP_OK || err == ESP_ERR_INVALID_STATE, err, TAG, "event loop");
    s_events = xEventGroupCreate();
    ESP_RETURN_ON_FALSE(s_events, ESP_ERR_NO_MEM, TAG, "event group");
    s_net_ready = true;
    return ESP_OK;
}

bool clock_valid(void) { return time(NULL) >= CLOCK_VALID_FROM; }

uint16_t clock_now_min(void)
{
    const time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    return (uint16_t)(tm.tm_hour * 60 + tm.tm_min);
}

void clock_set_now_min(uint16_t min)
{
    const time_t now = time(NULL);
    struct tm tm;
    if (clock_valid()) {
        localtime_r(&now, &tm);
    } else {
        tm = (struct tm){.tm_year = 2026 - 1900, .tm_mon = 0, .tm_mday = 2};
    }
    tm.tm_hour = (min % 1440) / 60;
    tm.tm_min = min % 60;
    tm.tm_sec = 0;
    tm.tm_isdst = -1; /* mktime picks CET or CEST for that date */
    const struct timeval tv = {.tv_sec = mktime(&tm)};
    settimeofday(&tv, NULL);
    log_time("set by hand");
}

static void on_net_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    (void)data;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_events, BIT_NO_WIFI); /* no network or a wrong password: give up this time */
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(s_events, BIT_GOT_IP);
    }
}

static TickType_t ticks_left(TickType_t deadline)
{
    const TickType_t now = xTaskGetTickCount();
    return (int32_t)(deadline - now) > 0 ? deadline - now : 0;
}

/* Wi-Fi on, one NTP exchange, then Wi-Fi stopped and released. On success *correction_us is how far the
 * clock moved beyond the real time that passed: the drift since the last sync (spec §3.2). */
static bool sync_once(int64_t *correction_us)
{
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(CLOCK_SYNC_TIMEOUT_MS);
    xEventGroupClearBits(s_events, BIT_GOT_IP | BIT_NO_WIFI);
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();
    const wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    if (!netif || esp_wifi_init(&init) != ESP_OK) {
        if (netif) {
            esp_netif_destroy_default_wifi(netif);
        }
        return false;
    }
    esp_event_handler_instance_t on_wifi = NULL, on_ip = NULL;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_net_event, NULL, &on_wifi);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_net_event, NULL, &on_ip);
    wifi_config_t cfg = {0};
    strlcpy((char *)cfg.sta.ssid, CONFIG_CAT_WIFI_SSID, sizeof(cfg.sta.ssid));
    strlcpy((char *)cfg.sta.password, CONFIG_CAT_WIFI_PASSWORD, sizeof(cfg.sta.password));
    bool ok = false;
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) == ESP_OK && esp_wifi_set_mode(WIFI_MODE_STA) == ESP_OK &&
        esp_wifi_set_config(WIFI_IF_STA, &cfg) == ESP_OK && esp_wifi_start() == ESP_OK) {
        const EventBits_t bits =
            xEventGroupWaitBits(s_events, BIT_GOT_IP | BIT_NO_WIFI, pdFALSE, pdFALSE, ticks_left(deadline));
        if (bits & BIT_GOT_IP) {
            esp_sntp_config_t sntp = ESP_NETIF_SNTP_DEFAULT_CONFIG(CLOCK_NTP_SERVER);
            if (esp_netif_sntp_init(&sntp) == ESP_OK) {
                struct timeval t0, t1;
                gettimeofday(&t0, NULL);
                const int64_t m0 = esp_timer_get_time();
                ok = esp_netif_sntp_sync_wait(ticks_left(deadline)) == ESP_OK;
                if (ok) {
                    gettimeofday(&t1, NULL);
                    const int64_t m1 = esp_timer_get_time();
                    *correction_us = ((int64_t)(t1.tv_sec - t0.tv_sec) * 1000000 + (t1.tv_usec - t0.tv_usec)) -
                                     (m1 - m0);
                }
                esp_netif_sntp_deinit();
            }
        }
        esp_wifi_stop();
    }
    esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, on_ip);
    esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi);
    esp_wifi_deinit();
    esp_netif_destroy_default_wifi(netif);
    return ok;
}

static void sync_task(void *arg)
{
    (void)arg;
    const size_t int0 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t ps0 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    int64_t correction_us = 0;
    if (sync_once(&correction_us)) {
        const int64_t now_us = esp_timer_get_time();
        ESP_LOGI(TAG, "NTP: corrected by %+lld ms, %lld s after the previous sync", (long long)(correction_us / 1000),
                 s_last_sync_us ? (long long)((now_us - s_last_sync_us) / 1000000) : -1LL);
        s_last_sync_us = now_us;
        log_time("time now");
    } else {
        ESP_LOGW(TAG, "NTP sync failed; keeping the current time");
    }
    const size_t int1 = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    const size_t ps1 = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    ESP_LOGI(TAG, "after the sync: free internal %u (%+ld), psram %u (%+ld)", (unsigned)int1, (long)int1 - (long)int0,
             (unsigned)ps1, (long)ps1 - (long)ps0);
    portENTER_CRITICAL(&s_mux);
    s_running = false;
    portEXIT_CRITICAL(&s_mux);
    vTaskDelete(NULL);
}

void clock_sync_start(void)
{
    if (!s_net_ready) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    const bool start = !s_running;
    s_running = true;
    portEXIT_CRITICAL(&s_mux);
    if (!start) {
        return;
    }
    if (xTaskCreatePinnedToCore(sync_task, "ntp", SYNC_STACK, NULL, 2, NULL, 0) != pdPASS) {
        ESP_LOGE(TAG, "no memory for the NTP task");
        portENTER_CRITICAL(&s_mux);
        s_running = false;
        portEXIT_CRITICAL(&s_mux);
    }
}
```

The `esp_timer` time runs on the same crystal as the system time while the chip is awake (`CONFIG_ESP_TIME_FUNCS_USE_ESP_TIMER=y`). So `(t1 − t0) − (m1 − m0)` is exactly the jump NTP applied. On the first sync after a power loss that jump is decades. The log then shows `-1` as the time since the previous sync, which marks it as the first.

- [ ] **Step 3: Declare the new component dependencies**

Replace `components/platform/CMakeLists.txt` with:

```cmake
idf_component_register(SRC_DIRS "." INCLUDE_DIRS "include"
                       REQUIRES assets espressif__esp_lcd_touch
                       PRIV_REQUIRES logic nvs_flash esp_driver_gpio esp_wifi esp_netif esp_event esp_timer)
```

- [ ] **Step 4: Add the credentials, empty by default**

In `main/Kconfig.projbuild`, add before `endmenu`:

```
    config CAT_WIFI_SSID
        string "Wi-Fi network for setting the clock (NTP)"
        default ""
        help
            Spec 3.2 and 8. Set this and CAT_WIFI_PASSWORD only in the git-ignored sdkconfig.secrets
            (README, "Wi-Fi for the clock"), never in a committed file. Empty: no NTP, and the parent
            sets the clock by hand.
    config CAT_WIFI_PASSWORD
        string "Wi-Fi password"
        default ""
```

Replace the top-level `CMakeLists.txt` with:

```cmake
cmake_minimum_required(VERSION 3.16)
# Wi-Fi credentials (spec §8) come from the git-ignored sdkconfig.secrets when it exists. Builds that pass
# their own SDKCONFIG_DEFAULTS (the diag, soak, perf and gallery variants) list it themselves if they need it.
if(NOT DEFINED SDKCONFIG_DEFAULTS AND EXISTS "${CMAKE_CURRENT_LIST_DIR}/sdkconfig.secrets")
    set(SDKCONFIG_DEFAULTS "sdkconfig.defaults;sdkconfig.secrets")
endif()
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cat_toy)
```

(`tools/cmake/project.cmake` reads `SDKCONFIG_DEFAULTS` as a variable before falling back to `sdkconfig.defaults`. See lines 660-685 of that file.)

Append to `sdkconfig.defaults`:

```
# Wi-Fi for the clock's NTP sync (spec §3.2): Wi-Fi and lwIP buffers may use PSRAM
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
```

Add this section to `README.md`, after "Developer builds":

```markdown
## Wi-Fi for the clock (optional)

The toy sets its clock over NTP at boot and at the wake-up minute when it knows a Wi-Fi network
(spec §3.2). Without one, set the time in the parent area (Sleep → Now). Put the network in
`sdkconfig.secrets` next to `sdkconfig.defaults`. The file is git-ignored; never commit it:

    CONFIG_CAT_WIFI_SSID="network name"
    CONFIG_CAT_WIFI_PASSWORD="password"

A build directory reads it only when it creates its `sdkconfig`, so delete `sdkconfig` after creating or
changing the file, then build. Developer builds that pass `SDKCONFIG_DEFAULTS` read it only if you add
it to their list.
```

- [ ] **Step 5: Bring the existing generated configs up to date** (git-ignored; they already say "is not set", which the new default cannot override)

Run:
```bash
for f in sdkconfig build-*/sdkconfig; do
  [ -f "$f" ] && sed -i '' 's/^# CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP is not set$/CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y/' "$f"
done
grep -c "^CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y" sdkconfig build-soak/sdkconfig
```
Expected: `sdkconfig:1` and `build-soak/sdkconfig:1`. Do not open or print any other line of these files: once a secrets file exists, they hold the password.

- [ ] **Step 6: Start the clock at boot**

In `main/app_main.c`:
- add `#include "clock.h"`;
- right after `ESP_ERROR_CHECK(settings_init());`, add:

```c
    if (clock_init() != ESP_OK) {
        ESP_LOGE(TAG, "network stack unavailable: no NTP");
    }
```

- in the normal branch, after the `sensors_start()` check, add:

```c
    clock_sync_start(); /* NTP at boot, in the background (spec §3.2) */
```

- [ ] **Step 7: Build all variants**

Run:
```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1 && idf.py reconfigure >/dev/null && idf.py build 2>&1 | tail -3
idf.py build 2>&1 | grep -E "warning:" | grep -E "components/platform|main/" || echo "no warnings"
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" reconfigure build 2>&1 | tail -1
ls -l build/cat_toy.bin
```
Expected:
- `Project build complete.` and `no warnings`;
- the soak build completes;
- `cat_toy.bin` is under 4,194,304 bytes (the factory partition). Note its size in the report: Wi-Fi adds several hundred KB.

- [ ] **Step 8: Commit**

```bash
git add components/platform main/Kconfig.projbuild main/app_main.c CMakeLists.txt sdkconfig.defaults README.md
git status --short   # sdkconfig.secrets and sdkconfig must not appear
git commit -m "feat: the clock: time zone, validity, manual set and NTP at boot"
```

---

### Task 4: Assets for the scenes, the parent area and the sunrise

**Files:**
- Modify: `tools/export_images/ui_images.mjs`
- Modify: `tools/build_sounds/synth.py`, `tools/build_sounds/sounds.txt`
- Modify: `components/assets/include/sounds.h`, `components/assets/sounds.c`
- Regenerated and committed: `components/assets/assets_manifest.c` (and `.h` if it changes)

**Interfaces:**
- Consumes: nothing.
- Produces (Tasks 5, 7 and 8):
  - the images `ui/star` (22 px), `ui/hour_star_on` and `ui/hour_star_off` (26 px), `ui/hills` (cropped; `assets_ui_pos("ui/hills", ...)` gives its top-left), `ui/p_vol` and `ui/p_moon` (36 px), `ui/p_moon_row`, `ui/p_sun_row` and `ui/p_clock_row` (24 px);
  - the sounds `SND_WHISPER` ("Pssst… spavaj") and `SND_WAKE` (chime + happy meow + "Dobro jutro!").

- [ ] **Step 1: Add the images**

In `tools/export_images/ui_images.mjs`, add above `export const UI_IMAGES`:

```js
// Sleepy clock (Screen.dc.html clock view): the night sky's stars, the hour stars and the hills.
const STAR4 = 'M10 0 Q10 10 20 10 Q10 10 10 20 Q10 10 0 10 Q10 10 10 0 Z';
const HOUR_STAR = 'M10 1 Q10 10 19 10 Q10 10 10 19 Q10 10 1 10 Q10 10 10 1 Z';
const HILLS = 'M0 404 Q116 346 233 396 Q350 346 466 404 L466 466 L0 466 Z';
// Parent-area line icons (Screen.dc.html ICON); `clock` is new, for the Now row.
const ICON = {
  vol: 'M4 9 L8 9 L13 5 L13 19 L8 15 L4 15 Z M16.5 9 Q18.5 12 16.5 15 M19 6.5 Q22.5 12 19 17.5',
  moon: 'M20 14.5 A8.5 8.5 0 1 1 9.5 4 A7 7 0 0 0 20 14.5 Z',
  sun: 'M12 8 A4 4 0 1 1 11.99 8 Z M12 2 L12 4 M12 20 L12 22 M2 12 L4 12 M20 12 L22 12 M5 5 L6.5 6.5 M17.5 17.5 L19 19 M5 19 L6.5 17.5 M17.5 6.5 L19 5',
  clock: 'M12 3 A9 9 0 1 1 11.99 3 Z M12 7 L12 12 L15.5 14',
};
const lineIcon = (name, d, size, color) => ({
  name,
  viewBox: [0, 0, 24, 24],
  size,
  crop: false,
  svg: `<path d="${d}" fill="none" stroke="${color}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"/>`,
});
```

and append to the `UI_IMAGES` array, after `ui/piano_note`:

```js
  { name: 'ui/star', viewBox: [0, 0, 20, 20], size: 22, crop: false, svg: `<path d="${STAR4}" fill="#FFE58A"/>` },
  {
    name: 'ui/hour_star_on',
    viewBox: [0, 0, 20, 20],
    size: 26,
    crop: false,
    svg: `<path d="${HOUR_STAR}" fill="#FFE58A" stroke="#FFE58A" stroke-width="2"/>`,
  },
  {
    name: 'ui/hour_star_off',
    viewBox: [0, 0, 20, 20],
    size: 26,
    crop: false,
    svg: `<path d="${HOUR_STAR}" fill="none" stroke="#6B6B8A" stroke-width="2"/>`,
  },
  {
    name: 'ui/hills',
    viewBox: [0, 0, 466, 466],
    crop: true,
    svg: `<path d="${HILLS}" fill="#3FCB6A" stroke="#1D7A3A" stroke-width="6" stroke-linejoin="round"/>`,
  },
  lineIcon('ui/p_vol', ICON.vol, 36, '#F2EFFA'),
  lineIcon('ui/p_moon', ICON.moon, 36, '#F2EFFA'),
  lineIcon('ui/p_moon_row', ICON.moon, 24, '#FFC76B'),
  lineIcon('ui/p_sun_row', ICON.sun, 24, '#FFC76B'),
  lineIcon('ui/p_clock_row', ICON.clock, 24, '#FFC76B'),
```

The moon, the sun disc and the rays need no images. They are LVGL circles and lines (Task 5).

- [ ] **Step 2: Make the wake chime the design's, and add the sunrise sound**

In `tools/build_sounds/synth.py`, add on the line after `C5, E5, G5, C6, E6, G6 = ...`:

```python
G4 = 392.0  # the design's wake chime starts on G4
```

and replace the `"chime_wake"` entry with the design's four rising triangle tones (`Main.dc.html` `setMorning`: 392, 523, 659 and 784 Hz, each gliding up 2 %):

```python
    "chime_wake": lambda: sum(
        (sweep(f, f * 1.02, 0.2, harmonics=(1, 0, 0.11, 0, 0.04), release=0.6) for f in (G4, C5, E5, G5)), []
    ),
```

In `tools/build_sounds/sounds.txt`, add at the end of the `# Derived` block:

```
wake          concat  chime_wake+meow_2+dobro_jutro
```

- [ ] **Step 3: Add the sound ids**

In `components/assets/include/sounds.h`, add before `SND_COUNT`:

```c
    SND_WHISPER, /* "Pssst… spavaj": play it with audio_play_quiet() (spec §3.12) */
    SND_WAKE,    /* the sunrise: the rising chime, a happy meow and "Dobro jutro!" */
```

In `components/assets/sounds.c`, add to `FILES`:

```c
    [SND_WHISPER] = "pssst_spavaj",
    [SND_WAKE] = "wake",
```

- [ ] **Step 4: Rebuild the assets and check they fit**

Run:
```bash
tools/build_assets.sh 2>&1 | tail -5
ls -l assets/img/ui/star.bin assets/img/ui/hills.bin assets/img/ui/p_clock_row.bin assets/snd/wake.wav
grep -n "ui/hills" components/assets/assets_manifest.c
. ~/esp/esp-idf/export.sh >/dev/null 2>&1 && idf.py build 2>&1 | tail -3
```
Expected:
- the host tests pass and the export runs without errors;
- all four files exist; `hills.bin` is under 200 KB;
- the manifest has a `ui/hills` position row;
- `Project build complete.` (so the partition still fits). Note the printed size of `assets/` in the commit message body.

Look at the contact sheet the exporter writes, next to the other exported previews. Check the stars, the hills and the icons against `Screen.dc.html`.

- [ ] **Step 5: Commit**

```bash
git add tools/export_images/ui_images.mjs tools/build_sounds components/assets
git commit -m "feat: images and sounds for the night and day scenes and the parent area"
```

---

### Task 5: The night and day scenes, and the Sleepy clock app

**Files:**
- Create: `components/ui/include/scenes.h`, `components/ui/scenes.c`
- Create: `components/ui/apps/clock.c`
- Delete: `components/ui/apps/placeholder.c` (it only holds `app_clock`)
- Modify: `components/ui/include/tokens.h`
- Modify: `README.md` ("Adding an app", step 3)

**Interfaces:**
- Consumes: the Task 4 images, `cat_create`/`cat_set_origin`/`cat_set_state`, `ui_plain`/`ui_circle`/`ui_image`, `assets_image`/`assets_ui_pos`.
- Produces (Task 7):
  - `night_scene_t` with `root`, `cat`, `hour[HOUR_STARS]` and `lit`;
  - `void scene_night_create(night_scene_t *s, lv_obj_t *parent)`;
  - `void scene_night_set_stars(night_scene_t *s, uint8_t lit)`;
  - `day_scene_t` with `root`, `cat` and `rays[DAY_RAYS][2]`;
  - `void scene_day_create(day_scene_t *s, lv_obj_t *parent)`;
  - `const app_t app_clock` (already referenced by `registry.c`).

- [ ] **Step 1: Add the tokens**

In `components/ui/include/tokens.h`, change the comment `/* Placeholder apps: an L-scale cat centered on the screen */` to `/* An L-scale cat centered on the screen (the boot animation) */`. Then append:

```c
/* Sleepy clock scenes (Screen.dc.html clock view; spec §3.12) */
#define COLOR_MOON            0xFFE58A
#define NIGHT_MOON_CX         118  /* an r 40 disc ... */
#define NIGHT_MOON_CY         114
#define NIGHT_MOON_D          80
#define NIGHT_MOON_CUT_CX     138  /* ... with an r 34 black disc cut out of it */
#define NIGHT_MOON_CUT_CY     100
#define NIGHT_MOON_CUT_D      68
#define NIGHT_STARS           9
#define NIGHT_TWINKLE_MS      3000 /* 30 % -> 100 % -> 30 % */
#define NIGHT_TWINKLE_LOW_OPA 77   /* 30 % */
#define NIGHT_CAT_X           133  /* scale M origin: the design's 210 px sleeping cat, centered */
#define NIGHT_CAT_Y           176
#define HOUR_STARS            5
#define HOUR_STAR_SIZE        26
#define HOUR_STAR_R           200  /* on r 200, 12° apart around 6 o'clock */
#define HOUR_STAR_STEP_DEG    12
#define COLOR_SUN             0xFFC83A
#define COLOR_SUN_EDGE        0xD98A00
#define DAY_SUN_X             123  /* the 220 px sun box */
#define DAY_SUN_Y             2
#define DAY_SUN_BOX           220
#define DAY_SUN_D             126  /* r 60 with its 6 px stroke centered on it */
#define DAY_SUN_BORDER        6
#define DAY_RAYS              12
#define DAY_RAY_R0            74
#define DAY_RAY_R1            98
#define DAY_RAY_W             10
#define DAY_RAY_TURN_MS       2000 /* one ray's 30° per 2 s: the design's 24 s turn */
#define DAY_RISE_PX           110
#define DAY_RISE_MS           4000
#define DAY_CAT_X             133  /* scale M origin (the design's 200 px proud cat) */
#define DAY_CAT_Y             196
#define DAY_CAT_W             200  /* the scale M cat's box: a tap inside it purrs */
#define DAY_CAT_H             210
```

- [ ] **Step 2: Write the scenes header**

Create `components/ui/include/scenes.h`:

```c
#pragma once
#include <stdint.h>
#include "lvgl.h"
#include "tokens.h"

/* The Sleepy clock's scenes (Screen.dc.html clock view; spec §3.12). Each builds into `parent` and runs
 * its own animations. Every animation targets one of the scene's objects, so deleting the parent (or
 * cleaning it) stops them all. */

typedef struct {
    lv_obj_t *root; /* the whole scene: hide it to stop drawing, move it for the burn-in shift */
    lv_obj_t *cat;  /* the sleeping cat, scale M */
    lv_obj_t *hour[HOUR_STARS];
    uint8_t lit;    /* hour stars lit now */
} night_scene_t;

/* The moon, nine twinkling stars, the sleeping cat and five unlit hour stars. */
void scene_night_create(night_scene_t *s, lv_obj_t *parent);
/* Lights the first `lit` hour stars from the left (at most HOUR_STARS). */
void scene_night_set_stars(night_scene_t *s, uint8_t lit);

typedef struct {
    lv_obj_t *root;
    lv_obj_t *cat; /* the proud cat, scale M */
    lv_point_precise_t rays[DAY_RAYS][2]; /* the turning rays' end points: lv_line keeps a pointer */
} day_scene_t;

/* The sun with its turning rays, rising DAY_RISE_PX over DAY_RISE_MS; the hills; the proud cat. `s`
 * must outlive the scene's objects, because the rays read their points from it. */
void scene_day_create(day_scene_t *s, lv_obj_t *parent);
```

- [ ] **Step 3: Write the scenes**

Create `components/ui/scenes.c`:

```c
#include "scenes.h"
#include <math.h>
#include "assets.h"
#include "cat.h"
#include "ui_util.h"

#define DEG_TO_RAD (3.14159265f / 180.0f)

/* Design nightStars: x, y, and the twinkle's delay in ms. */
static const int16_t NIGHT_STAR_POS[NIGHT_STARS][3] = {
    {300, 70, 0},     {360, 120, 800}, {390, 196, 1600}, {252, 44, 400},  {196, 96, 1200},
    {96, 200, 2000},  {70, 268, 600},  {380, 276, 1400}, {170, 150, 2400},
};

static lv_obj_t *layer(lv_obj_t *parent)
{
    lv_obj_t *o = ui_plain(parent);
    lv_obj_set_size(o, SCREEN_SIZE, SCREEN_SIZE);
    return o;
}

static void image_opa(void *obj, int32_t v) { lv_obj_set_style_image_opa(obj, (lv_opa_t)v, 0); }

/* The design's .twinkle: 30 % -> 100 % -> 30 % over 3 s, forever, starting after delay_ms. */
static void twinkle(lv_obj_t *img, uint32_t delay_ms)
{
    lv_obj_set_style_image_opa(img, NIGHT_TWINKLE_LOW_OPA, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, image_opa);
    lv_anim_set_values(&a, NIGHT_TWINKLE_LOW_OPA, LV_OPA_COVER);
    lv_anim_set_duration(&a, NIGHT_TWINKLE_MS / 2);
    lv_anim_set_reverse_duration(&a, NIGHT_TWINKLE_MS / 2);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_start(&a);
}

void scene_night_create(night_scene_t *s, lv_obj_t *parent)
{
    s->root = layer(parent);
    ui_circle(s->root, NIGHT_MOON_CX, NIGHT_MOON_CY, NIGHT_MOON_D, COLOR_MOON);
    ui_circle(s->root, NIGHT_MOON_CUT_CX, NIGHT_MOON_CUT_CY, NIGHT_MOON_CUT_D, 0x000000);
    for (size_t i = 0; i < NIGHT_STARS; i++) {
        lv_obj_t *star = ui_image(s->root, "ui/star", NIGHT_STAR_POS[i][0], NIGHT_STAR_POS[i][1]);
        twinkle(star, (uint32_t)NIGHT_STAR_POS[i][2]);
    }
    s->cat = cat_create(s->root, CAT_SCALE_M);
    cat_set_origin(s->cat, NIGHT_CAT_X, NIGHT_CAT_Y);
    cat_set_state(s->cat, CAT_SLEEPING);
    for (int i = 0; i < HOUR_STARS; i++) {
        /* Design: star i at 90° + (i - 2) * 12° on r 200, the first one on the left. */
        const float a = (90.0f + (float)((i - HOUR_STARS / 2) * HOUR_STAR_STEP_DEG)) * DEG_TO_RAD;
        const int32_t x = SCREEN_CENTER - (int32_t)lroundf(HOUR_STAR_R * cosf(a)) - HOUR_STAR_SIZE / 2;
        const int32_t y = SCREEN_CENTER + (int32_t)lroundf(HOUR_STAR_R * sinf(a)) - HOUR_STAR_SIZE / 2;
        s->hour[i] = ui_image(s->root, "ui/hour_star_off", x, y);
    }
    s->lit = 0;
}

void scene_night_set_stars(night_scene_t *s, uint8_t lit)
{
    if (lit > HOUR_STARS) {
        lit = HOUR_STARS;
    }
    if (lit == s->lit) {
        return;
    }
    s->lit = lit;
    for (int i = 0; i < HOUR_STARS; i++) {
        lv_image_set_src(s->hour[i], assets_image(i < lit ? "ui/hour_star_on" : "ui/hour_star_off"));
    }
}

/* Turns the rays: v is the angle in 0.1°, over one ray's 30° (the pattern repeats every 30°). */
static void rays_step(lv_anim_t *a, int32_t v)
{
    day_scene_t *s = lv_anim_get_user_data(a);
    lv_obj_t *rays = a->var;
    const float c = DAY_SUN_BOX / 2.0f;
    for (uint32_t i = 0; i < DAY_RAYS; i++) {
        const float ang = ((float)i * (360.0f / DAY_RAYS) + (float)v / 10.0f) * DEG_TO_RAD;
        const float co = cosf(ang), si = sinf(ang);
        s->rays[i][0].x = (lv_value_precise_t)lroundf(c + DAY_RAY_R0 * co);
        s->rays[i][0].y = (lv_value_precise_t)lroundf(c + DAY_RAY_R0 * si);
        s->rays[i][1].x = (lv_value_precise_t)lroundf(c + DAY_RAY_R1 * co);
        s->rays[i][1].y = (lv_value_precise_t)lroundf(c + DAY_RAY_R1 * si);
        lv_line_set_points(lv_obj_get_child(rays, (int32_t)i), s->rays[i], 2);
    }
}

static void set_y(void *obj, int32_t v) { lv_obj_set_y(obj, v); }

void scene_day_create(day_scene_t *s, lv_obj_t *parent)
{
    s->root = layer(parent);
    lv_obj_t *sun = ui_plain(s->root);
    lv_obj_set_size(sun, DAY_SUN_BOX, DAY_SUN_BOX);
    lv_obj_set_pos(sun, DAY_SUN_X, DAY_SUN_Y + DAY_RISE_PX);
    lv_obj_t *rays = ui_plain(sun);
    lv_obj_set_size(rays, DAY_SUN_BOX, DAY_SUN_BOX);
    for (int i = 0; i < DAY_RAYS; i++) {
        lv_obj_t *l = lv_line_create(rays);
        lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(l, DAY_SUN_BOX, DAY_SUN_BOX); /* fixed: the points move inside it */
        lv_obj_set_style_line_width(l, DAY_RAY_W, 0);
        lv_obj_set_style_line_color(l, lv_color_hex(COLOR_SUN), 0);
        lv_obj_set_style_line_rounded(l, true, 0);
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, rays);
    lv_anim_set_user_data(&a, s);
    lv_anim_set_custom_exec_cb(&a, rays_step); /* applied at once, so the rays have points before drawing */
    lv_anim_set_values(&a, 0, 3600 / DAY_RAYS);
    lv_anim_set_duration(&a, DAY_RAY_TURN_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    lv_obj_t *disc = ui_circle(sun, DAY_SUN_BOX / 2, DAY_SUN_BOX / 2, DAY_SUN_D, COLOR_SUN);
    lv_obj_set_style_border_width(disc, DAY_SUN_BORDER, 0);
    lv_obj_set_style_border_color(disc, lv_color_hex(COLOR_SUN_EDGE), 0);

    lv_anim_t r; /* the design's .sun-rise: up 110 px over 4 s, ease-in-out */
    lv_anim_init(&r);
    lv_anim_set_var(&r, sun);
    lv_anim_set_exec_cb(&r, set_y);
    lv_anim_set_values(&r, DAY_SUN_Y + DAY_RISE_PX, DAY_SUN_Y);
    lv_anim_set_duration(&r, DAY_RISE_MS);
    lv_anim_set_path_cb(&r, lv_anim_path_ease_in_out);
    lv_anim_start(&r);

    int16_t hx, hy;
    assets_ui_pos("ui/hills", &hx, &hy);
    ui_image(s->root, "ui/hills", hx, hy);
    s->cat = cat_create(s->root, CAT_SCALE_M);
    cat_set_origin(s->cat, DAY_CAT_X, DAY_CAT_Y);
    cat_set_state(s->cat, CAT_PROUD);
}
```

- [ ] **Step 4: Write the Sleepy clock app**

Create `components/ui/apps/clock.c`:

```c
#include "app.h"
#include "audio.h"
#include "scenes.h"
#include "tokens.h"

/* Sleepy clock (spec §3.12): the day scene, and a tap on the cat purrs as in Pet the cat. The app is
 * never open at night: the bedtime lock closes it. */

static day_scene_t s_day; /* the rays read their points from here, so it outlives the screen */
static lv_obj_t *s_cat;   /* NULL once destroyed */
static lv_timer_t *s_react_timer, *s_purr_timer;

static lv_timer_t *one_shot(lv_timer_cb_t cb, uint32_t ms)
{
    lv_timer_t *t = lv_timer_create(cb, ms, NULL);
    lv_timer_set_repeat_count(t, 1);
    return t;
}

static void react_done(lv_timer_t *t)
{
    (void)t;
    s_react_timer = NULL; /* one-shot timers delete themselves */
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    cat_set_state(s_cat, CAT_PROUD);
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

static void tapped(lv_event_t *e)
{
    (void)e;
    if (!s_cat) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    lv_indev_t *indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    if (p.x < DAY_CAT_X || p.x >= DAY_CAT_X + DAY_CAT_W || p.y < DAY_CAT_Y || p.y >= DAY_CAT_Y + DAY_CAT_H) {
        return; /* only the cat purrs */
    }
    cancel_timers();
    cat_set_state(s_cat, CAT_PURRING);
    audio_loop_start(SND_PURR);
    s_purr_timer = one_shot(purr_done, PET_PURR_SOUND_MS);
    s_react_timer = one_shot(react_done, PET_PURR_MS);
}

static void clock_create(lv_obj_t *screen)
{
    scene_day_create(&s_day, screen);
    s_cat = s_day.cat;
    lv_obj_add_event_cb(screen, tapped, LV_EVENT_CLICKED, NULL);
}

static void clock_destroy(void)
{
    cancel_timers();
    s_cat = NULL;
    /* the purr loop: app_manager calls audio_stop_all() after this */
}

const app_t app_clock = {
    .id = "clock",
    .icon = "apps/clock",
    .icon_small = "apps/clock_96",
    .color = COLOR_CLOCK,
    .name_sound = SND_NAME_CLOCK,
    .swipe_sound = SND_SWIPE_CLOCK,
    .react = CAT_SLEEPY,
    .create = clock_create,
    .destroy = clock_destroy,
    .on_shake = NULL,
    .on_tilt = NULL,
};
```

- [ ] **Step 5: Remove the placeholder**

Run: `git rm components/ui/apps/placeholder.c`

In `README.md`, "Adding an app", delete step 3 ("If it replaces a placeholder, delete that line from `apps/placeholder.c`."). Every app is real now.

Check that nothing else uses the placeholder: `grep -rn "placeholder_\|PLACEHOLDER(" components main` must print nothing. `PLACEHOLDER_CAT_X/Y` stay, because `boot.c` uses them.

- [ ] **Step 6: Build the normal and soak variants**

Run:
```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1 && idf.py reconfigure >/dev/null && idf.py build 2>&1 | tail -3
idf.py build 2>&1 | grep -E "warning:" | grep -E "components/ui" || echo "no warnings"
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" reconfigure build 2>&1 | tail -1
```
Expected: both builds complete, and `no warnings`. The soak now opens the real Sleepy clock with the other apps. It runs on the device in Task 9.

- [ ] **Step 7: Commit**

```bash
git add components/ui README.md
git commit -m "feat: Sleepy clock with the day scene; the night scene for the lock"
```

---

### Task 6: The parent hold on any screen

**Files:**
- Create: `components/ui/include/hold_ring.h`, `components/ui/hold_ring.c`
- Modify: `components/ui/launcher.c`, `components/ui/include/launcher.h`

**Interfaces:**
- Consumes: `hold_update`/`hold_reset`/`hold_owns_touch` (`gestures.h`), `touch_raw_get` (`touch_raw.h`), `app_manager_current`, `ui_arc`.
- Produces (Task 7):
  - `typedef struct { void (*holding)(void); void (*done)(void); } hold_ring_cb_t;`
  - `void hold_ring_attach(lv_obj_t *screen, int32_t cx, int32_t cy, const hold_ring_cb_t *cb)`
  - `lv_obj_t *launcher_screen(void)`

- [ ] **Step 1: Write the header**

Create `components/ui/include/hold_ring.h`:

```c
#pragma once
#include <stdint.h>
#include "lvgl.h"

/* The parent entry (spec §3.1): two fingers held still for 5 s, on the menu or on the bedtime lock. */
typedef struct {
    void (*holding)(void); /* every 50 ms once the hold owns the touch (1 s): keep the screen lit */
    void (*done)(void);    /* once, at 5 s: open the parent area */
} hold_ring_cb_t;

/* While `screen` is the active screen and no app is open, a 50 ms timer feeds two-finger touches to the
 * hold tracker (gestures.h). Once the hold owns the touch, the rest of that touch is swallowed, so letting
 * go taps nothing, and a progress ring centered on (cx, cy) shows. The ring is a child of `screen`; the
 * timer goes when the ring is deleted with it. */
void hold_ring_attach(lv_obj_t *screen, int32_t cx, int32_t cy, const hold_ring_cb_t *cb);
```

- [ ] **Step 2: Move the hold out of the launcher**

Create `components/ui/hold_ring.c`. The body of `poll()` is the launcher's `hold_tick()`, and the ring styling is the launcher's:

```c
#include "hold_ring.h"
#include "app_manager.h"
#include "gestures.h"
#include "tokens.h"
#include "touch_raw.h"
#include "ui_util.h"

typedef struct {
    lv_obj_t *screen, *ring;
    lv_timer_t *timer;
    hold_track_t track;
    hold_ring_cb_t cb;
} hold_ring_t;

/* The hold owns this touch (the ring is showing): make LVGL ignore the rest of it, so letting go is not
 * also a tap on whatever is under the fingers (the same call the app manager uses before closing an app). */
static void swallow_touch(void)
{
    for (lv_indev_t *i = lv_indev_get_next(NULL); i; i = lv_indev_get_next(i)) {
        if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(i) == LV_INDEV_STATE_PRESSED) {
            lv_indev_wait_release(i);
        }
    }
}

static void poll(lv_timer_t *t)
{
    hold_ring_t *h = lv_timer_get_user_data(t);
    if (app_manager_current() || lv_screen_active() != h->screen) {
        hold_reset(&h->track); /* only while this screen shows */
        ui_set_hidden(h->ring, true);
        return;
    }
    const touch_raw_t raw = touch_raw_get();
    uint16_t progress = 0;
    const hold_state_t st = hold_update(&h->track, raw.count, raw.x, raw.y, lv_tick_get(), &progress);
    if (hold_owns_touch(st)) {
        swallow_touch();
        h->cb.holding();
    }
    ui_set_hidden(h->ring, st != HOLD_PROGRESS);
    if (st == HOLD_PROGRESS) {
        lv_arc_set_value(h->ring, progress);
    } else if (st == HOLD_DONE) {
        h->cb.done();
    }
}

static void ring_deleted(lv_event_t *e)
{
    hold_ring_t *h = lv_event_get_user_data(e);
    lv_timer_delete(h->timer);
    lv_free(h);
}

void hold_ring_attach(lv_obj_t *screen, int32_t cx, int32_t cy, const hold_ring_cb_t *cb)
{
    hold_ring_t *h = lv_malloc_zeroed(sizeof(*h));
    LV_ASSERT_MALLOC(h);
    h->screen = screen;
    h->cb = *cb;
    hold_reset(&h->track);
    h->ring = ui_arc(screen, cx, cy, HOLD_RING_R, HOLD_RING_W);
    lv_arc_set_rotation(h->ring, 270); /* start at 12 o'clock */
    lv_arc_set_bg_angles(h->ring, 0, 360);
    lv_arc_set_range(h->ring, 0, 1000);
    lv_obj_set_style_arc_opa(h->ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(h->ring, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(h->ring, HOLD_RING_OPA, LV_PART_INDICATOR);
    ui_set_hidden(h->ring, true);
    h->timer = lv_timer_create(poll, HOLD_POLL_MS, h);
    lv_obj_add_event_cb(h->ring, ring_deleted, LV_EVENT_DELETE, h);
}
```

In `components/ui/launcher.c`:
- remove `static lv_obj_t *s_hold_ring;`, `static hold_track_t s_hold;`, `swallow_touch()` and `hold_tick()`, and remove `#include "touch_raw.h"`. Add `#include "hold_ring.h"`;
- add these two functions where `hold_tick()` was:

```c
static void hold_holding(void)
{
    idle_wake_if_asleep(); /* show the ring even if the hold started on a dimmed or dark menu */
}

static void hold_done(void)
{
    idle_wake(); /* the hold may have started on a dimmed or dark menu */
    s_busy = true; /* launcher_show() clears it when the menu comes back */
    app_manager_open(&PARENT_APP);
}
```

- in `launcher_create()`, replace everything from `s_hold_ring = ui_arc(...)` to `lv_timer_create(hold_tick, HOLD_POLL_MS, NULL);` with:

```c
    static const hold_ring_cb_t HOLD = {.holding = hold_holding, .done = hold_done};
    hold_ring_attach(s_scr, HOLD_RING_CX, HOLD_RING_CY, &HOLD);
```

- add at the end of the file:

```c
lv_obj_t *launcher_screen(void) { return s_scr; }
```

In `components/ui/include/launcher.h`, add `#include "lvgl.h"`, and:

```c
lv_obj_t *launcher_screen(void); /* the menu's screen, which lives for the whole run */
```

- [ ] **Step 3: Build**

Run:
```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1 && idf.py reconfigure >/dev/null && idf.py build 2>&1 | tail -3
idf.py build 2>&1 | grep -E "warning:" | grep -E "components/ui" || echo "no warnings"
grep -n "hold_update\|touch_raw" components/ui/launcher.c || echo "moved"
```
Expected: `Project build complete.`, `no warnings`, `moved`. The behavior on the menu is unchanged. Task 9 checks it on the device (A).

- [ ] **Step 4: Commit**

```bash
git add components/ui
git commit -m "chore: the parent two-finger hold works on any screen"
```

---

### Task 7: The bedtime lock and the sunrise

**Files:**
- Create: `components/ui/include/bedtime.h`, `components/ui/bedtime.c`
- Modify: `components/ui/app_manager.c`, `components/ui/include/app_manager.h`
- Modify: `components/ui/idle.c`, `components/ui/include/idle.h`
- Modify: `components/ui/include/tokens.h`

**Interfaces:**
- Consumes:
  - `schedule_is_night`, `schedule_stars_left`, `SCHEDULE_STARS` (Task 1);
  - `power_on_usb`, `settings_bedtime`, `settings_wake`, `audio_play_quiet` (Task 2);
  - `clock_valid`, `clock_now_min`, `clock_sync_start` (Task 3);
  - `SND_WHISPER`, `SND_WAKE` (Task 4);
  - `night_scene_t`, `day_scene_t`, `scene_*` (Task 5);
  - `hold_ring_attach`, `launcher_screen` (Task 6);
  - `PARENT_APP` (`parent.h`).
- Produces:
  - `void bedtime_init(void)`, `bool bedtime_take_over(void)`, `bool bedtime_showing(void)`, `void bedtime_power(void)`
  - `bool app_manager_started(void)`
  - `void idle_hand_over(void)`

How the pieces meet:
- **Where the menu would show next,** `app_manager` asks `bedtime_take_over()` first: when the boot animation ends, and when any app closes. The parent area is an app too. So an app closed at bedtime and a parent leaving at night both land on the lock, without the mew.
- **The 1 s tick** in `bedtime.c`:
  - starts the night: it closes an open app (which then lands on the lock), or shows the lock over the menu;
  - ends the night with the sunrise;
  - does nothing while the parent area is open.
- **While the lock or the sunrise shows,** `idle.c` stands down, and `handle_button()` sends PWR to `bedtime_power()` and ignores Home.
- **Screen ownership:** the lock screen is transient. The menu screen is permanent and must never be auto-deleted:
  - from an app or the boot screen, the lock loads with `auto_del = true`, deleting that app or boot screen, as `launcher_show()` does;
  - over the menu it loads with `auto_del = false`, and only when the menu is the active screen with no load pending (`lv_display_get_screen_loading(NULL) == NULL`). Otherwise the pending load would make the menu the screen that gets deleted. See `lv_screen_load_anim()` in `managed_components/lvgl__lvgl/src/display/lv_display.c`, lines 724-770;
  - the sunrise reuses the lock screen, and `launcher_show()` deletes it after the fade.

- [ ] **Step 1: Add the tokens**

Append to `components/ui/include/tokens.h`:

```c
/* Bedtime lock and sunrise (spec §3.12) */
#define BEDTIME_TICK_MS        1000
#define BEDTIME_BRIGHTNESS     35
#define BEDTIME_ON_MS          30000 /* battery: the panel turns off 30 s after the lock shows */
#define BEDTIME_TOUCH_ON_MS    5000  /* a touch shows the scene for 5 s */
#define BEDTIME_WHISPER_GAP_MS 5000
#define BEDTIME_WHISPER_GAIN   30    /* % of the playing volume */
#define BEDTIME_SHH_MS         2200  /* design: the shh face after a night tap */
#define BEDTIME_RING_CX        233   /* the hold ring around the sleeping cat's head */
#define BEDTIME_RING_CY        279
#define SUNRISE_FROM_PCT       10
#define SUNRISE_MENU_MS        60000
```

- [ ] **Step 2: Write the header**

Create `components/ui/include/bedtime.h`:

```c
#pragma once
#include <stdbool.h>

/* The bedtime lock and the sunrise (spec §3.8, §3.12). LVGL task only. */
void bedtime_init(void); /* starts the 1 s schedule check; app_manager_init() calls it */
/* Called where the menu would show next (the boot animation ended, an app closed). While it is night it
 * shows the lock instead, deleting the active screen, and returns true. Otherwise it forgets a lock the
 * parent area moved out of the night, and returns false. */
bool bedtime_take_over(void);
/* True while the lock or the sunrise is on screen (not while the parent area is open over it). */
bool bedtime_showing(void);
/* PWR while bedtime_showing(): the lock's panel off or back on. During the sunrise: the menu, dark. */
void bedtime_power(void);
```

- [ ] **Step 3: Write the lock and the sunrise**

Create `components/ui/bedtime.c`:

```c
#include "bedtime.h"
#include <stdint.h>
#include "app_manager.h"
#include "audio.h"
#include "cat.h"
#include "clock.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_random.h"
#include "hold_ring.h"
#include "idle.h"
#include "launcher.h"
#include "parent.h"
#include "power.h"
#include "scenes.h"
#include "schedule.h"
#include "sdkconfig.h"
#include "settings.h"
#include "tokens.h"
#include "ui_util.h"

_Static_assert(HOUR_STARS == SCHEDULE_STARS, "one hour star for each star the schedule counts");

typedef enum { BT_DAY, BT_LOCK, BT_SUNRISE } bt_state_t;

static const char *TAG = "bedtime";
static bt_state_t s_state;
static lv_obj_t *s_scr;      /* the lock's screen, reused by the sunrise; NULL in the day */
static night_scene_t s_night;
static day_scene_t s_day;    /* the rays read their points from here */
static uint16_t s_prev_min;  /* the minute at the last tick, for the daily NTP sync */
/* The lock's panel (spec §3.12): lit all night on USB unless PWR turned it off; otherwise lit until
 * s_on_until (30 s after the lock shows, 5 s after a touch). */
static bool s_lit, s_user_off, s_usb;
static uint32_t s_on_until;
static bool s_whispered; /* s_last_whisper is set */
static uint32_t s_last_whisper, s_last_shift;
static lv_timer_t *s_shh_timer, *s_menu_timer;
static bool s_sun_up;   /* the sun has risen: a tap goes to the menu */
static int s_fade_var;  /* identifies the sunrise's brightness anim (lv_anim needs a non-NULL var) */

static lv_timer_t *one_shot(lv_timer_cb_t cb, uint32_t ms)
{
    lv_timer_t *t = lv_timer_create(cb, ms, NULL);
    lv_timer_set_repeat_count(t, 1);
    return t;
}

static void delete_timer(lv_timer_t **t)
{
    if (*t) {
        lv_timer_delete(*t);
        *t = NULL;
    }
}

static bool night_now(void)
{
#if CONFIG_CAT_DEV_SOAK
    return false; /* the soak opens and closes apps around the clock; the lock would take them over */
#else
    return clock_valid() && schedule_is_night(clock_now_min(), settings_bedtime(), settings_wake());
#endif
}

static void log_heap(const char *what)
{
    ESP_LOGI(TAG, "%s: free internal %u, psram %u", what, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

/* ---- the lock's panel ---- */

static void set_lit(bool lit)
{
    if (lit == s_lit) {
        return;
    }
    s_lit = lit;
    ui_set_hidden(s_night.root, !lit); /* hidden, the twinkles stop drawing */
    display_set_brightness(lit ? BEDTIME_BRIGHTNESS : 0);
}

static void update_panel(void)
{
    set_lit((s_usb && !s_user_off) || (int32_t)(s_on_until - lv_tick_get()) > 0);
}

/* Keeps the scene lit for at least ms from now. */
static void keep_lit(uint32_t ms)
{
    const uint32_t until = lv_tick_get() + ms;
    if ((int32_t)(until - s_on_until) > 0) {
        s_on_until = until;
    }
}

/* ---- the lock ---- */

static void shh_done(lv_timer_t *t)
{
    (void)t;
    s_shh_timer = NULL; /* one-shot timers delete themselves */
    if (s_state == BT_LOCK) {
        cat_set_state(s_night.cat, CAT_SLEEPING);
    }
}

/* A touch shows the scene for 5 s; the cat whispers "Pssst… spavaj", at most once every 5 s. */
static void lock_pressed(lv_event_t *e)
{
    (void)e;
    if (s_state != BT_LOCK) {
        return;
    }
    keep_lit(BEDTIME_TOUCH_ON_MS);
    update_panel();
    if (s_whispered && lv_tick_elaps(s_last_whisper) < BEDTIME_WHISPER_GAP_MS) {
        return;
    }
    s_whispered = true;
    s_last_whisper = lv_tick_get();
    audio_play_quiet(SND_WHISPER, BEDTIME_WHISPER_GAIN);
    cat_set_state(s_night.cat, CAT_SHH);
    delete_timer(&s_shh_timer);
    s_shh_timer = one_shot(shh_done, BEDTIME_SHH_MS);
}

static void hold_holding(void)
{
    keep_lit(BEDTIME_TOUCH_ON_MS); /* the ring stays visible while the parent holds */
    update_panel();
}

static void hold_done(void)
{
    display_set_brightness(BRIGHTNESS_FULL); /* the parent area is never dimmed */
    lv_display_trigger_activity(NULL);
    app_manager_open(&PARENT_APP); /* leaving it comes back through bedtime_take_over() */
}

static void build_lock(void)
{
    s_scr = ui_screen();
    scene_night_create(&s_night, s_scr);
    lv_obj_add_event_cb(s_scr, lock_pressed, LV_EVENT_PRESSED, NULL);
    static const hold_ring_cb_t HOLD = {.holding = hold_holding, .done = hold_done};
    hold_ring_attach(s_scr, BEDTIME_RING_CX, BEDTIME_RING_CY, &HOLD);
}

/* Shows the lock (built once per night) and lights it: 30 s on battery, all night on USB. */
static void show_lock(bool delete_previous)
{
    if (!s_scr) {
        build_lock();
    }
    s_state = BT_LOCK;
    scene_night_set_stars(&s_night, schedule_stars_left(clock_now_min(), settings_wake()));
    idle_hand_over(); /* the lock runs its own panel rules */
    s_user_off = false;
    s_usb = power_on_usb();
    s_on_until = lv_tick_get() + BEDTIME_ON_MS;
    s_lit = false;
    set_lit(true);
    s_last_shift = lv_tick_get();
    lv_screen_load_anim(s_scr, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_HOME_MS, 0, delete_previous);
    ESP_LOGI(TAG, "lock (USB power: %s)", s_usb ? "yes" : "no");
    log_heap("lock shown");
}

/* The night is over without a sunrise (the parent moved the times). The caller shows the menu, and that
 * load deletes the screen that is active then. */
static void drop_lock(void)
{
    delete_timer(&s_shh_timer);
    if (s_scr && s_scr != lv_screen_active()) {
        lv_obj_delete(s_scr);
    }
    s_scr = NULL;
    s_state = BT_DAY;
    log_heap("lock dropped");
}

static void lock_tick(uint16_t now)
{
    scene_night_set_stars(&s_night, schedule_stars_left(now, settings_wake()));
    const bool usb = power_on_usb();
    if (usb != s_usb) {
        ESP_LOGI(TAG, "USB power: %s", usb ? "yes" : "no");
        if (!usb) {
            keep_lit(BEDTIME_ON_MS); /* unplugged: 30 s, as on battery */
        }
        s_usb = usb;
    }
    update_panel();
    if (lv_tick_elaps(s_last_shift) >= BURN_SHIFT_MS) {
        s_last_shift = lv_tick_get();
        const int32_t span = 2 * BURN_SHIFT_PX + 1;
        lv_obj_set_pos(s_night.root, (int32_t)(esp_random() % span) - BURN_SHIFT_PX,
                       (int32_t)(esp_random() % span) - BURN_SHIFT_PX);
    }
}

/* ---- the sunrise ---- */

static void set_brightness(void *var, int32_t v)
{
    (void)var;
    display_set_brightness((uint8_t)v);
}

static void finish_sunrise(void)
{
    delete_timer(&s_menu_timer);
    lv_anim_delete(&s_fade_var, set_brightness);
    display_set_brightness(BRIGHTNESS_FULL);
    s_state = BT_DAY;
    s_scr = NULL; /* launcher_show() deletes the screen once the menu has faded in */
    lv_display_trigger_activity(NULL); /* the quiet night must not dim the menu at once */
    launcher_show();
    log_heap("sunrise over");
}

static void menu_later(lv_timer_t *t)
{
    (void)t;
    s_menu_timer = NULL; /* one-shot timers delete themselves */
    if (s_state == BT_SUNRISE) {
        finish_sunrise();
    }
}

static void sun_up(lv_anim_t *a)
{
    (void)a;
    if (s_state != BT_SUNRISE) {
        return;
    }
    s_sun_up = true;
    audio_play(SND_WAKE); /* the rising chime, a happy meow and "Dobro jutro!" */
    s_menu_timer = one_shot(menu_later, SUNRISE_MENU_MS);
}

static void sunrise_clicked(lv_event_t *e)
{
    (void)e;
    if (s_state == BT_SUNRISE && s_sun_up) {
        finish_sunrise();
    }
}

static void start_sunrise(void)
{
    delete_timer(&s_shh_timer);
    s_state = BT_SUNRISE;
    s_sun_up = false;
    lv_obj_clean(s_scr); /* the night scene, and the hold ring with its timer */
    s_night = (night_scene_t){0};
    scene_day_create(&s_day, s_scr);
    lv_obj_add_event_cb(s_scr, sunrise_clicked, LV_EVENT_CLICKED, NULL);
    display_set_brightness(SUNRISE_FROM_PCT);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &s_fade_var);
    lv_anim_set_exec_cb(&a, set_brightness);
    lv_anim_set_values(&a, SUNRISE_FROM_PCT, BRIGHTNESS_FULL);
    lv_anim_set_duration(&a, DAY_RISE_MS);
    lv_anim_set_completed_cb(&a, sun_up);
    lv_anim_start(&a);
    log_heap("sunrise");
}

/* ---- the schedule ---- */

static void start_night(void)
{
    if (app_manager_current()) {
        app_manager_home(); /* its close lands on the lock through bedtime_take_over(), without the mew */
        return;
    }
    if (lv_screen_active() != launcher_screen() || lv_display_get_screen_loading(NULL)) {
        return; /* the menu is still fading in: try again next second */
    }
    show_lock(false); /* the menu screen stays for the morning */
}

static void tick(lv_timer_t *t)
{
    (void)t;
    const uint16_t now = clock_now_min();
    if (clock_valid() && now != s_prev_min && now == settings_wake()) {
        clock_sync_start(); /* NTP once a day, at the wake-up minute (spec §3.2) */
    }
    s_prev_min = now;
    if (!app_manager_started() || app_manager_current() == &PARENT_APP) {
        return; /* the boot animation; parent changes take effect when the parent leaves (spec §3.12) */
    }
    const bool night = night_now();
    if (s_state == BT_DAY && night) {
        start_night();
    } else if (s_state == BT_LOCK && !night) {
        start_sunrise();
    } else if (s_state == BT_LOCK) {
        lock_tick(now);
    }
}

void bedtime_init(void)
{
    s_prev_min = clock_now_min();
    lv_timer_create(tick, BEDTIME_TICK_MS, NULL);
}

bool bedtime_take_over(void)
{
    if (s_state == BT_SUNRISE) {
        return false; /* not reached: nothing can open during the sunrise */
    }
    if (night_now()) {
        show_lock(true); /* the closing app's screen, or the boot screen, goes */
        return true;
    }
    if (s_state == BT_LOCK) {
        drop_lock(); /* the parent moved the times out of the night: the menu, without the sunrise */
    }
    return false;
}

bool bedtime_showing(void) { return s_state != BT_DAY && !app_manager_current(); }

void bedtime_power(void)
{
    if (s_state == BT_SUNRISE) {
        finish_sunrise();
        idle_power_toggle(); /* the menu, then dark */
        return;
    }
    if (s_state != BT_LOCK) {
        return;
    }
    if (s_lit) {
        s_user_off = true;
        s_on_until = lv_tick_get();
        set_lit(false);
    } else {
        s_user_off = false;
        keep_lit(BEDTIME_ON_MS);
        update_panel();
    }
}
```

- [ ] **Step 4: Hand the panel over in idle.c**

In `components/ui/include/idle.h`, add:

```c
/* The bedtime lock takes the panel: back to awake with no dimming and no touch catcher. The brightness
 * is left to the caller. */
void idle_hand_over(void);
```

In `components/ui/idle.c`, add `#include "bedtime.h"`. Add this function after `idle_power_toggle()`:

```c
void idle_hand_over(void)
{
    lv_anim_delete(&s_anim_var, set_brightness);
    s_forced_off = false;
    s_level = 0;
    show_catcher(false);
    lv_display_trigger_activity(NULL);
    launcher_set_idle_level(0); /* the menu's cat is awake when the menu comes back */
}
```

In `tick()`, add as the first statement after `(void)t;`:

```c
    if (bedtime_showing()) {
        return; /* the lock and the sunrise run their own panel rules (spec §3.12) */
    }
```

- [ ] **Step 5: Route the menu, Home and PWR through bedtime in app_manager.c**

In `components/ui/include/app_manager.h`, add:

```c
bool app_manager_started(void); /* true once the boot animation has ended (app_manager_start) */
```

In `components/ui/app_manager.c`:
- add `#include "bedtime.h"`;
- in `app_manager_init()`, call `bedtime_init();` after `idle_init();`;
- replace `app_manager_start()` and add `app_manager_started()`:

```c
void app_manager_start(void)
{
    if (!bedtime_take_over()) { /* a start inside the night shows the lock after the boot animation */
        launcher_show();
    }
    s_started = true; /* buttons are ignored until this point -- see handle_button() */
}

bool app_manager_started(void) { return s_started; }
```

- at the end of `close_current_app()`, replace the mew and `launcher_show()` with:

```c
    app->destroy();
    audio_stop_all();
    if (bedtime_take_over()) {
        return; /* night: the lock instead of the menu, without the Home mew (spec §3.12) */
    }
    if (idle_level() != 3) {
        /* Screen is dark (e.g. Power turned it off, then this app's own idle timeout closed it):
         * don't mew in the dark. */
        audio_play(SND_HOME);
    }
    launcher_show(); /* fades the menu in and deletes the app screen */
```

- in `do_home()`, add after the `cancel_pending_home()` block:

```c
    if (bedtime_showing()) {
        return; /* Home does nothing on the lock (spec §3.12); also a PWR-Home queued before it showed */
    }
```

- in `handle_button()`, add right after `lv_display_trigger_activity(NULL);`, before the `#if CONFIG_CAT_HOME_ON_POWER` block:

```c
    if (bedtime_showing()) {
        if (id == BUTTON_POWER) {
            bedtime_power(); /* PWR still turns the lock's panel off and on; Home does nothing */
        }
        return;
    }
```

- [ ] **Step 6: Build**

Run:
```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1 && idf.py reconfigure >/dev/null && idf.py build 2>&1 | tail -3
idf.py build 2>&1 | grep -E "warning:" | grep -E "components/ui" || echo "no warnings"
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" reconfigure build 2>&1 | tail -1
sh test/run.sh | tail -2
```
Expected: both builds complete, `no warnings`, and all host tests pass.

Review Focus 1 and 2 are device behaviors. Task 9 checks C and D pin them. Before committing, walk through these paths in the code and write the answers in your report:
- **An app closing at bedtime:** `start_night` calls `app_manager_home`, then `close_current_app`, then `bedtime_take_over`, then `show_lock(true)`. The lock deletes the app screen, and no mew plays.
- **The lock over the menu:** `start_night` calls `show_lock(false)` only when `lv_screen_active() == launcher_screen()` and no load is pending, so the menu is kept.
- **The parent leaving at night:** `close_current_app` calls `bedtime_take_over`, then `show_lock(true)`. The existing lock screen comes back, and the parent screen is deleted.
- **The parent leaving after moving the times out of the night:** `drop_lock` deletes the inactive lock, then `launcher_show`, which deletes the parent screen, with the mew.
- **The sunrise ending:** `finish_sunrise` calls `launcher_show`, which deletes the lock screen, and the brightness anim is deleted.

- [ ] **Step 7: Commit**

```bash
git add components/ui
git commit -m "feat: the bedtime lock, the night light and the sunrise"
```

---

### Task 8: The parent area: home tiles and Sleep times

**Files:**
- Modify: `components/ui/parent.c` (rewritten), `components/ui/include/parent.h` (comment)
- Modify: `components/ui/include/tokens.h`

**Interfaces:**
- Consumes:
  - `schedule_step`, `schedule_format`, `schedule_now_step`, `SCHEDULE_SLEEP_STEP`, `SCHEDULE_NOW_START` (Task 1);
  - `settings_bedtime`/`settings_set_bedtime`/`settings_wake`/`settings_set_wake` (Task 2);
  - `clock_valid`, `clock_now_min`, `clock_set_now_min` (Task 3);
  - the `ui/p_*` icons (Task 4).
- Produces: `PARENT_APP`, unchanged in type. Task 7 opens it from the lock.

Pages are rebuilt into the one app screen (`lv_obj_clean`) on a deferred call. A page button's own event must not delete that button while LVGL is still dispatching to it. Sleep times edits a local copy of Bedtime and Wake up. It saves them when a ± button is let go and in `destroy()`, because Home in the middle of a hold never delivers the release. So a hold does not write NVS ten times a second. Now sets the clock on every step.

- [ ] **Step 1: Add the tokens**

Append to `components/ui/include/tokens.h`:

```c
/* Parent home and Sleep times (design "Parent · home", "Parent · sleep times"; spec §3.12) */
#define COLOR_P_VALUE        0xFFC76B /* tile values and the row icons */
#define PARENT_TILE_SIZE     140
#define PARENT_TILE_Y        88
#define PARENT_TILE_VOL_X    88
#define PARENT_TILE_SLEEP_X  238
#define PARENT_BACK_X        158      /* "< Parents" */
#define PARENT_BACK_Y        34
#define PARENT_BACK_W        150
#define PARENT_BACK_H        52
#define PARENT_ROW_Y0        96       /* Sleep times: the first row's label; three rows 100 px apart */
#define PARENT_ROW_STEP      100
#define PARENT_ROW_VALUE_DY  28
#define PARENT_ROW_BTN_DY    12
#define PARENT_ROW_BTN       76
#define PARENT_ROW_MINUS_X   70
#define PARENT_ROW_PLUS_X    320
#define PARENT_TIMES_NOTE_Y  394
```

- [ ] **Step 2: Rewrite parent.c**

Replace `components/ui/parent.c` with:

```c
#include "parent.h"
#include <stdint.h>
#include "audio.h"
#include "clock.h"
#include "schedule.h"
#include "settings.h"
#include "tokens.h"
#include "ui_util.h"
#include "volume.h"

/* The parent area (spec §3.1, §3.12): a home screen with the Volume and Sleep tiles, Max volume and Sleep
 * times. The pages share the one app screen: a page switch cleans it and builds the next page. Home
 * (BOOT) leaves the whole area. */

typedef enum { PAGE_HOME, PAGE_VOLUME, PAGE_TIMES } page_t;
typedef enum { ROW_BED, ROW_WAKE, ROW_NOW, ROW_COUNT } row_t;

static const struct {
    const char *icon, *label;
} ROWS[ROW_COUNT] = {
    [ROW_BED] = {"ui/p_moon_row", "Bedtime"},
    [ROW_WAKE] = {"ui/p_sun_row", "Wake up"},
    [ROW_NOW] = {"ui/p_clock_row", "Now"},
};

static lv_obj_t *s_scr;              /* NULL once destroyed: every callback checks it */
static lv_obj_t *s_arc, *s_value;    /* Max volume */
static lv_obj_t *s_times[ROW_COUNT]; /* Sleep times: the three values; NULL off that page */
static lv_timer_t *s_now_timer;      /* keeps Now current while Sleep times shows */
static uint16_t s_bed, s_wake;       /* edited on Sleep times, saved when a button is let go */
static uint32_t s_press_ms;          /* when the current ± press began */

static void open_page(page_t page);

static lv_obj_t *text(lv_obj_t *parent, const lv_font_t *font, uint32_t color, int32_t x, int32_t y,
                      int32_t w, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(l, w);
    lv_obj_set_pos(l, x, y);
    lv_label_set_text(l, txt);
    return l;
}

static lv_obj_t *surface_button(int32_t x, int32_t y, int32_t w, int32_t h)
{
    lv_obj_t *b = lv_button_create(s_scr);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_P_SURFACE), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_P_BORDER), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 3, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(COLOR_P_BORDER), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    return b;
}

static lv_obj_t *label_in(lv_obj_t *parent, const lv_font_t *font, uint32_t color, const char *txt)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, txt);
    return l;
}

static void page_async(void *page)
{
    if (s_scr) {
        open_page((page_t)(uintptr_t)page);
    }
}

static void go_page(lv_event_t *e)
{
    /* Deferred: the page switch deletes the button this event came from. */
    lv_async_call(page_async, lv_event_get_user_data(e));
}

static void back_button(void)
{
    lv_obj_t *b = surface_button(PARENT_BACK_X, PARENT_BACK_Y, PARENT_BACK_W, PARENT_BACK_H);
    lv_obj_center(label_in(b, &lv_font_montserrat_20, COLOR_P_TEXT, LV_SYMBOL_LEFT " Parents"));
    lv_obj_add_event_cb(b, go_page, LV_EVENT_CLICKED, (void *)(uintptr_t)PAGE_HOME);
}

/* ---- home ---- */

static void tile(int32_t x, const char *icon, const char *label, const char *value, page_t page)
{
    lv_obj_t *b = surface_button(x, PARENT_TILE_Y, PARENT_TILE_SIZE, PARENT_TILE_SIZE);
    lv_obj_add_event_cb(b, go_page, LV_EVENT_CLICKED, (void *)(uintptr_t)page);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(b, 2, 0);
    ui_image(b, icon, 0, 0); /* the flex layout places it */
    label_in(b, &lv_font_montserrat_20, COLOR_P_TEXT, label);
    label_in(b, &lv_font_montserrat_16, COLOR_P_VALUE, value);
}

static void build_home(void)
{
    text(s_scr, &lv_font_montserrat_20, COLOR_P_MUTED, 133, 46, 200, "Parents");
    char vol[8], bed[6];
    lv_snprintf(vol, sizeof(vol), "%u%%", (unsigned)settings_volume_cap());
    schedule_format(settings_bedtime(), bed);
    tile(PARENT_TILE_VOL_X, "ui/p_vol", "Volume", vol, PAGE_VOLUME);
    tile(PARENT_TILE_SLEEP_X, "ui/p_moon", "Sleep", bed, PAGE_TIMES);
    text(s_scr, &lv_font_montserrat_16, COLOR_P_MUTED, 113, 398, 240, "Home button exits");
}

/* ---- Max volume ---- */

static void show_volume(void)
{
    const uint8_t v = settings_volume_cap();
    lv_arc_set_value(s_arc, v);
    lv_label_set_text_fmt(s_value, "%u%%", (unsigned)v);
}

static void step_volume(int dir)
{
    if (!s_scr || !s_arc) {
        return; /* destroyed, or the page is gone */
    }
    const uint8_t v = volume_step(settings_volume_cap(), dir);
    settings_set_volume_cap(v);
    audio_set_volume(v);
    audio_play(dir > 0 ? SND_BLIP_UP : SND_BLIP_DOWN); /* a click at the new level */
    show_volume();
}

static void on_minus(lv_event_t *e) { (void)e; step_volume(-1); }
static void on_plus(lv_event_t *e) { (void)e; step_volume(+1); }

static void build_volume(void)
{
    back_button();
    text(s_scr, &lv_font_montserrat_20, COLOR_P_MUTED, 133, 98, 200, "Max volume");
    s_arc = ui_arc(s_scr, PARENT_ARC_CX, PARENT_ARC_CY, PARENT_ARC_R, PARENT_ARC_W);
    lv_arc_set_bg_angles(s_arc, 135, 45); /* 270 degrees, gap at the bottom */
    lv_arc_set_range(s_arc, 0, VOLUME_MAX);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_ARC), LV_PART_INDICATOR);
    s_value = text(s_scr, &lv_font_montserrat_48, COLOR_P_TEXT, 153, 226, 160, "");
    lv_obj_t *m = surface_button(PARENT_MINUS_X, PARENT_BTN_Y, PARENT_BTN_SIZE, PARENT_BTN_SIZE);
    lv_obj_center(label_in(m, &lv_font_montserrat_40, COLOR_P_TEXT, "-"));
    lv_obj_add_event_cb(m, on_minus, LV_EVENT_CLICKED, NULL);
    lv_obj_t *p = surface_button(PARENT_PLUS_X, PARENT_BTN_Y, PARENT_BTN_SIZE, PARENT_BTN_SIZE);
    lv_obj_center(label_in(p, &lv_font_montserrat_40, COLOR_P_TEXT, "+"));
    lv_obj_add_event_cb(p, on_plus, LV_EVENT_CLICKED, NULL);
    show_volume();
}

/* ---- Sleep times ---- */

static void show_times(void)
{
    char t[6];
    schedule_format(s_bed, t);
    lv_label_set_text(s_times[ROW_BED], t);
    schedule_format(s_wake, t);
    lv_label_set_text(s_times[ROW_WAKE], t);
    if (clock_valid()) {
        schedule_format(clock_now_min(), t);
        lv_label_set_text(s_times[ROW_NOW], t);
    } else {
        lv_label_set_text(s_times[ROW_NOW], "--:--"); /* not set yet (spec §3.12) */
    }
}

static void save_times(void)
{
    if (s_bed != settings_bedtime()) {
        settings_set_bedtime(s_bed);
    }
    if (s_wake != settings_wake()) {
        settings_set_wake(s_wake);
    }
}

static void step_row(row_t row, int dir, uint32_t held_ms)
{
    if (row == ROW_BED) {
        s_bed = schedule_step(s_bed, dir * SCHEDULE_SLEEP_STEP);
    } else if (row == ROW_WAKE) {
        s_wake = schedule_step(s_wake, dir * SCHEDULE_SLEEP_STEP);
    } else if (!clock_valid()) {
        clock_set_now_min(SCHEDULE_NOW_START); /* the first press starts from 12:00 */
    } else {
        clock_set_now_min(schedule_step(clock_now_min(), dir * (int32_t)schedule_now_step(held_ms)));
    }
    show_times();
}

/* A ± button: one step on the press, then one per LVGL long-press repeat (every 100 ms after 400 ms).
 * User data: row * 2, plus 1 for +. */
static void time_button(lv_event_t *e)
{
    if (!s_scr || !s_times[ROW_BED]) {
        return; /* destroyed, or the page is gone */
    }
    const uintptr_t k = (uintptr_t)lv_event_get_user_data(e);
    const row_t row = (row_t)(k / 2);
    const int dir = (k & 1) ? 1 : -1;
    switch (lv_event_get_code(e)) {
    case LV_EVENT_PRESSED:
        s_press_ms = lv_tick_get();
        step_row(row, dir, 0);
        break;
    case LV_EVENT_LONG_PRESSED_REPEAT: step_row(row, dir, lv_tick_elaps(s_press_ms)); break;
    case LV_EVENT_RELEASED:
    case LV_EVENT_PRESS_LOST: save_times(); break;
    default: break;
    }
}

static void now_tick(lv_timer_t *t)
{
    (void)t;
    if (s_scr && s_times[ROW_NOW]) {
        show_times();
    }
}

static void build_times(void)
{
    back_button();
    s_bed = settings_bedtime();
    s_wake = settings_wake();
    for (int r = 0; r < ROW_COUNT; r++) {
        const int32_t y = PARENT_ROW_Y0 + r * PARENT_ROW_STEP;
        lv_obj_t *head = ui_plain(s_scr); /* icon and label, centered as a pair */
        lv_obj_set_pos(head, 113, y);
        lv_obj_set_size(head, 240, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(head, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(head, 8, 0);
        ui_image(head, ROWS[r].icon, 0, 0);
        label_in(head, &lv_font_montserrat_20, COLOR_P_MUTED, ROWS[r].label);
        s_times[r] = text(s_scr, &lv_font_montserrat_48, COLOR_P_TEXT, 153, y + PARENT_ROW_VALUE_DY, 160, "");
        for (int d = 0; d < 2; d++) {
            lv_obj_t *b = surface_button(d ? PARENT_ROW_PLUS_X : PARENT_ROW_MINUS_X, y + PARENT_ROW_BTN_DY,
                                         PARENT_ROW_BTN, PARENT_ROW_BTN);
            lv_obj_center(label_in(b, &lv_font_montserrat_40, COLOR_P_TEXT, d ? "+" : "-"));
            lv_obj_add_event_cb(b, time_button, LV_EVENT_ALL, (void *)(uintptr_t)(r * 2 + d));
        }
    }
    text(s_scr, &lv_font_montserrat_14, COLOR_P_MUTED, 118, PARENT_TIMES_NOTE_Y, 230,
         "Outside these hours\nthe toy sleeps");
    s_now_timer = lv_timer_create(now_tick, 1000, NULL);
    show_times();
}

/* ---- pages ---- */

static void leave_page(void)
{
    if (s_times[ROW_BED]) {
        save_times();
    }
    if (s_now_timer) {
        lv_timer_delete(s_now_timer);
        s_now_timer = NULL;
    }
    s_arc = s_value = NULL;
    for (int r = 0; r < ROW_COUNT; r++) {
        s_times[r] = NULL;
    }
}

static void open_page(page_t page)
{
    leave_page();
    lv_obj_clean(s_scr);
    if (page == PAGE_VOLUME) {
        build_volume();
    } else if (page == PAGE_TIMES) {
        build_times();
    } else {
        build_home();
    }
}

static void parent_create(lv_obj_t *scr)
{
    s_scr = scr;
    open_page(PAGE_HOME);
}

static void parent_destroy(void)
{
    leave_page(); /* saves the times: Home in the middle of a hold never delivers the release */
    s_scr = NULL;
}

const app_t PARENT_APP = {
    .id = "parent",
    .icon = NULL,
    .icon_small = NULL,
    .color = COLOR_P_ARC,
    .name_sound = SND_BLIP_UP, /* unused: the parent area is never in the carousel */
    .swipe_sound = SND_BLIP_UP,
    .react = CAT_HAPPY,
    .create = parent_create,
    .destroy = parent_destroy,
    .on_shake = NULL,
    .on_tilt = NULL,
};
```

In `components/ui/include/parent.h`, replace the comment with:

```c
/* The parent area (spec 3.1, 3.12). Opened by a two-finger hold on the menu or on the bedtime lock, never
 * from the carousel, so it is not in APPS. Home tiles (Volume, Sleep), Max volume and Sleep times; M7 adds
 * Apps, Cat and the battery. */
```

- [ ] **Step 3: Build**

Run:
```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1 && idf.py build 2>&1 | tail -3
idf.py build 2>&1 | grep -E "warning:" | grep -E "components/ui" || echo "no warnings"
grep -n "CONFIG_LV_FONT_MONTSERRAT_14=y" sdkconfig.defaults
```
Expected: `Project build complete.`, `no warnings`, and the font line is present. Montserrat 14, 16, 20, 40 and 48 are all enabled.

- [ ] **Step 4: Commit**

```bash
git add components/ui
git commit -m "feat: parent home tiles and the Sleep times screen with Now"
```

---

### Task 9: Device session, soak, docs and the PR (controller)

The controller runs this task itself, because it needs the board and the user's eyes. Never guess a result that needs eyes or hands on the board. Ask with AskUserQuestion, and put the full instructions inside the question text. Log every run under `build-logs/m6-*.log` (git-ignored).

- [ ] **Step 1: Verify the branch**

```bash
sh test/run.sh | tail -2
. ~/esp/esp-idf/export.sh >/dev/null 2>&1 && idf.py build 2>&1 | tail -1
```

- [ ] **Step 2: Flash the normal build without Wi-Fi, and run the checks**

The normal build has no credentials unless `sdkconfig.secrets` existed when its `sdkconfig` was created. The Task 3 boot log tells which ("no Wi-Fi configured"). Flash it: `tools/flash_and_log.sh 60 > build-logs/m6.log`. Ask the user, one AskUserQuestion per group:

- **A. Unchanged basics:**
  - the menu two-finger hold still opens the parent area after 5 s (the ring shows at 1 s);
  - PWR turns the screen off and on;
  - Home works from every app.

  The listener misses the first ~3 s of boot, so the boot-time lines (`USB power`, `time at boot`) may not show. The USB bit is checked through the lock's own log line (`lock (USB power: yes)`) and through the panel behavior in D.
- **B. The parent area and the manual clock** (Review Focus 3):
  - power the board off (hold PWR about 7 s) and on again, so the time is lost;
  - open the parent area. Check the Parents home with the Volume and Sleep tiles, the value colors and "Home button exits";
  - Volume opens Max volume, and "‹ Parents" goes back;
  - Sleep shows three rows. Now shows `--:--`. One + press sets 12:00;
  - holding + repeats, and after 2 s steps by 10;
  - set Now to 00:30 and check it stays (not `--:--`);
  - Bedtime and Wake up step by 15 and repeat on hold;
  - Home leaves. Reopening shows the saved Bedtime on the Sleep tile.
- **C. Bedtime starting** (Review Focus 1):
  - set Bedtime to the next quarter hour and Now to one minute before it, then open Magic paws and draw;
  - at the minute, the app closes with no mew and the lock fades in: moon, nine twinkling stars, sleeping cat, five hour stars lit (wake-up more than 4 h away), dim;
  - repeat from the menu (no app open), and with the parent area open (the lock shows only on leaving).
- **D. On the lock** (Review Focus 2):
  - Home does nothing;
  - a tap whispers "Pssst… spavaj", quieter than other sounds but audible, and the cat shows its shh face for about 2 s; a second tap within 5 s is silent;
  - PWR off, PWR on;
  - on USB the scene stays lit for more than 30 s;
  - unplug USB: about 30 s later the panel is off; a tap lights it for about 5 s with the whisper; plugging USB back in lights it for good;
  - the two-finger hold shows the ring around the cat's head and opens the parent area; Home returns to the lock;
  - moving Bedtime so that it is no longer night, then Home, goes to the menu with no sunrise.
- **E. The sunrise:**
  - from the lock, set Wake up to the next quarter hour and Now to one minute before it, then Home back to the lock;
  - at the minute, the sun rises over about 4 s while the screen brightens, then the chime, a meow and "Dobro jutro!";
  - a tap during the rise does nothing; a tap after it goes to the menu without the mew, and the menu does not dim at once;
  - repeat once and wait 60 s: the menu comes by itself.
- **F. Restart inside the night:** with the lock showing, reset the board without cutting its power (for example `python -m esptool --port /dev/cu.usbmodem101 chip_id`, which hard-resets the chip on exit), then listen with the passive logger. The boot animation plays, then the lock shows: the time survives a restart.
- **G. The Sleepy clock app** (daytime):
  - the sun rises on open, the rays turn slowly, then the hills and the proud cat;
  - a tap on the cat purrs (face and sound); a tap on the sky does nothing;
  - Home.

Record every answer. Anything that fails goes to `superpowers:systematic-debugging` before any fix, and gets its own `fix:` commit and a re-check.

- [ ] **Step 3: FPS**

Build and flash the perf variant (`-B build-perf ... sdkconfig.perf`). Log the Sleepy clock (rays turning plus the rise) and the lock (twinkling stars) for 20 s each, and read the `sysmon` lines. Target: 24 fps or more (spec §7). The spec has no fallback for the rays. If they fall below 24 fps, ask the user before changing anything, as with the M5 piano.

- [ ] **Step 4: NTP and drift** (Review Focus 4)

Ask the user whether `sdkconfig.secrets` exists. If they want NTP tested, they create it themselves (never ask for the password in chat). Build a separate directory that reads it:
```bash
idf.py -B build-ntp -D SDKCONFIG=build-ntp/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.secrets" build
tools/flash_and_log.sh 40 -B build-ntp -D SDKCONFIG=build-ntp/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.secrets" > build-logs/m6-ntp.log
```
Check in the log:
- `NTP: corrected by ...`, `time now: <correct local time>`;
- `after the sync:` heap deltas;
- the UI stays smooth during the sync (ask the user to swipe the menu meanwhile).

For the failure path, build `build-ntpfail` with `SDKCONFIG_DEFAULTS="sdkconfig.defaults;build-ntpfail/bogus.secrets"`. There `build-ntpfail/bogus.secrets` holds a made-up SSID and password (created by the controller, git-ignored under `build-*`). Expect `NTP sync failed` within about 10 s of boot, with heap deltas close to the successful run.

For the drift: in the `build-ntp` firmware, set Wake up so that the wake-up minute comes a few minutes later (a daily sync). Then compare the `corrected by` value against the seconds since the previous sync. Repeat at the end of the session (hours later), and record the corrections and the implied seconds per day in `docs/device-checks.md` (spec §3.2).

- [ ] **Step 5: Soak**

```bash
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" build
tools/flash_and_log.sh 420 -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" > build-logs/m6-soak.log
grep SOAK build-logs/m6-soak.log | tail -3
```
Expected: `PASS` (internal and PSRAM within ±4 KB over 50 rounds, now with the real Sleepy clock).

- [ ] **Step 6: Record the results**

Append an "M6 time and bedtime" section to `docs/device-checks.md`: groups A-G, the FPS, NTP (success, failure, heap), the drift so far, the soak, and the whisper loudness. Commit: `docs: M6 device results`.

- [ ] **Step 7: Final review, fix wave, PR**

Run the final whole-branch review on the most capable model (the diff from `5c66476`), then one fix wave and a scoped re-review. Open the PR from `davor/m6-time-bedtime` against `davor/m5-paws-yarn-piano` (stacked; retarget to `main` once #5 and #6 merge), with no attribution trailers. End with the headings Blocked on me, Changed, Found.
