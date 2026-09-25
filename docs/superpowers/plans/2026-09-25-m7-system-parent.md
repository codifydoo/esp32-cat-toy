# M7 System and Parent Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The toy looks after itself and its parent:
- a first-run cat picker;
- the hungry-cat and eating-cat screens for a low battery and for charging;
- the full parent area, in Fredoka;
- deep sleep on battery, with the right screen on waking;
- the menu icon's breathing against burn-in.

**Architecture:**
- **Host-tested logic (`components/logic`):**
  - `power_rules.c` (new): when a low-battery screen is due, when the battery is critical, and what a restart shows;
  - `schedule.c`: when deep sleep may start, and the minute its timer wakes the toy.
- **Platform (`components/platform`):**
  - `power.c`: the AXP2101 fuel gauge and charger, polled by the sensors task, which also reports USB being plugged in;
  - `deepsleep.c` (new): entering deep sleep and reading why the chip woke;
  - `display.c`: panel sleep;
  - `clock.c`: seconds until a local minute, and whether a sync runs;
  - `settings.c`: the first-run flag.
- **Cat and assets:**
  - `cat.c` tracks its live cats, so a new variant reloads them;
  - `assets.c` can release a variant's cached images.
- **UI (`components/ui`):**
  - `first_run.c` (new): the picker, used at first run and from the parent area;
  - `overlays.c` (new): the hungry-cat and eating-cat screens;
  - `power_watch.c` (new): battery events, the critical sleep and the deep-sleep countdown;
  - `parent.c`: four tiles, the battery, Show apps and Cat, in Fredoka;
  - `app_manager.c`: the restart choice, first run, and routing around the overlays;
  - `bedtime.c`: the lock after waking, lit or dark, and the sunrise at once;
  - `launcher.c`: paw reload and the icon breathing.

**Tech Stack:**
- ESP-IDF v5.5.5 (C11): `esp_sleep` ext1 and timer wake, and `driver/rtc_io`;
- LVGL 9.4;
- `waveshare/esp32_s3_touch_amoled_1_75c` 3.0.0;
- CMake + Unity host tests under UBSan;
- Node + resvg for images, sox for sounds, `lv_font_conv` for fonts.

**Spec:** `docs/superpowers/specs/2026-09-22-cat-toy-design.md`, mainly:
- §3.13 (new for M7): the battery gauge, low battery, charging, the cat picker, the parent area, deep sleep and burn-in;
- §3.7: deep sleep and its wake sources;
- §3.8 and §3.12: the lock and the sunrise that deep sleep hooks into;
- §4.5: the settings, including "first run done";
- §4.6: the memory budget (one cat variant loaded);
- §4.7: an unavailable gauge hides every battery screen;
- §7: the leak soak;
- §8: no network beyond NTP; recordings are discarded when the device sleeps.

The design is the UI source of truth:
- `design/claude-design/Screen.dc.html`:
  - the views `pick` (lines 28-39), `lowbatt` and `charging` (292-326);
  - `phome`, `pvolume`, `ptimes` and `papps` (340-398);
  - the scripts' `PICK`, `V` (fur colors), `ICON` (`grid`, `cat`), `tiles`, `catNames` and `appTiles` (555-600);
- `Main.dc.html` `onAct` `pick`/`pcat`/`papp`: the 1400 ms celebration, and at most 5 of 6 apps hidden;
- `Screens.dc.html`:
  - the keyframes `charge` (2 s, scaleX 0.15 → 1) and `twinkle` (`.blink-slow`, 1.6 s);
  - `.icon-breathe` (`breathe`, 3 s, scale 1 → 1.025);
  - the notes "Pick your cat", "Low battery" and "Charging";
- `Spec.dc.html`: "Burn-in and power", and the Fredoka sizes.

## Global Constraints

- **Toolchain:** ESP-IDF v5.5.5, target `esp32s3`.
  - Run every `idf.py` command in a shell that has sourced `~/esp/esp-idf/export.sh`. The port is `/dev/cu.usbmodem101`.
  - After adding a `.c` file or a source directory to a component, reconfigure each build directory you build (`idf.py reconfigure`, or `-B <dir> ... reconfigure`): sources are globbed at configure time.
- **Build variants:**
  - Each is `idf.py -B build-X -D SDKCONFIG=build-X/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.X" build`.
  - Existing variants: `diag`, `soak`, `perf` and `gallery`. This plan adds `powerdiag` (Task 1), `power` and `drain` (Task 3).
  - The normal build is `idf.py build`. Its `sdkconfig` reads the git-ignored `sdkconfig.secrets` when it was created.
- **Assets:**
  - `assets/` is generated and git-ignored. After changing anything under `tools/export_images/` or `tools/build_sounds/`, run `tools/build_assets.sh`.
  - The generated manifest `components/assets/assets_manifest.{c,h}` is committed.
  - `idf.py build` fails if `assets/` does not fit the assets partition.
- **Board quirks:**
  - Opening the serial port resets the board. Always flash with `tools/flash_and_log.sh`.
  - Only the controller flashes, and only the controller runs the device steps (the end of Task 1, and Task 11). Implementers build only.
- **Secrets (spec §8):**
  - Wi-Fi credentials live only in the git-ignored `sdkconfig.secrets`.
  - Never read, print, log, copy or commit that file, any generated `sdkconfig` (top-level or `build*/sdkconfig`), or the password. When comparing configs, filter out `CAT_WIFI` lines first.
  - Never grep generated configs at all: the Kconfig defaults and the committed `sdkconfig.*` overlays say what a build contains.
  - Logs under `build-logs/` may hold the network name from before M6's fix. Do not open `build-logs/m6-ntp*.log`.
- **Commits:** prefixes `feat:`, `fix:`, `test:`, `chore:` and `docs:`. No attribution trailers of any kind.
- **Downloads:** only the two approved in Task 5:
  - the Fredoka TTF and its `OFL.txt` from `github.com/google/fonts` (`ofl/fredoka`);
  - `lv_font_conv` from npm, under `tools/fonts/`.
  Nothing else gets downloaded or installed.
- **Spec §3.13, summarized (the plan's requirements):**
  - **Gauge:**
    - the AXP2101 fuel gauge (0-100 %) and the charger status;
    - unavailable when the AXP2101 reports no battery or the reading is outside 0-100. Then no battery screen shows, and the parent home hides the percentage.
  - **Low battery:**
    - the hungry cat (sleepy face, empty bowl, blinking red battery, "Gladna sam…") for 4 s, once at 10 % and once at 5 %; connecting USB arms both again;
    - it shows over the menu or an open app, then play resumes. The app keeps its state, and touches, tilt and shake do not reach it meanwhile. Home works as always;
    - it never shows over the boot animation, the cat picker, the parent area, the lock or the sunrise. A level reached there, or while the panel is off, shows the next time the menu or an app is lit;
    - **at 2 % on battery the toy deep-sleeps until charged.** The hungry cat shows for 4 s first if the panel is on. While asleep for the battery, a touch or BOOT shows the hungry cat for 4 s and it sleeps again. No timer is set. With USB connected, the next touch wakes it normally.
  - **Charging:**
    - the eating cat (full bowl, filling green battery, "Njam njam!" with 2 s of munching) for 4 s, when USB power is connected and the gauge is available;
    - it shows over the menu or an open app, as low battery does. Connecting USB wakes a dim or dark menu first;
    - it never shows over the boot animation, the picker, the parent area, the lock or the sunrise, and it is never shown later.
  - **Cat picker:**
    - the design's three 156 px pads with the cats at scale S; the current cat's pad has the thick ring and a tinted fill;
    - **first run** = the "first run done" setting is missing. The boot animation plays, then the picker;
    - a tap chooses at once: the thick ring, the proud face, a meow; 1.4 s later the menu (or the lock at night). The choice and "first run done" are saved on the tap;
    - Home on the first-run picker goes to the menu, keeps the orange cat and saves nothing;
    - the parent area's Cat tile opens the same picker, which returns to the parent home;
    - after a choice, the other variants' images are released.
  - **Parent area:**
    - four tiles: Volume, Sleep, Apps ("5 of 6") and Cat (Orange, Grey or Tuxedo), with the battery percentage in the center;
    - Show apps: the design's grid of six 96 px icons. A tap hides or shows an app (30 %, a muted label, the crossed-eye badge). The last shown app cannot be hidden. Each change is saved at once;
    - the text is Fredoka at 17, 20, 22, 46 and 54 px.
  - **Deep sleep:**
    - it starts on battery 10 minutes after the panel went off (the menu's idle off or PWR, or the lock's dark panel). Never on USB, never in the last 15 minutes before wake time (including the wake minute), and never during an NTP sync;
    - before sleeping: stop the sound, put the panel to sleep, and hold the speaker amplifier off;
    - wake sources: a touch (GPIO11), BOOT (GPIO0), and a timer 10 minutes before wake time while the clock is valid and there is a night;
    - PWR wakes it only if Task 1 finds the AXP2101 interrupt on a free RTC pin;
    - the restart shows:
      - after the timer inside the night: the lock with its panel dark; the sync is due and the sunrise follows at wake time;
      - after the timer past wake time: the sunrise at once;
      - after a touch or BOOT inside the night: the lock, lit for 5 s, without the boot animation;
      - otherwise: the boot animation;
    - RAM is lost: Magic paws' drawing, and a recording.
  - **Burn-in:** the menu icon breathes 100 → 102.5 % over 3 s, except while the menu is dimmed.

## Review Focus

1. **Changing the cat while other cats are alive.** The menu's cat, the lock's cat when the parent area is open at night, and the menu's two paw images must all switch to the new variant. The old variant's images must be released only after nothing shows them. Expected: no magenta placeholder, no crash, and free PSRAM back within ~0.5 MB of its first-boot level after switching three times. Pinned by Task 4 (the cat registry and the release), Task 6 (`launcher_cat_changed()` and the deferred release) and the Task 11 check B.
2. **Deep sleep entry with something in flight:**
   - an NTP sync: no sleep until it ends;
   - a sound: stopped first;
   - a finger on the glass: the toy wakes at once, and the wake path still works;
   - a battery screen showing: no sleep until it ends (except the critical one, which sleeps when it ends);
   - the 15 minutes before wake time: never.
   Pinned by Task 2's `schedule_sleep_allowed()` tests and Task 9's countdown, and checked on the device in Task 11 (E).
3. **The gauge unavailable or wild** (no battery, a reading of 101 or 255, a jump from 50 to 3). Expected: no battery screen and no critical sleep while unavailable. A jump down shows one hungry cat, not two. Pinned by Task 2's `battery_rules` tests.
4. **USB plugged or unplugged at the edges:**
   - during the boot animation: no charging screen, and none later;
   - on the lock: no screen, and the night light follows;
   - in the parent area: no screen;
   - while the hungry cat shows: no second screen;
   - while deep-sleeping: no wake, and the next touch starts normally.
   Pinned by Task 8's `can_show()` and Task 9's plan, and checked in Task 11 (C, D).
5. **Waking at the wrong minute.** The timer runs on the RTC slow clock. It may fire early (lock dark, the sunrise at wake time) or after wake time (the sunrise at once), and on a DST night by an hour. Expected: never a lit lock in the morning, never a missing sunrise. Pinned by Task 2's `power_wake_plan()` tests, `clock_secs_until()` using `mktime` (DST), and the Task 11 check E.

---

## File structure

| File | Task | Responsibility |
| --- | --- | --- |
| `main/power_diag.c/.h`, `main/Kconfig.projbuild`, `sdkconfig.powerdiag`, `main/app_main.c` | 1 | Hardware facts: gauge registers, the touch INT, a PWR IRQ pin, deep-sleep wakes |
| `components/platform/display.c/.h` | 1 | `display_sleep()` |
| `tools/serial_log.py` | 1 | Survives the USB port dropping in deep sleep; optional host timestamps |
| `components/logic/power_rules.c/.h`, `test/test_power_rules.c` | 2 | Battery screens, the critical level, the restart plan |
| `components/logic/schedule.c/.h`, `test/test_schedule.c` | 2 | When deep sleep may start; the wake timer's minute |
| `components/platform/power.c/.h`, `sensors.c/.h` | 3 | Gauge, charger, 500 ms poll, USB-in event |
| `components/platform/deepsleep.c/.h` | 3 | Enter deep sleep; the wake cause |
| `components/platform/clock.c/.h`, `settings.c/.h` | 3 | `clock_secs_until()`, `clock_sync_running()`, the first-run flag |
| `main/Kconfig.projbuild`, `sdkconfig.power`, `sdkconfig.drain` | 3 | Dev options for the device checks |
| `tools/export_images/ui_images.mjs`, `tools/build_sounds/sounds.txt`, `components/assets/*` | 4 | Battery glyphs, bowls, parent icons, eye badge; the charging sound; releasing images |
| `components/cat/cat.c/.h` | 4 | Live-cat registry; `cat_set_variant()` reloads; `cat_variant_name()` |
| `assets-src/fonts/*`, `tools/fonts/*`, `components/ui/fonts/*.c`, `components/ui/include/fonts.h` | 5 | Fredoka for LVGL |
| `components/ui/first_run.c/.h`, `launcher.c/.h`, `app_manager.c/.h` | 6 | The picker and first run |
| `components/ui/parent.c/.h` | 7 | Four tiles, battery, Show apps, Cat, Fredoka |
| `components/ui/overlays.c/.h`, `power_watch.c/.h`, `app_manager.c` | 8 | Hungry and eating cat; battery events; critical sleep |
| `components/ui/power_watch.c`, `bedtime.c/.h`, `app_manager.c/.h`, `main/app_main.c` | 9 | The deep-sleep countdown; the restart plan |
| `components/ui/launcher.c` | 10 | Icon breathing |
| `components/ui/include/tokens.h` | 4, 6-10 | Design constants |
| `README.md`, `docs/device-checks.md`, the spec | 1, 3, 5, 11 | Device results; dev options; the Fredoka note |

---

### Task 1: Power diagnostics (the hardware facts first)

Deep sleep and the battery screens rest on four facts nobody has checked on this board:
- what the AXP2101 fuel gauge and charger report;
- whether the touch controller's INT (GPIO11) can wake the chip from deep sleep;
- whether the AXP2101 interrupt reaches a free RTC pin, so PWR could wake it;
- how far the RTC slow clock drifts in deep sleep.

This task builds a diagnostic firmware. Its last step is the controller's, on the device, before Task 2 starts.

**Files:**
- Create: `main/power_diag.c`, `main/power_diag.h`, `sdkconfig.powerdiag`
- Modify: `main/Kconfig.projbuild`, `main/app_main.c`, `components/platform/display.c`, `components/platform/include/display.h`, `tools/serial_log.py`, `docs/device-checks.md` (controller, Step 6)

**Interfaces:**
- Consumes: `pmu_read/pmu_write` (`pmu.h`), `buttons_init/buttons_poll` (`buttons.h`), `display_lock/display_unlock`.
- Produces:
  - `void display_sleep(void)` in `display.h` (used by Task 3's `deepsleep.c`);
  - `tools/serial_log.py PORT [SECONDS] [-t]`, which survives the port vanishing (used in Tasks 1 and 11);
  - the device facts in `docs/device-checks.md`, "Power diagnostics (M7)". Task 3 needs two of them: whether the gauge had to be switched on, and the PWR pin, if any.

- [ ] **Step 1: `display_sleep()`**

In `components/platform/display.c`, keep the panel handle. Replace the local `esp_lcd_panel_handle_t panel = NULL;` in `display_init()` with a file-level `static esp_lcd_panel_handle_t s_panel;`, and use `s_panel` in its two places (`bsp_display_new(&panel_cfg, &s_panel, &s_io)` and `.panel = s_panel`). Add `#include "esp_lcd_panel_ops.h"`, and:

```c
void display_sleep(void)
{
    display_set_brightness(0);
    if (s_panel) {
        esp_lcd_panel_disp_on_off(s_panel, false);
        esp_lcd_panel_disp_sleep(s_panel, true); /* CO5300 sleep-in; the restart after deep sleep wakes it */
    }
}
```

In `components/platform/include/display.h`, after `display_brightness()`:

```c
/* Brightness 0, display off, then the panel's sleep mode: only right before deep sleep, since only the
 * restart after it wakes the panel. LVGL task, or with display_lock() held. */
void display_sleep(void);
```

- [ ] **Step 2: `tools/serial_log.py` survives deep sleep, and can stamp lines**

The USB port vanishes while the chip deep-sleeps, and today `s.read()` then raises and the logger dies. Replace the file's `main()` and add `stamp()`; keep `open_port()` exactly as it is:

```python
def stamp():
    t = time.time()
    return time.strftime("%H:%M:%S", time.localtime(t)) + ".%03d " % int((t % 1) * 1000)


def main():
    stamps = "-t" in sys.argv[1:]
    args = [a for a in sys.argv[1:] if a != "-t"]
    port = args[0]
    seconds = float(args[1]) if len(args) > 1 else 10.0
    end = time.time() + seconds
    s = open_port(port, time.time() + 5)
    at_line_start = True
    try:
        while time.time() < end:
            try:
                data = s.read(4096)
            except serial.SerialException:
                # Deep sleep takes USB down with the chip: wait for the port to come back.
                s.close()
                sys.stdout.write("\n[port gone at %s]\n" % stamp().strip())
                sys.stdout.flush()
                try:
                    s = open_port(port, end)
                except serial.SerialException:
                    return
                sys.stdout.write("[port back at %s]\n" % stamp().strip())
                at_line_start = True
                continue
            if not data:
                continue
            text = data.decode("utf-8", "replace")
            if stamps:
                pieces = text.split("\n")
                out = []
                for i, piece in enumerate(pieces):
                    if at_line_start and piece:
                        out.append(stamp())
                        at_line_start = False
                    out.append(piece)
                    if i < len(pieces) - 1:
                        out.append("\n")
                        at_line_start = True
                text = "".join(out)
            sys.stdout.write(text)
            sys.stdout.flush()
    finally:
        s.close()
```

Update the module docstring to:

```python
"""Print the board's serial output for N seconds (a non-interactive idf.py monitor).

Usage: serial_log.py PORT [SECONDS] [-t]
-t prefixes each line with the host time (HH:MM:SS.mmm), to time deep sleep against the RTC. The port
vanishes while the chip deep-sleeps (USB goes down with it); the logger waits for it and says so."""
```

Check: `python3 -m py_compile tools/serial_log.py` prints nothing.

- [ ] **Step 3: The Kconfig option and the build variant**

Add to `main/Kconfig.projbuild`, after `CAT_DEV_SOAK`:

```
    config CAT_DEV_POWER_DIAG
        bool "Boot into the power diagnostics (gauge, touch INT, PWR pin, deep-sleep wakes)"
        default n
```

Create `sdkconfig.powerdiag` containing the single line `CONFIG_CAT_DEV_POWER_DIAG=y`.

In `main/app_main.c`, add after the `CONFIG_CAT_DEV_SOAK` include block:

```c
#if CONFIG_CAT_DEV_POWER_DIAG
#include "power_diag.h"
#endif
```

and turn the variant branch into:

```c
#if CONFIG_CAT_DEV_DIAG
    diag_start();
#elif CONFIG_CAT_DEV_GALLERY
    cat_set_variant(CAT_VARIANT_ORANGE);
    gallery_start();
#elif CONFIG_CAT_DEV_POWER_DIAG
    power_diag_start();
#else
```

(The `#else` body stays as it is.)

- [ ] **Step 4: `main/power_diag.h` and `main/power_diag.c`**

`main/power_diag.h`:

```c
#pragma once
void power_diag_start(void);
```

`main/power_diag.c`:

```c
#include "sdkconfig.h"
#if CONFIG_CAT_DEV_POWER_DIAG
#include "power_diag.h"
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <sys/time.h>
#include "bsp/esp-bsp.h"
#include "buttons.h"
#include "display.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "pmu.h"

/* M7 Task 1: the hardware facts deep sleep and the battery screens rest on (docs/device-checks.md, "Power
 * diagnostics (M7)"). Round 0 (a cold start) watches the gauge, the touch INT and the free RTC pins for
 * WATCH_S while the user presses PWR and touches the screen, then sleeps TIMER_SLEEP_S on the timer alone.
 * Round 1 expects that timer wake and sleeps until a touch; round 2 until BOOT; round 3 until PWR, only if
 * round 0 saw a pin follow PWR. Rounds 1-3 also wake by themselves after FALLBACK_S. */

#define WATCH_S       40
#define TIMER_SLEEP_S 180
#define FALLBACK_S    300
#define READ_S        8 /* each message stays this long before the screen goes dark */

#define REG_STATUS1   0x00 /* bit 5 VBUS good, bit 3 battery present */
#define REG_STATUS2   0x01 /* bits 6:5 current direction (1 charging), bits 2:0 charger state */
#define REG_GAUGE_CTL 0x18 /* bit 3 fuel gauge on (XPowersLib enableGauge) */
#define REG_ADC_EN    0x30 /* bit 0 battery voltage ADC */
#define REG_VBAT_H    0x34 /* battery voltage in mV: 6 high bits ... */
#define REG_VBAT_L    0x35 /* ... and 8 low bits */
#define REG_PERCENT   0xA4 /* fuel gauge, 0-100 % */

static const gpio_num_t CANDIDATES[] = {GPIO_NUM_3, GPIO_NUM_13, GPIO_NUM_17, GPIO_NUM_18, GPIO_NUM_21};
#define N_CAND (sizeof(CANDIDATES) / sizeof(CANDIDATES[0]))

static const char *TAG = "PWRDIAG";
RTC_DATA_ATTR static uint32_t s_round;
RTC_DATA_ATTR static int64_t s_sleep_at_us; /* gettimeofday() at sleep entry: RTC time across the sleep */
RTC_DATA_ATTR static uint32_t s_pwr_pins;   /* bit i: CANDIDATES[i] was low at every PWR press in round 0 */
static lv_obj_t *s_label;
static volatile uint32_t s_int_edges;

static void say(const char *fmt, ...)
{
    char buf[200];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    ESP_LOGI(TAG, "screen: %s", buf);
    if (display_lock(-1)) {
        lv_label_set_text(s_label, buf);
        display_unlock();
    }
}

static uint8_t reg(uint8_t r)
{
    uint8_t v = 0xFF;
    pmu_read(r, &v);
    return v;
}

static void log_power(void)
{
    const uint8_t s1 = reg(REG_STATUS1), s2 = reg(REG_STATUS2);
    const unsigned mv = ((reg(REG_VBAT_H) & 0x3Fu) << 8) | reg(REG_VBAT_L);
    ESP_LOGI(TAG, "status1 0x%02x (vbus %d, battery %d) status2 0x%02x (direction %d, charger %d) gauge_ctl 0x%02x "
                  "adc 0x%02x percent %u vbat %u mV",
             s1, (s1 >> 5) & 1, (s1 >> 3) & 1, s2, (s2 >> 5) & 3, s2 & 7, reg(REG_GAUGE_CTL), reg(REG_ADC_EN),
             reg(REG_PERCENT), mv);
}

static void IRAM_ATTR int_edge(void *arg)
{
    (void)arg;
    s_int_edges++;
}

static int64_t rtc_now_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000000 + tv.tv_usec;
}

/* Round 0: the gauge, the touch INT and which free RTC pin (if any) follows PWR. */
static void watch(void)
{
    say("Round 0: press PWR 3 times, a few seconds apart, and touch the screen a few times. %d s.", WATCH_S);
    const uint8_t ctl = reg(REG_GAUGE_CTL);
    ESP_LOGI(TAG, "gauge %s at start (0x18 = 0x%02x)", (ctl & 0x08) ? "on" : "off", ctl);
    log_power();
    if (!(ctl & 0x08)) {
        pmu_write(REG_GAUGE_CTL, ctl | 0x08);
        ESP_LOGI(TAG, "gauge switched on");
    }
    uint8_t prev[N_CAND];
    uint32_t low_at_start = 0, followed = UINT32_MAX, presses = 0;
    for (size_t i = 0; i < N_CAND; i++) {
        const gpio_config_t c = {.pin_bit_mask = BIT64(CANDIDATES[i]), .mode = GPIO_MODE_INPUT, .pull_up_en = 1};
        gpio_config(&c);
    }
    vTaskDelay(pdMS_TO_TICKS(20));
    for (size_t i = 0; i < N_CAND; i++) {
        prev[i] = (uint8_t)gpio_get_level(CANDIDATES[i]);
        low_at_start |= prev[i] ? 0 : 1u << i;
        ESP_LOGI(TAG, "gpio%d starts %d", CANDIDATES[i], prev[i]);
    }
    gpio_install_isr_service(0); /* ESP_ERR_INVALID_STATE if already installed: fine */
    gpio_set_intr_type(BSP_LCD_TOUCH_INT, GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(BSP_LCD_TOUCH_INT, int_edge, NULL);
    gpio_intr_enable(BSP_LCD_TOUCH_INT);
    buttons_init();
    const int64_t end = esp_timer_get_time() + (int64_t)WATCH_S * 1000000;
    int64_t next_log = 0;
    while (esp_timer_get_time() < end) {
        uint32_t low_now = 0;
        for (size_t i = 0; i < N_CAND; i++) { /* sampled before buttons_poll() clears the PWR interrupt */
            const uint8_t lvl = (uint8_t)gpio_get_level(CANDIDATES[i]);
            if (lvl != prev[i]) {
                ESP_LOGI(TAG, "gpio%d -> %d", CANDIDATES[i], lvl);
                prev[i] = lvl;
            }
            low_now |= lvl ? 0 : 1u << i;
        }
        if (buttons_poll() & BUTTON_EV_POWER) {
            presses++;
            followed &= low_now;
            ESP_LOGI(TAG, "PWR press %" PRIu32 ": candidates low 0x%02" PRIx32, presses, low_now);
        }
        if (esp_timer_get_time() >= next_log) {
            log_power();
            ESP_LOGI(TAG, "touch INT edges so far: %" PRIu32, s_int_edges);
            next_log = esp_timer_get_time() + 2000000;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    gpio_isr_handler_remove(BSP_LCD_TOUCH_INT);
    s_pwr_pins = presses ? followed & ~low_at_start : 0;
    ESP_LOGI(TAG, "round 0 done: %" PRIu32 " PWR presses, pins that followed PWR 0x%02" PRIx32 ", touch INT edges %" PRIu32,
             presses, s_pwr_pins, s_int_edges);
}

static void sleep_now(uint32_t seconds, uint32_t pwr_bits, const char *text)
{
    say("%s (the screen goes dark in %d s)", text, READ_S);
    vTaskDelay(pdMS_TO_TICKS(READ_S * 1000));
    uint64_t mask = BIT64(GPIO_NUM_0) | BIT64(BSP_LCD_TOUCH_INT);
    for (size_t i = 0; i < N_CAND; i++) {
        if (pwr_bits & (1u << i)) {
            mask |= BIT64(CANDIDATES[i]);
        }
    }
    ESP_ERROR_CHECK(esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW));
    ESP_ERROR_CHECK(esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON)); /* keeps the pull-ups */
    for (int pin = 0; pin < 22; pin++) {
        if (mask & BIT64(pin)) {
            rtc_gpio_pullup_en((gpio_num_t)pin);
            rtc_gpio_pulldown_dis((gpio_num_t)pin);
        }
    }
    ESP_ERROR_CHECK(esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000));
    s_sleep_at_us = rtc_now_us();
    ESP_LOGI(TAG, "sleeping: round %" PRIu32 ", timer %" PRIu32 " s, ext1 pins 0x%" PRIx64, s_round, seconds, mask);
    display_lock(-1); /* kept: nothing may draw again before the sleep */
    display_sleep();
    vTaskDelay(pdMS_TO_TICKS(100)); /* the log line drains */
    esp_deep_sleep_start();
}

static void diag_task(void *arg)
{
    (void)arg;
    const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_UNDEFINED) {
        s_round = 0; /* a cold start or reset: RTC memory was reset too */
    } else {
        s_round++;
        const uint64_t pins = cause == ESP_SLEEP_WAKEUP_EXT1 ? esp_sleep_get_ext1_wakeup_status() : 0;
        const double slept = (double)(rtc_now_us() - s_sleep_at_us) / 1e6;
        ESP_LOGI(TAG, "woke: cause %d (%s), ext1 pins 0x%" PRIx64 ", slept %.1f s by the RTC", (int)cause,
                 cause == ESP_SLEEP_WAKEUP_TIMER ? "timer" : cause == ESP_SLEEP_WAKEUP_EXT1 ? "pin" : "other", pins,
                 slept);
        say("Woke by %s (pins 0x%llx) after %.0f s.", cause == ESP_SLEEP_WAKEUP_TIMER ? "the timer" : "a pin",
            (unsigned long long)pins, slept);
        for (int i = 0; i < 10; i++) { /* host time minus uptime = boot time, for the drift (serial_log -t) */
            ESP_LOGI(TAG, "alive, uptime %lld ms", (long long)(esp_timer_get_time() / 1000));
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    switch (s_round) {
    case 0:
        watch();
        sleep_now(TIMER_SLEEP_S, 0, "Round 1: sleeping 3 minutes. Don't touch anything: it wakes by itself.");
        break;
    case 1: sleep_now(FALLBACK_S, 0, "Round 2: sleeping. Wait 20 s, then touch the screen once to wake it."); break;
    case 2: sleep_now(FALLBACK_S, 0, "Round 3: sleeping. Wait 20 s, then press BOOT to wake it."); break;
    case 3:
        if (s_pwr_pins) {
            sleep_now(FALLBACK_S, s_pwr_pins, "Round 4: sleeping. Wait 20 s, then press PWR (short) to wake it.");
        }
        break;
    default: break;
    }
    say("Done. Leave it on USB.");
    for (;;) {
        log_power();
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void power_diag_start(void)
{
    pmu_init();
    display_lock(-1);
    lv_obj_t *scr = lv_screen_active();
    s_label = lv_label_create(scr);
    lv_obj_set_width(s_label, 360);
    lv_label_set_long_mode(s_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_label);
    display_unlock();
    xTaskCreatePinnedToCore(diag_task, "pwrdiag", 6144, NULL, 3, NULL, 0);
}
#endif
```

- [ ] **Step 5: Build every variant touched**

```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py -B build-powerdiag -D SDKCONFIG=build-powerdiag/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.powerdiag" build 2>&1 | tail -1
idf.py -B build-m7 -D SDKCONFIG=build-m7/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" build 2>&1 | tail -1
sh test/run.sh 2>&1 | grep "tests passed"
```

Expected:
- both builds: `Project build complete`, with no `warning:` in `components/` or `main/`;
- host tests: `100% tests passed out of 13`.

`build-m7` is the no-Wi-Fi normal build the later tasks reuse.

Commit:

```bash
git add main/power_diag.c main/power_diag.h main/Kconfig.projbuild main/app_main.c sdkconfig.powerdiag \
        components/platform/display.c components/platform/include/display.h tools/serial_log.py
git commit -m "feat: power diagnostics build, panel sleep, and a logger that survives deep sleep"
```

- [ ] **Step 6 (controller, on the device): run the diagnostics and record the facts**

The controller runs this step itself: it needs the board and the user's eyes. Never guess a result that needs eyes or hands on the board. Ask with AskUserQuestion, and put the full instructions inside the question text.

1. Flash, then start the stamped logger in the background for 15 minutes:
   ```bash
   tools/flash_and_log.sh 5 -B build-powerdiag -D SDKCONFIG=build-powerdiag/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.powerdiag" > build-logs/m7-powerdiag-flash.log
   python tools/serial_log.py /dev/cu.usbmodem101 900 -t > build-logs/m7-powerdiag.log
   ```
2. Ask the user to follow the screen. The questions carry the full text:
   - Round 0: press PWR 3 times a few seconds apart, and touch the screen a few times within 40 s;
   - Round 1: don't touch anything; the screen goes dark for about 3 minutes and comes back by itself;
   - Round 2: wait 20 s after it goes dark, then touch the screen once;
   - Round 3: the same, but press BOOT;
   - Round 4 appears only if a PWR pin was found: press PWR.

   Ask them to report:
   - whether each wake happened within about a second of their press, or only by itself after 5 minutes;
   - whether the panel was fully dark while asleep;
   - whether the speaker hissed or clicked while asleep;
   - the battery state (plugged into the Mac by USB, battery connected).
3. From the log, record:
   - the gauge: was it on at start (`gauge on at start`)? The `percent`, `vbat`, `vbus`, `battery` and `charger` values;
   - the touch INT edges during round 0;
   - `pins that followed PWR`;
   - each `woke:` line (cause and pins);
   - the drift: for round 1, the host time of the `sleeping` line against the host time of an `alive, uptime` line minus its uptime. Add about 0.3 s for the boot before `app_main`, then compare with 180 s.

   The chip's own `slept X s by the RTC` counts on the same RTC clock as the timer, so it always reads about 180 s. The host timestamps are the only real reference.
4. Write a "Power diagnostics (M7) — 2026-09-25" section in `docs/device-checks.md`: a results table, the drift estimate in seconds per hour, and what it means for the 10-minute lead. Commit: `docs: M7 power diagnostics`.
5. **Rulings before Task 2**, each recorded in the ledger:
   - The gauge. If it was off at start, Task 3's `power_init()` switching it on is required, and its first reading may be wrong for a while. Note that for Task 11.
   - The touch wake. If round 2 woke only by itself after 5 minutes, the touch cannot wake the chip. Stop and ask the user: the spec's deep sleep relies on a touch wake, and the alternatives (BOOT only, or no deep sleep) change what she experiences.
   - The PWR pin. If round 4 woke on PWR, give Task 3's `WAKE_PWR_GPIO` that pin, and remove the spec §3.13 hedge. Otherwise keep `GPIO_NUM_NC`, and the spec's "PWR does not wake it" stands.
   - The drift. If it is over about 30 s per hour (5 minutes across a 10-hour night), the 10-minute lead is too short. Rule a longer `SCHEDULE_WAKE_LEAD_MIN` for Task 2 (and `SCHEDULE_SLEEP_GUARD_MIN` = lead + 5), and amend spec §3.13.

---

### Task 2: Power and sleep rules (pure logic)

**Files:**
- Create: `components/logic/include/power_rules.h`, `components/logic/power_rules.c`, `test/test_power_rules.c`
- Modify: `components/logic/include/schedule.h`, `components/logic/schedule.c`, `test/test_schedule.c`

**Interfaces:**
- Consumes: `schedule_minutes_until()`, `schedule_step()`, `SCHEDULE_SYNC_BEFORE_MIN` (`schedule.h`).
- Produces (used by Tasks 8 and 9):
  - `power_rules.h`: `POWER_LOW_PCT` 10, `POWER_LOWER_PCT` 5, `POWER_CRITICAL_PCT` 2, `battery_rules_t`, `battery_verdict_t {BATTERY_OK, BATTERY_CRITICAL}`, `woke_t {WOKE_COLD, WOKE_TOUCH, WOKE_TIMER}`, `wake_plan_t {WAKE_BOOT, WAKE_LOCK_LIT, WAKE_LOCK_DARK, WAKE_SUNRISE, WAKE_HUNGRY}`, and:
    - `void battery_rules_init(battery_rules_t *r)`
    - `battery_verdict_t battery_rules_update(battery_rules_t *r, int pct, bool usb)`
    - `bool battery_rules_take_pending(battery_rules_t *r)`
    - `bool battery_critical(int pct, bool usb)`
    - `wake_plan_t power_wake_plan(woke_t woke, bool night, bool battery_sleep, bool critical)`
  - `schedule.h`: `SCHEDULE_WAKE_LEAD_MIN` (= `SCHEDULE_SYNC_BEFORE_MIN`, 10), `SCHEDULE_SLEEP_GUARD_MIN` 15, and:
    - `bool schedule_sleep_allowed(bool valid, uint16_t now, uint16_t bed, uint16_t wake)`
    - `int32_t schedule_wake_timer_min(bool valid, uint16_t bed, uint16_t wake)`

If the Task 1 ruling changed the lead, use the ruled values for `SCHEDULE_WAKE_LEAD_MIN` and `SCHEDULE_SLEEP_GUARD_MIN`, and adjust the tests below to match. The ruling is in the ledger, and the dispatch carries it.

- [ ] **Step 1: Write the failing tests for the power rules**

`test/test_power_rules.c`:

```c
#include "power_rules.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static battery_rules_t fresh(void)
{
    battery_rules_t r;
    battery_rules_init(&r);
    return r;
}

static void test_no_screen_above_ten_percent(void)
{
    battery_rules_t r = fresh();
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 50, false));
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 11, false));
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
}

static void test_ten_percent_shows_once(void)
{
    battery_rules_t r = fresh();
    battery_rules_update(&r, 10, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r)); /* taken */
    battery_rules_update(&r, 9, false);
    battery_rules_update(&r, 10, false); /* gauge jitter back up and down */
    battery_rules_update(&r, 8, false);
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
}

static void test_five_percent_shows_once_more(void)
{
    battery_rules_t r = fresh();
    battery_rules_update(&r, 10, false);
    battery_rules_take_pending(&r);
    battery_rules_update(&r, 5, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
    battery_rules_update(&r, 4, false);
    battery_rules_update(&r, 3, false);
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
}

static void test_a_jump_past_both_levels_shows_one_screen(void)
{
    battery_rules_t r = fresh();
    battery_rules_update(&r, 50, false);
    battery_rules_update(&r, 4, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
    battery_rules_update(&r, 4, false);
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
}

static void test_a_pending_screen_waits_until_taken(void)
{
    battery_rules_t r = fresh();
    battery_rules_update(&r, 9, false);
    battery_rules_update(&r, 9, false);
    battery_rules_update(&r, 8, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
}

static void test_usb_power_arms_both_levels_again_and_drops_a_pending_screen(void)
{
    battery_rules_t r = fresh();
    battery_rules_update(&r, 4, false); /* pending, both levels used */
    battery_rules_update(&r, 4, true);  /* plugged in: charging, no hungry cat */
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
    battery_rules_update(&r, 12, false); /* unplugged */
    battery_rules_update(&r, 10, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
    battery_rules_update(&r, 5, false);
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
}

static void test_critical_only_on_battery_at_two_percent_or_less(void)
{
    battery_rules_t r = fresh();
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 3, false));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, battery_rules_update(&r, 2, false));
    TEST_ASSERT_EQUAL(BATTERY_CRITICAL, battery_rules_update(&r, 0, false));
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 0, true));
    TEST_ASSERT_TRUE(battery_critical(2, false));
    TEST_ASSERT_FALSE(battery_critical(3, false));
    TEST_ASSERT_FALSE(battery_critical(1, true));
}

static void test_an_unavailable_or_wild_gauge_never_shows_or_sleeps(void)
{
    battery_rules_t r = fresh();
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, -1, false));
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 101, false));
    TEST_ASSERT_EQUAL(BATTERY_OK, battery_rules_update(&r, 255, false));
    TEST_ASSERT_FALSE(battery_rules_take_pending(&r));
    TEST_ASSERT_FALSE(battery_critical(-1, false));
    TEST_ASSERT_FALSE(battery_critical(200, false));
    battery_rules_update(&r, 9, false); /* the gauge comes back: the levels are still armed */
    TEST_ASSERT_TRUE(battery_rules_take_pending(&r));
}

static void test_wake_plan_after_a_cold_start_is_the_boot_animation(void)
{
    TEST_ASSERT_EQUAL(WAKE_BOOT, power_wake_plan(WOKE_COLD, false, false, false));
    TEST_ASSERT_EQUAL(WAKE_BOOT, power_wake_plan(WOKE_COLD, true, false, false)); /* then the lock, as today */
}

static void test_wake_plan_after_a_touch(void)
{
    TEST_ASSERT_EQUAL(WAKE_LOCK_LIT, power_wake_plan(WOKE_TOUCH, true, false, false));
    TEST_ASSERT_EQUAL(WAKE_BOOT, power_wake_plan(WOKE_TOUCH, false, false, false));
}

static void test_wake_plan_after_the_timer(void)
{
    TEST_ASSERT_EQUAL(WAKE_LOCK_DARK, power_wake_plan(WOKE_TIMER, true, false, false));
    TEST_ASSERT_EQUAL(WAKE_SUNRISE, power_wake_plan(WOKE_TIMER, false, false, false)); /* woke after wake time */
}

static void test_wake_plan_while_the_battery_is_still_empty(void)
{
    TEST_ASSERT_EQUAL(WAKE_HUNGRY, power_wake_plan(WOKE_TOUCH, false, true, true));
    TEST_ASSERT_EQUAL(WAKE_HUNGRY, power_wake_plan(WOKE_TOUCH, true, true, true));
    TEST_ASSERT_EQUAL(WAKE_BOOT, power_wake_plan(WOKE_TOUCH, false, true, false)); /* charged, or on USB now */
    TEST_ASSERT_EQUAL(WAKE_LOCK_LIT, power_wake_plan(WOKE_TOUCH, true, true, false));
    TEST_ASSERT_EQUAL(WAKE_BOOT, power_wake_plan(WOKE_TOUCH, false, false, true)); /* not a battery sleep */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_no_screen_above_ten_percent);
    RUN_TEST(test_ten_percent_shows_once);
    RUN_TEST(test_five_percent_shows_once_more);
    RUN_TEST(test_a_jump_past_both_levels_shows_one_screen);
    RUN_TEST(test_a_pending_screen_waits_until_taken);
    RUN_TEST(test_usb_power_arms_both_levels_again_and_drops_a_pending_screen);
    RUN_TEST(test_critical_only_on_battery_at_two_percent_or_less);
    RUN_TEST(test_an_unavailable_or_wild_gauge_never_shows_or_sleeps);
    RUN_TEST(test_wake_plan_after_a_cold_start_is_the_boot_animation);
    RUN_TEST(test_wake_plan_after_a_touch);
    RUN_TEST(test_wake_plan_after_the_timer);
    RUN_TEST(test_wake_plan_while_the_battery_is_still_empty);
    return UNITY_END();
}
```

- [ ] **Step 2: Write the failing schedule tests**

Add to `test/test_schedule.c`, before `main()`:

```c
static void test_sleep_allowed_except_in_the_last_quarter_hour_before_wake(void)
{
    const uint16_t bed = HM(19, 30), wake = HM(7, 0);
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(15, 0), bed, wake));
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(22, 0), bed, wake));
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(6, 44), bed, wake)); /* 16 minutes before */
    TEST_ASSERT_FALSE(schedule_sleep_allowed(true, HM(6, 45), bed, wake)); /* 15 minutes before */
    TEST_ASSERT_FALSE(schedule_sleep_allowed(true, HM(6, 59), bed, wake));
    TEST_ASSERT_FALSE(schedule_sleep_allowed(true, HM(7, 0), bed, wake)); /* the wake minute: the sunrise */
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(7, 1), bed, wake));
}

static void test_sleep_allowed_any_time_without_a_night_or_a_valid_clock(void)
{
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(6, 55), HM(7, 0), HM(7, 0))); /* bed == wake */
    TEST_ASSERT_TRUE(schedule_sleep_allowed(false, HM(6, 55), HM(19, 30), HM(7, 0)));
}

static void test_sleep_guard_wraps_past_midnight(void)
{
    const uint16_t bed = HM(22, 0), wake = HM(0, 5);
    TEST_ASSERT_FALSE(schedule_sleep_allowed(true, HM(23, 55), bed, wake));
    TEST_ASSERT_TRUE(schedule_sleep_allowed(true, HM(23, 49), bed, wake));
}

static void test_wake_timer_minute_is_ten_minutes_before_wake(void)
{
    TEST_ASSERT_EQUAL_INT32(HM(6, 50), schedule_wake_timer_min(true, HM(19, 30), HM(7, 0)));
    TEST_ASSERT_EQUAL_INT32(HM(23, 55), schedule_wake_timer_min(true, HM(22, 0), HM(0, 5)));
}

static void test_no_wake_timer_without_a_night_or_a_valid_clock(void)
{
    TEST_ASSERT_EQUAL_INT32(-1, schedule_wake_timer_min(false, HM(19, 30), HM(7, 0)));
    TEST_ASSERT_EQUAL_INT32(-1, schedule_wake_timer_min(true, HM(7, 0), HM(7, 0)));
}
```

and register them in `main()` after `RUN_TEST(test_sync_due_wraps_past_midnight);`:

```c
    RUN_TEST(test_sleep_allowed_except_in_the_last_quarter_hour_before_wake);
    RUN_TEST(test_sleep_allowed_any_time_without_a_night_or_a_valid_clock);
    RUN_TEST(test_sleep_guard_wraps_past_midnight);
    RUN_TEST(test_wake_timer_minute_is_ten_minutes_before_wake);
    RUN_TEST(test_no_wake_timer_without_a_night_or_a_valid_clock);
```

- [ ] **Step 3: Run the tests and see them fail**

Run: `sh test/run.sh 2>&1 | tail -5`
Expected: the build fails (`power_rules.h` not found, and `schedule_sleep_allowed` undeclared).

- [ ] **Step 4: Implement `power_rules`**

`components/logic/include/power_rules.h`:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* The battery screens and what a restart shows (spec §3.7, §3.13). Pure: no ESP-IDF. */
#define POWER_LOW_PCT      10 /* the hungry cat shows once at 10 % ... */
#define POWER_LOWER_PCT    5  /* ... and once more at 5 % */
#define POWER_CRITICAL_PCT 2  /* on battery, at or below this the toy deep-sleeps until it is charged */

typedef struct {
    bool armed_low, armed_lower; /* the 10 % and 5 % screens have not shown since USB power was connected */
    bool pending;                /* a low-battery screen is due and has not shown yet */
} battery_rules_t;

typedef enum { BATTERY_OK, BATTERY_CRITICAL } battery_verdict_t;

typedef enum {
    WOKE_COLD,  /* power-on or a reset */
    WOKE_TOUCH, /* from deep sleep by a touch or BOOT (or PWR, where wired) */
    WOKE_TIMER, /* from deep sleep by the timer before wake time */
} woke_t;

typedef enum {
    WAKE_BOOT,      /* the boot animation, then the menu, the lock or the first-run picker */
    WAKE_LOCK_LIT,  /* the lock at once, lit as after a touch */
    WAKE_LOCK_DARK, /* the lock at once with its panel off: the sunrise follows at wake time */
    WAKE_SUNRISE,   /* the sunrise at once: the timer fired after wake time */
    WAKE_HUNGRY,    /* the battery is still empty: the hungry cat for 4 s, then deep sleep again */
} wake_plan_t;

void battery_rules_init(battery_rules_t *r); /* both levels armed, nothing pending */
/* One reading: pct 0-100, or anything else when the gauge is unavailable, which never shows a screen or
 * sleeps (spec §4.7). Crossing 10 % or 5 % on battery makes one screen pending (one only, even when a jump
 * crosses both). USB power arms both levels again and drops a pending screen. */
battery_verdict_t battery_rules_update(battery_rules_t *r, int pct, bool usb);
/* True once per pending screen, which the caller then shows. Call only when it can show. */
bool battery_rules_take_pending(battery_rules_t *r);
/* On battery at POWER_CRITICAL_PCT or less, with a valid reading. */
bool battery_critical(int pct, bool usb);
/* battery_sleep: the toy went to sleep for the battery; critical: battery_critical() now. */
wake_plan_t power_wake_plan(woke_t woke, bool night, bool battery_sleep, bool critical);
```

`components/logic/power_rules.c`:

```c
#include "power_rules.h"

static bool valid(int pct) { return pct >= 0 && pct <= 100; }

void battery_rules_init(battery_rules_t *r)
{
    r->armed_low = true;
    r->armed_lower = true;
    r->pending = false;
}

battery_verdict_t battery_rules_update(battery_rules_t *r, int pct, bool usb)
{
    if (!valid(pct)) {
        return BATTERY_OK;
    }
    if (usb) {
        r->armed_low = true;
        r->armed_lower = true;
        r->pending = false;
        return BATTERY_OK;
    }
    if (pct <= POWER_LOWER_PCT && r->armed_lower) {
        r->armed_lower = false;
        r->armed_low = false; /* a jump past both levels shows one screen */
        r->pending = true;
    } else if (pct <= POWER_LOW_PCT && r->armed_low) {
        r->armed_low = false;
        r->pending = true;
    }
    return battery_critical(pct, usb) ? BATTERY_CRITICAL : BATTERY_OK;
}

bool battery_rules_take_pending(battery_rules_t *r)
{
    const bool p = r->pending;
    r->pending = false;
    return p;
}

bool battery_critical(int pct, bool usb) { return !usb && valid(pct) && pct <= POWER_CRITICAL_PCT; }

wake_plan_t power_wake_plan(woke_t woke, bool night, bool battery_sleep, bool critical)
{
    if (woke != WOKE_COLD && battery_sleep && critical) {
        return WAKE_HUNGRY;
    }
    switch (woke) {
    case WOKE_TIMER: return night ? WAKE_LOCK_DARK : WAKE_SUNRISE;
    case WOKE_TOUCH: return night ? WAKE_LOCK_LIT : WAKE_BOOT;
    default: return WAKE_BOOT;
    }
}
```

- [ ] **Step 5: Implement the schedule additions**

In `components/logic/include/schedule.h`, after the `SCHEDULE_SYNC_GAP_S` line:

```c
#define SCHEDULE_WAKE_LEAD_MIN   SCHEDULE_SYNC_BEFORE_MIN /* the deep-sleep timer wakes the toy this long before
                                                           * wake time, the minute the daily sync is due */
#define SCHEDULE_SLEEP_GUARD_MIN 15 /* no deep sleep in this many minutes before wake time (spec §3.13) */
```

and at the end:

```c
/* Deep sleep may start now: never from SCHEDULE_SLEEP_GUARD_MIN minutes before wake time up to and including
 * the wake minute, while the clock is valid and there is a night (spec §3.13). */
bool schedule_sleep_allowed(bool valid, uint16_t now, uint16_t bed, uint16_t wake);
/* The local minute the deep-sleep timer wakes the toy (SCHEDULE_WAKE_LEAD_MIN before wake time), or -1 for no
 * timer: the clock is not valid, or bed == wake (no night). */
int32_t schedule_wake_timer_min(bool valid, uint16_t bed, uint16_t wake);
```

In `components/logic/schedule.c`, at the end:

```c
bool schedule_sleep_allowed(bool valid, uint16_t now, uint16_t bed, uint16_t wake)
{
    if (!valid || bed == wake) {
        return true;
    }
    return schedule_minutes_until(now, wake) > SCHEDULE_SLEEP_GUARD_MIN;
}

int32_t schedule_wake_timer_min(bool valid, uint16_t bed, uint16_t wake)
{
    if (!valid || bed == wake) {
        return -1;
    }
    return schedule_step(wake, -SCHEDULE_WAKE_LEAD_MIN);
}
```

- [ ] **Step 6: Run the tests and see them pass**

Run: `sh test/run.sh 2>&1 | grep -E "tests passed|FAIL"`
Expected: `100% tests passed out of 14` (the new `test_power_rules` suite makes 14).

- [ ] **Step 7: Commit**

```bash
git add components/logic/include/power_rules.h components/logic/power_rules.c test/test_power_rules.c \
        components/logic/include/schedule.h components/logic/schedule.c test/test_schedule.c
git commit -m "feat: battery screen, critical and wake-plan rules, and when deep sleep may start"
```

---

### Task 3: Platform: the gauge, deep sleep, and the first-run flag

**Files:**
- Create: `components/platform/deepsleep.c`, `components/platform/include/deepsleep.h`, `sdkconfig.power`, `sdkconfig.drain`
- Modify:
  - `components/platform/power.c`, `components/platform/include/power.h`;
  - `components/platform/sensors.c`, `components/platform/include/sensors.h`;
  - `components/platform/settings.c`, `components/platform/include/settings.h`;
  - `components/platform/clock.c`, `components/platform/include/clock.h`;
  - `components/platform/buttons.c`, `components/platform/include/buttons.h` (only if Task 1 found a PWR pin);
  - `components/ui/app_manager.c` (one line), `main/Kconfig.projbuild`, `main/app_main.c`, `README.md`

**Interfaces:**
- Consumes: `pmu_read/pmu_write`, `display_sleep()` (Task 1), `audio_stop_all()`.
- Produces (used by Tasks 6-9):
  - `power.h`: `power_state_t {int8_t pct; bool usb; bool charging;}`, `void power_poll(bool with_gauge)`, `power_state_t power_state(void)`, and `power_init()` and `power_on_usb()` as before;
  - `sensors.h`: `SENSOR_EV_USB_IN`;
  - `deepsleep.h`: `deepsleep_cause_t {DEEPSLEEP_COLD, DEEPSLEEP_BY_PIN, DEEPSLEEP_BY_TIMER}`, and:
    - `void deepsleep_init(void)`
    - `deepsleep_cause_t deepsleep_cause(void)`
    - `bool deepsleep_was_for_battery(void)`
    - `void deepsleep_start(uint32_t timer_s, bool for_battery)`
  - `settings.h`: `bool settings_first_run_done(void)`, `void settings_set_first_run_done(void)`;
  - `clock.h`: `uint32_t clock_secs_until(uint16_t min)`, `bool clock_sync_running(void)`;
  - Kconfig: `CONFIG_CAT_DEV_IGNORE_USB`, `CONFIG_CAT_DEV_BATTERY_DRAIN`, `CONFIG_CAT_DEV_FAST_SLEEP`.

- [ ] **Step 1: The dev options and their variants**

Add to `main/Kconfig.projbuild`, after `CAT_DEV_POWER_DIAG`:

```
    config CAT_DEV_IGNORE_USB
        bool "Treat USB power as battery (deep sleep and the battery screens on the Mac's cable)"
        default n
    config CAT_DEV_BATTERY_DRAIN
        bool "Fake a draining battery: 12 % at boot, one point less every 10 s; 1 % after a battery sleep"
        default n
    config CAT_DEV_FAST_SLEEP
        bool "Deep-sleep 20 s after the panel goes off instead of 10 minutes"
        default n
```

Create `sdkconfig.power`:

```
CONFIG_CAT_DEV_IGNORE_USB=y
CONFIG_CAT_DEV_FAST_SLEEP=y
```

Create `sdkconfig.drain`:

```
CONFIG_CAT_DEV_IGNORE_USB=y
CONFIG_CAT_DEV_BATTERY_DRAIN=y
CONFIG_CAT_DEV_FAST_SLEEP=y
```

In `README.md`, add two rows to the "Developer builds" table, after "Soak test":

```
| Deep sleep on the USB cable (USB counts as battery; sleep 20 s after the panel goes off) | same with `power` |
| Battery screens (as `power`, plus a fake battery: 12 % at boot, one point less every 10 s, 1 % after a battery sleep; a reset starts over) | same with `drain` |
```

- [ ] **Step 2: The gauge and the charger in `power.c`**

Replace `components/platform/include/power.h` with:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Power state (spec §3.12, §3.13): USB power, the battery gauge and the charger. */
typedef struct {
    int8_t pct;    /* battery 0-100 %, or -1 when the gauge is unavailable (spec §4.7) */
    bool usb;      /* USB power: the AXP2101 "VBUS good" bit */
    bool charging; /* the charger reports current into the battery */
} power_state_t;

/* Call once, after display_init() (the I2C bus is up): turns the fuel gauge on, takes a first reading. */
void power_init(void);
/* The "VBUS good" bit read now (one I2C read, any task); false when unknown. */
bool power_on_usb(void);
/* Reads USB and the charger, and the gauge when with_gauge. The sensors task calls it every 500 ms, with the
 * gauge every 5 s (spec §4.5). The result is kept for power_state(). */
void power_poll(bool with_gauge);
/* The latest power_poll() result; any task. */
power_state_t power_state(void);
```

Replace `components/platform/power.c` with:

```c
#include "power.h"
#include "deepsleep.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "pmu.h"
#include "sdkconfig.h"

#define PMU_REG_STATUS1   0x00
#define PMU_REG_STATUS2   0x01
#define PMU_REG_GAUGE_CTL 0x18
#define PMU_REG_PERCENT   0xA4
#define PMU_VBUS_GOOD     (1u << 5) /* status 1 (XPowersLib isVbusGood) */
#define PMU_BATTERY_IN    (1u << 3) /* status 1 (isBatteryConnect) */
#define PMU_GAUGE_ON      (1u << 3) /* gauge control (enableGauge) */
#define PMU_DIR_CHARGING  1u        /* status 2, bits 6:5: current flows into the battery (isCharging) */
#define DRAIN_START_PCT   12        /* CONFIG_CAT_DEV_BATTERY_DRAIN */
#define DRAIN_STEP_S      10

static const char *TAG = "power";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static power_state_t s_state = {.pct = -1}; /* under s_lock */

void power_init(void)
{
    if (!pmu_init()) {
        return;
    }
    uint8_t ctl = 0;
    if (pmu_read(PMU_REG_GAUGE_CTL, &ctl) == ESP_OK && !(ctl & PMU_GAUGE_ON)) {
        pmu_write(PMU_REG_GAUGE_CTL, ctl | PMU_GAUGE_ON);
        ESP_LOGI(TAG, "fuel gauge switched on");
    }
    power_poll(true);
    const power_state_t s = power_state();
    ESP_LOGI(TAG, "USB power: %s, battery %d %%, %s", s.usb ? "yes" : "no", s.pct,
             s.charging ? "charging" : "not charging");
}

bool power_on_usb(void)
{
#if CONFIG_CAT_DEV_IGNORE_USB
    return false;
#else
    uint8_t sts = 0;
    return pmu_read(PMU_REG_STATUS1, &sts) == ESP_OK && (sts & PMU_VBUS_GOOD) != 0;
#endif
}

void power_poll(bool with_gauge)
{
    uint8_t s1 = 0, s2 = 0, pct = 0xFF;
    const bool ok = pmu_read(PMU_REG_STATUS1, &s1) == ESP_OK && pmu_read(PMU_REG_STATUS2, &s2) == ESP_OK;
    const bool battery = ok && (s1 & PMU_BATTERY_IN);
    const bool gauge_read = with_gauge && battery && pmu_read(PMU_REG_PERCENT, &pct) == ESP_OK;
    portENTER_CRITICAL(&s_lock);
    s_state.usb = ok && (s1 & PMU_VBUS_GOOD);
    s_state.charging = ok && ((s2 >> 5) & 3u) == PMU_DIR_CHARGING;
    if (!battery) {
        s_state.pct = -1;
    } else if (gauge_read) {
        s_state.pct = pct <= 100 ? (int8_t)pct : -1;
    }
#if CONFIG_CAT_DEV_IGNORE_USB
    s_state.usb = false;
#endif
#if CONFIG_CAT_DEV_BATTERY_DRAIN
    /* Still empty after a sleep for the battery, so the hungry-cat wake can be checked; a reset starts over. */
    const int64_t steps = esp_timer_get_time() / 1000000 / DRAIN_STEP_S;
    s_state.pct = deepsleep_was_for_battery() ? 1
                  : (int8_t)(steps >= DRAIN_START_PCT ? 0 : DRAIN_START_PCT - steps);
    s_state.charging = false;
#endif
    portEXIT_CRITICAL(&s_lock);
}

power_state_t power_state(void)
{
    portENTER_CRITICAL(&s_lock);
    const power_state_t s = s_state;
    portEXIT_CRITICAL(&s_lock);
    return s;
}
```

- [ ] **Step 3: The sensors task polls power and reports USB being plugged in**

In `components/platform/include/sensors.h`, add to the enum after `SENSOR_EV_SHAKE`:

```c
    SENSOR_EV_USB_IN, /* USB power connected (spec §3.13: the charging screen); not at boot */
```

In `components/platform/sensors.c`:
- add `#include "power.h"`;
- add after `#define EVENT_QUEUE_LEN 8`:

```c
#define POWER_EVERY 25  /* loops: 500 ms, USB and the charger */
#define GAUGE_EVERY 250 /* loops: 5 s, the fuel gauge (spec §4.5) */
```

- add the file-level state `static bool s_usb; /* sensors task only: USB at the last poll */`;
- in `sensors_task`, declare `uint32_t loops = 0;` before `for (;;)`, and after `sample_imu();` add:

```c
        if (loops % POWER_EVERY == 0) {
            power_poll(loops % GAUGE_EVERY == 0);
            const bool usb = power_state().usb;
            if (usb && !s_usb) {
                post(SENSOR_EV_USB_IN);
            }
            s_usb = usb;
        }
        loops++;
```

- in `sensors_start()`, before creating the task: `s_usb = power_state().usb; /* power_init() ran: USB at boot is no event */`.

In `components/ui/app_manager.c` `poll_sensors()`, right after the `SENSOR_EV_SHAKE` block's closing brace, add:

```c
        if (ev == SENSOR_EV_USB_IN) {
            continue; /* nothing shows for it yet */
        }
```

The next line maps every other event to a button, so without this an unhandled USB event would act as PWR.

- [ ] **Step 4: The first-run flag**

`components/platform/include/settings.h`, at the end:

```c
bool settings_first_run_done(void); /* the cat picker ran once (spec §3.13); false while the key is missing */
void settings_set_first_run_done(void);
```

`components/platform/settings.c`:
- add `static uint8_t s_first_done;`;
- in `settings_init()`, after `nvs_get_u16(s_nvs, "wake", &s_wake);`, add `nvs_get_u8(s_nvs, "first", &s_first_done);`;
- change the first `ESP_LOGI` in `settings_init()` to also print `first run %s` with `s_first_done ? "done" : "pending"`;
- at the end:

```c
bool settings_first_run_done(void) { return s_first_done != 0; }
void settings_set_first_run_done(void)
{
    s_first_done = 1;
    commit(nvs_set_u8(s_nvs, "first", s_first_done), "first");
}
```

- [ ] **Step 5: `clock_secs_until()` and `clock_sync_running()`**

`components/platform/include/clock.h`, at the end:

```c
/* Seconds from now until the next local min:00 (DST-aware), 1 to 86400 plus an hour on a DST day. Only
 * meaningful while clock_valid(). */
uint32_t clock_secs_until(uint16_t min);
/* A background NTP sync is running (spec §3.13: no deep sleep during one). Any task. */
bool clock_sync_running(void);
```

`components/platform/clock.c`, at the end:

```c
static time_t local_at(struct tm tm, uint16_t min)
{
    tm.tm_hour = (min % 1440) / 60;
    tm.tm_min = min % 60;
    tm.tm_sec = 0;
    tm.tm_isdst = -1; /* mktime picks CET or CEST for that date */
    return mktime(&tm);
}

uint32_t clock_secs_until(uint16_t min)
{
    const time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    time_t at = local_at(tm, min);
    if (at <= now) {
        tm.tm_mday += 1; /* tomorrow; mktime normalizes the date */
        at = local_at(tm, min);
    }
    return (uint32_t)(at - now);
}

bool clock_sync_running(void)
{
    portENTER_CRITICAL(&s_mux);
    const bool running = s_running;
    portEXIT_CRITICAL(&s_mux);
    return running;
}
```

- [ ] **Step 6: `deepsleep.c`**

`components/platform/include/deepsleep.h`:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Deep sleep on battery (spec §3.7, §3.13). */
typedef enum {
    DEEPSLEEP_COLD,     /* power-on or a reset */
    DEEPSLEEP_BY_PIN,   /* a touch (GPIO11), BOOT (GPIO0), or PWR where wired */
    DEEPSLEEP_BY_TIMER, /* the timer set by deepsleep_start() */
} deepsleep_cause_t;

/* Call first in app_main: reads why the chip started, and releases the speaker amplifier's sleep hold. */
void deepsleep_init(void);
deepsleep_cause_t deepsleep_cause(void);
/* The deep sleep this start woke from was for the battery (deepsleep_start(.., true)). */
bool deepsleep_was_for_battery(void);
/* Stops all sound, puts the panel to sleep, holds the speaker amplifier off and deep-sleeps. Never returns:
 * the chip restarts on a touch, BOOT (PWR where wired), or after timer_s seconds when timer_s > 0. LVGL task,
 * with the LVGL lock held. */
void deepsleep_start(uint32_t timer_s, bool for_battery);
```

`components/platform/deepsleep.c`:

```c
#include "deepsleep.h"
#include "audio.h"
#include "bsp/esp-bsp.h"
#include "display.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define WAKE_BOOT_GPIO  GPIO_NUM_0
#define WAKE_TOUCH_GPIO BSP_LCD_TOUCH_INT /* GPIO11: low while the CST9217 reports a touch */
#define WAKE_PWR_GPIO   GPIO_NUM_NC       /* the AXP2101 interrupt, if the M7 power diagnostics found a pin */

static const char *TAG = "deepsleep";
RTC_DATA_ATTR static bool s_for_battery; /* survives deep sleep */
static deepsleep_cause_t s_cause;

void deepsleep_init(void)
{
    switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT1: s_cause = DEEPSLEEP_BY_PIN; break;
    case ESP_SLEEP_WAKEUP_TIMER: s_cause = DEEPSLEEP_BY_TIMER; break;
    default: s_cause = DEEPSLEEP_COLD; break;
    }
    if (s_cause == DEEPSLEEP_COLD) {
        s_for_battery = false;
    }
    gpio_hold_dis(BSP_POWER_AMP_IO); /* the amplifier pin must follow the codec driver again */
    gpio_deep_sleep_hold_dis();
    static const char *const NAMES[] = {"cold start", "woke by a pin", "woke by the timer"};
    ESP_LOGI(TAG, "%s%s", NAMES[s_cause], deepsleep_was_for_battery() ? " (slept for the battery)" : "");
}

deepsleep_cause_t deepsleep_cause(void) { return s_cause; }

bool deepsleep_was_for_battery(void) { return s_cause != DEEPSLEEP_COLD && s_for_battery; }

static void wake_pin(gpio_num_t pin, uint64_t *mask)
{
    if (pin != GPIO_NUM_NC) {
        *mask |= BIT64(pin);
    }
}

void deepsleep_start(uint32_t timer_s, bool for_battery)
{
    ESP_LOGI(TAG, "deep sleep: timer %lu s%s", (unsigned long)timer_s, for_battery ? ", for the battery" : "");
    s_for_battery = for_battery;
    audio_stop_all();
    display_sleep();
    gpio_set_level(BSP_POWER_AMP_IO, 0); /* the speaker amplifier stays off while the chip sleeps */
    gpio_hold_en(BSP_POWER_AMP_IO);
    gpio_deep_sleep_hold_en();
    uint64_t mask = 0;
    wake_pin(WAKE_BOOT_GPIO, &mask);
    wake_pin(WAKE_TOUCH_GPIO, &mask);
    wake_pin(WAKE_PWR_GPIO, &mask);
    esp_sleep_enable_ext1_wakeup_io(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON); /* keeps the pull-ups on in sleep */
    for (int pin = 0; pin < GPIO_NUM_MAX; pin++) {
        if (mask & BIT64(pin)) {
            rtc_gpio_pullup_en((gpio_num_t)pin);
            rtc_gpio_pulldown_dis((gpio_num_t)pin);
        }
    }
    if (timer_s > 0) {
        esp_sleep_enable_timer_wakeup((uint64_t)timer_s * 1000000);
    }
    vTaskDelay(pdMS_TO_TICKS(100)); /* the log line and the panel's sleep command drain */
    esp_deep_sleep_start();
}
```

Task 1's ruling on the PWR pin applies here:
- **If Task 1 found a pin,** set `WAKE_PWR_GPIO` to it. A PWR press latched but not yet cleared would hold that line low and wake the chip at once, so clear the latch first:
  - add to `buttons.h`: `void buttons_clear_power(void); /* clears a latched PWR press (before deep sleep) */`;
  - in `buttons.c`: `void buttons_clear_power(void) { pmu_write(PMU_REG_INTSTS2, PMU_PKEY_SHORT); }`;
  - call it in `deepsleep_start()` right before `esp_deep_sleep_start()` (include `buttons.h`).
- **Otherwise** leave `GPIO_NUM_NC` and skip this.

`components/platform/CMakeLists.txt`: `driver/rtc_io.h` and `driver/gpio.h` come from `esp_driver_gpio`, already in `PRIV_REQUIRES`, and `esp_sleep.h` from `esp_hw_support`, which every component gets. Add nothing unless the build says otherwise.

- [ ] **Step 7: `app_main` calls `deepsleep_init()` first**

In `main/app_main.c`:
- add `#include "deepsleep.h"`;
- add `deepsleep_init();` as the first line of `app_main()`, before the reset-reason log. It must run before `power_init()`: the drain build reads `deepsleep_was_for_battery()`.

`power_init()` already runs in the normal branch before `app_manager_init()`, and now takes the first gauge reading.

- [ ] **Step 8: Build every variant**

A new `.c` file needs a reconfigure, and a reconfigure must carry the same `-D` arguments as the build. A bare `idf.py -B build-X reconfigure` would configure against the top-level `sdkconfig`.

```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py -B build-m7 -D SDKCONFIG=build-m7/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" reconfigure build 2>&1 | tail -1
for v in power drain soak powerdiag; do
  idf.py -B build-$v -D SDKCONFIG=build-$v/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.$v" reconfigure build 2>&1 | tail -1
done
idf.py reconfigure build 2>&1 | tail -1
sh test/run.sh 2>&1 | grep "tests passed"
```

Expected: every build ends with `Project build complete`, with no `warning:` in `components/` or `main/`. Host tests: `100% tests passed out of 14`.

- [ ] **Step 9: Commit**

```bash
git add main/Kconfig.projbuild main/app_main.c sdkconfig.power sdkconfig.drain README.md \
        components/platform/power.c components/platform/include/power.h \
        components/platform/sensors.c components/platform/include/sensors.h \
        components/platform/settings.c components/platform/include/settings.h \
        components/platform/clock.c components/platform/include/clock.h \
        components/platform/deepsleep.c components/platform/include/deepsleep.h components/ui/app_manager.c
git commit -m "feat: battery gauge and charger, deep sleep, first-run flag, and power dev builds"
```

(Add `components/platform/buttons.c components/platform/include/buttons.h` if you changed them.)

---

### Task 4: Images, sounds, releasing a variant, and live cats that follow the variant

**Files:**
- Modify: `tools/export_images/ui_images.mjs`, `tools/build_sounds/sounds.txt`, `components/assets/include/sounds.h`, `components/assets/sounds.c`, `components/assets/include/assets.h`, `components/assets/assets.c`, `components/cat/include/cat.h`, `components/cat/cat.c`
- Regenerated (commit if changed): `components/assets/assets_manifest.c`, `components/assets/include/assets_manifest.h`

**Interfaces:**
- Consumes: nothing new.
- Produces (used by Tasks 6-8):
  - images:
    - `ui/batt_low` (72 × 38, red, with the stub);
    - `ui/batt_charge` (72 × 38, green frame and nub only);
    - `ui/batt_bolt` (72 × 38 canvas, the bolt only);
    - `ui/bowl_empty` (160 × 88) and `ui/bowl_full` (160 × 100);
    - `ui/p_grid` and `ui/p_cat` (36 px);
    - `ui/p_eye_off` (34 px);
  - sounds: `SND_HUNGRY` ("Gladna sam…") and `SND_CHARGING` (munching + "Njam njam!");
  - `void assets_release_prefix(const char *prefix)`;
  - `const char *cat_variant_name(cat_variant_t v)`: "orange", "grey" or "tux", also valid as a `cat_create_colored()` palette;
  - `cat_set_variant()` now reloads every live cat that uses the chosen variant.

- [ ] **Step 1: The images (from `Screen.dc.html` lowbatt, charging and papps, and `ICON`)**

In `tools/export_images/ui_images.mjs`, add two paths to `ICON`:

```js
  grid: 'M4 4 H10 V10 H4 Z M14 4 H20 V10 H14 Z M4 14 H10 V20 H4 Z M14 14 H20 V20 H14 Z',
  cat: 'M5 20 L5 8.5 L7.5 3.5 L10.5 7 L13.5 7 L16.5 3.5 L19 8.5 L19 20 Z M9.5 12.5 L9.5 13.5 M14.5 12.5 L14.5 13.5',
```

Add below `lineIcon`:

```js
// Battery glyphs (Screen.dc.html lowbatt and charging views), 72 x 38. The charging fill is an LVGL object
// drawn between the frame and the bolt, so it can grow (the design's `charge` keyframes).
const BATT_FRAME = (c) =>
  `<rect x="4" y="4" width="56" height="30" rx="7" fill="none" stroke="${c}" stroke-width="5"/>` +
  `<rect x="62" y="13" width="7" height="12" rx="2" fill="${c}"/>`;
// The bowl's body, with its rim at y0 (empty: 30, full: 42).
const BOWL = (y0) =>
  `<path d="M8 ${y0} L152 ${y0} Q142 ${y0 + 54} 80 ${y0 + 54} Q18 ${y0 + 54} 8 ${y0} Z" fill="#5A6B8A" stroke="#25324A" stroke-width="6" stroke-linejoin="round"/>`;
const KIBBLE = [[44, 36], [66, 28], [90, 28], [114, 36], [78, 18]]
  .map(([x, y]) => `<circle cx="${x}" cy="${y}" r="11" fill="#D98A4A" stroke="#7A3A10" stroke-width="4"/>`)
  .join('');
```

Append to `UI_IMAGES`, after `lineIcon('ui/p_clock_row', ...)`:

```js
  { name: 'ui/batt_low', viewBox: [0, 0, 72, 38], crop: false,
    svg: BATT_FRAME('#FF4D4D') + '<rect x="11" y="11" width="10" height="16" rx="3" fill="#FF4D4D"/>' },
  { name: 'ui/batt_charge', viewBox: [0, 0, 72, 38], crop: false, svg: BATT_FRAME('#5CD67A') },
  {
    name: 'ui/batt_bolt',
    viewBox: [0, 0, 72, 38],
    crop: false,
    svg: '<path d="M36 6 L26 20 L34 20 L30 32 L42 16 L34 16 Z" fill="#FFF6E6" stroke="#1D5A2B" stroke-width="2" stroke-linejoin="round"/>',
  },
  {
    name: 'ui/bowl_empty',
    viewBox: [0, 0, 160, 88],
    crop: false,
    svg:
      BOWL(30) +
      '<ellipse cx="80" cy="30" rx="74" ry="12" fill="#7F8FB0" stroke="#25324A" stroke-width="5"/>' +
      '<ellipse cx="80" cy="31" rx="58" ry="6" fill="#2A3346"/>',
  },
  { name: 'ui/bowl_full', viewBox: [0, 0, 160, 100], crop: false, svg: KIBBLE + BOWL(42) },
  lineIcon('ui/p_grid', ICON.grid, 36, '#F2EFFA'),
  lineIcon('ui/p_cat', ICON.cat, 36, '#F2EFFA'),
  {
    name: 'ui/p_eye_off',
    viewBox: [0, 0, 34, 34],
    crop: false,
    svg:
      '<circle cx="17" cy="17" r="15" fill="#1E1D2A" stroke="#FF6B6B" stroke-width="3"/>' +
      '<path d="M9 17 Q17 9 25 17 Q17 25 9 17 Z M10 24 L24 10" fill="none" stroke="#FF6B6B" stroke-width="2.5" stroke-linecap="round"/>',
  },
```

- [ ] **Step 2: The sounds**

In `tools/build_sounds/sounds.txt`, add under `# Derived`, after `wake ...`:

```
charging      concat  munch+njam_njam
```

`gladna_sam` is already built (a `say` placeholder until the parent records it; `docs/recording-list.md` lists it).

In `components/assets/include/sounds.h`, add before `SND_COUNT`:

```c
    SND_HUNGRY,   /* "Gladna sam…": the low-battery screen (spec §3.13) */
    SND_CHARGING, /* munching and "Njam njam!": the charging screen */
```

In `components/assets/sounds.c`, add to `FILES`:

```c
    [SND_HUNGRY] = "gladna_sam",
    [SND_CHARGING] = "charging",
```

- [ ] **Step 3: Regenerate the assets and check them**

```bash
tools/build_assets.sh 2>&1 | tail -3
ls assets/img/ui/batt_low.bin assets/img/ui/batt_charge.bin assets/img/ui/batt_bolt.bin \
   assets/img/ui/bowl_empty.bin assets/img/ui/bowl_full.bin assets/img/ui/p_grid.bin \
   assets/img/ui/p_cat.bin assets/img/ui/p_eye_off.bin assets/snd/charging.wav assets/snd/gladna_sam.wav
git status --short components/assets/
```

Expected: every file listed. If the manifest shows as modified, commit it with this task. Look at the exporter's PNG previews in `build-assets/preview/ui/` (`batt_*.png`, `bowl_*.png`, `p_grid.png`, `p_cat.png`, `p_eye_off.png`) against the design's lowbatt, charging and papps views.

- [ ] **Step 4: `assets_release_prefix()`**

In `components/assets/include/assets.h`, after `assets_image()`:

```c
/* Frees every cached image whose name starts with prefix (e.g. "cat/grey/") and drops it from LVGL's image
 * cache. Only for images no LVGL object shows any more (spec §3.13: a cat choice releases the other variants).
 * LVGL lock held. */
void assets_release_prefix(const char *prefix);
```

In `components/assets/assets.c`, at the end:

```c
void assets_release_prefix(const char *prefix)
{
    const size_t n = strlen(prefix);
    size_t count = 0, bytes = 0;
    entry_t **link = &s_cache;
    while (*link) {
        entry_t *e = *link;
        if (strncmp(e->name, prefix, n) != 0) {
            link = &e->next;
            continue;
        }
        *link = e->next;
        lv_image_cache_drop(&e->dsc); /* never NULL here: NULL would drop LVGL's whole cache */
        if (e->buf) {
            bytes += e->dsc.data_size;
            heap_caps_free(e->buf);
        }
        heap_caps_free(e);
        count++;
    }
    if (count) {
        ESP_LOGI(TAG, "released %u images (%u KB) under %s", (unsigned)count, (unsigned)(bytes / 1024), prefix);
    }
}
```

- [ ] **Step 5: Live cats follow the variant**

In `components/cat/cat.c`:
- give the struct a tag and a link: change `typedef struct {` (the `cat_t` one) to `typedef struct cat_s {`, and add as its first field `struct cat_s *next; /* every live cat: cat_set_variant() reloads them */`;
- add `static cat_t *s_cats;` after `static cat_variant_t s_variant = CAT_VARIANT_ORANGE;`;
- in `cat_create_colored()`, right after `lv_obj_set_user_data(c->root, c);`: `c->next = s_cats; s_cats = c;`;
- in `on_delete()`, before `lv_timer_delete(c->timer);`:

```c
    for (cat_t **p = &s_cats; *p; p = &(*p)->next) {
        if (*p == c) {
            *p = c->next;
            break;
        }
    }
```

- replace `cat_set_variant()` with:

```c
void cat_set_variant(cat_variant_t variant)
{
    const cat_variant_t v = variant < CAT_VARIANT_COUNT ? variant : CAT_VARIANT_ORANGE;
    if (v == s_variant) {
        return;
    }
    s_variant = v;
    /* Every live cat of the chosen variant shows the new one at once: hide all its layers and forget what they
     * showed, so apply() loads each visible part again under the new variant's name (a layer left visible with
     * `shown` cleared would never hide again). */
    for (cat_t *c = s_cats; c; c = c->next) {
        if (c->palette) {
            continue;
        }
        for (int l = 0; l < L_COUNT; l++) {
            lv_obj_add_flag(c->img[l], LV_OBJ_FLAG_HIDDEN);
            c->shown[l] = NULL;
        }
        apply(c);
    }
}

const char *cat_variant_name(cat_variant_t v) { return VARIANT_NAME[v < CAT_VARIANT_COUNT ? v : CAT_VARIANT_ORANGE]; }
```

In `components/cat/include/cat.h`:
- after `cat_set_variant`, add:

```c
/* "orange", "grey" or "tux": the variant's asset folder, also a valid cat_create_colored() palette. */
const char *cat_variant_name(cat_variant_t v);
```

- change the `cat_create_colored` comment to:

```c
/* A cat in another exported palette instead of the chosen variant: a variant name (cat_variant_name(), the
 * cat picker), or "piano_c", "piano_d", "piano_e", "piano_g" or "piano_a" (Meow piano, spec §3.11, which
 * exist at CAT_SCALE_S only). palette must outlive the cat (use a string literal or cat_variant_name()). Such a
 * cat keeps its palette when the chosen variant changes. */
```

- the comment above `cat_set_variant`: `/* The chosen variant (spec §3.13); every live cat without a palette switches to it at once. */`

- [ ] **Step 6: Build and test**

```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py -B build-m7 -D SDKCONFIG=build-m7/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" build 2>&1 | tail -1
idf.py -B build-gallery -D SDKCONFIG=build-gallery/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.gallery;sdkconfig.perf" build 2>&1 | tail -1
sh test/run.sh 2>&1 | grep "tests passed"
```

Expected: both builds complete with no new warnings (the gallery build exercises `cat_set_variant()`, which it calls per variant). Host tests `100% tests passed out of 14`.

- [ ] **Step 7: Commit**

```bash
git add tools/export_images/ui_images.mjs tools/build_sounds/sounds.txt components/assets components/cat
git commit -m "feat: battery, bowl and parent images, the charging sound, variant release, and live cats that follow the variant"
```

---

### Task 5: Fredoka for the parent area

The user approved exactly two downloads, 2026-09-25:
- the Fredoka font file and its `OFL.txt` from Google Fonts' GitHub repository (`github.com/google/fonts`, folder `ofl/fredoka`);
- the `lv_font_conv` converter from npm, as a dev tool under `tools/fonts/`.

Download nothing else. That repository holds Fredoka only as a variable font. `lv_font_conv` renders its default instance (Regular) and cannot pick the design's 500/600 weights, and pinning a weight would need another tool (fontTools), which was not approved. So the parent area gets Fredoka Regular, and the spec says so (Step 6).

**Files:**
- Create:
  - `assets-src/fonts/Fredoka.ttf`, `assets-src/fonts/OFL.txt`;
  - `tools/fonts/package.json`, `tools/fonts/package-lock.json`, `tools/fonts/build_fonts.sh`;
  - `components/ui/fonts/font_fredoka_17.c`, `_20.c`, `_22.c`, `_46.c`, `_54.c` (generated, committed);
  - `components/ui/include/fonts.h`
- Modify: `components/ui/CMakeLists.txt`, `docs/superpowers/specs/2026-09-22-cat-toy-design.md` (§3.13 Fredoka bullet)

**Interfaces:**
- Consumes: nothing.
- Produces (used by Task 7): `fonts.h` declaring the `lv_font_t` values `font_fredoka_17`, `font_fredoka_20`, `font_fredoka_22`, `font_fredoka_46`, `font_fredoka_54`:
  - 17, 20 and 22 carry ASCII, Č Ć Đ Š Ž č ć đ š ž and "‹" (U+2039);
  - 46 and 54 carry only space, `%`, `+`, `-`, the digits and `:`, the values and the ± buttons.

- [ ] **Step 1: Download the font and its license (approved)**

```bash
mkdir -p assets-src/fonts
curl -fL -o assets-src/fonts/Fredoka.ttf "https://raw.githubusercontent.com/google/fonts/main/ofl/fredoka/Fredoka%5Bwdth%2Cwght%5D.ttf"
curl -fL -o assets-src/fonts/OFL.txt "https://raw.githubusercontent.com/google/fonts/main/ofl/fredoka/OFL.txt"
file assets-src/fonts/Fredoka.ttf
head -3 assets-src/fonts/OFL.txt
```

Expected: `TrueType Font data`, and the OFL header naming Fredoka's copyright holders. If the TTF URL returns 404, list the folder through the same repository and use the `.ttf` it names:

```bash
curl -fsS https://api.github.com/repos/google/fonts/contents/ofl/fredoka | grep '"name"'
```

- [ ] **Step 2: The converter (approved)**

`tools/fonts/package.json`:

```json
{
  "name": "cat-toy-fonts",
  "private": true,
  "description": "Converts Fredoka (assets-src/fonts, SIL OFL) into LVGL C fonts: build_fonts.sh",
  "devDependencies": {
    "lv_font_conv": "^1.5.3"
  }
}
```

```bash
(cd tools/fonts && npm install --silent && ls node_modules/.bin/lv_font_conv)
```

`node_modules/` is already git-ignored. Commit `package-lock.json`.

- [ ] **Step 3: `tools/fonts/build_fonts.sh`**

```sh
#!/bin/sh
# Converts Fredoka (assets-src/fonts, SIL OFL; see OFL.txt there) into the parent area's LVGL fonts
# (spec §3.13). The variable font gives its default instance, Regular. Output: components/ui/fonts/.
set -e
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
TTF="$ROOT/assets-src/fonts/Fredoka.ttf"
OUT="$ROOT/components/ui/fonts"
CONV="$ROOT/tools/fonts/node_modules/.bin/lv_font_conv"
[ -x "$CONV" ] || (cd "$ROOT/tools/fonts" && npm ci --silent)
# ASCII, the Croatian letters and the "‹" of "‹ Parents".
TEXT="0x20-0x7E,0x106-0x107,0x10C-0x10D,0x110-0x111,0x160-0x161,0x17D-0x17E,0x2039"
# The big values (volume %, HH:MM, --:--) and the ± buttons.
DIGITS="0x20,0x25,0x2B,0x2D,0x30-0x3A"
mkdir -p "$OUT"
conv() { # name size range
    "$CONV" --font "$TTF" --size "$2" --bpp 4 --format lvgl --no-compress --lv-include lvgl.h \
        --range "$3" -o "$OUT/$1.c"
    echo "  $1"
}
conv font_fredoka_17 17 "$TEXT"
conv font_fredoka_20 20 "$TEXT"
conv font_fredoka_22 22 "$TEXT"
conv font_fredoka_46 46 "$DIGITS"
conv font_fredoka_54 54 "$DIGITS"
```

```bash
chmod +x tools/fonts/build_fonts.sh
tools/fonts/build_fonts.sh
ls -la components/ui/fonts/
```

Expected: five `.c` files. `lv_font_conv` prints a warning for any glyph the font lacks. Record any warning (for example U+2039) in the report: Task 7 then falls back to `<`.

- [ ] **Step 4: The header and the build**

`components/ui/include/fonts.h`:

```c
#pragma once
#include "lvgl.h"

/* Fredoka Regular (SIL OFL: assets-src/fonts/OFL.txt), converted by tools/fonts/build_fonts.sh for the parent
 * area (spec §3.13). 17, 20 and 22 hold ASCII, the Croatian letters and "‹"; 46 and 54 hold only the digits,
 * space, %, +, - and :. */
LV_FONT_DECLARE(font_fredoka_17)
LV_FONT_DECLARE(font_fredoka_20)
LV_FONT_DECLARE(font_fredoka_22)
LV_FONT_DECLARE(font_fredoka_46)
LV_FONT_DECLARE(font_fredoka_54)
```

`components/ui/CMakeLists.txt`: change `SRC_DIRS "." "apps"` to `SRC_DIRS "." "apps" "fonts"`.

```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py -B build-m7 -D SDKCONFIG=build-m7/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" reconfigure build 2>&1 | grep -E "error|warning:.*fonts|Project build complete" | head
```

Expected: `Project build complete`. The fonts are not referenced yet, and the linker drops them. If a generated file fails to compile against LVGL 9.4, report the compiler error rather than editing the generated file by hand. The usual cause is an old `lv_font_conv`; 1.5.3 targets LVGL 9.

- [ ] **Step 5: Check the glyphs landed**

```bash
for f in components/ui/fonts/*.c; do printf "%s: " "$f"; grep -c "U+" "$f"; done
grep -c "U+2039\|U+010C\|U+017D" components/ui/fonts/font_fredoka_20.c
```

Expected:
- 17, 20 and 22: at least 105 glyphs each (95 ASCII + 10 Croatian + "‹");
- 46 and 54: 15 each;
- the second grep: 3 (fewer if the font lacks "‹"; see Step 3).

- [ ] **Step 6: Say so in the spec**

In `docs/superpowers/specs/2026-09-22-cat-toy-design.md` §3.13, replace the Fredoka bullet under "The parent area" with:

```markdown
  - The text is Fredoka (SIL OFL) at the design's sizes (17, 20, 22, 46 and 54 px), converted with
    `lv_font_conv` at 4 bpp, with ASCII, Č Ć Đ Š Ž č ć đ š ž and "‹". The repository gets Fredoka from
    Google Fonts as a variable font, whose default instance is Regular: the design's 500 and 600
    weights are not reproduced.
```

- [ ] **Step 7: Commit**

```bash
git add assets-src/fonts tools/fonts/package.json tools/fonts/package-lock.json tools/fonts/build_fonts.sh \
        components/ui/fonts components/ui/include/fonts.h components/ui/CMakeLists.txt \
        docs/superpowers/specs/2026-09-22-cat-toy-design.md
git commit -m "feat: Fredoka fonts for the parent area"
```

---

### Task 6: The cat picker and the first run

**Files:**
- Create: `components/ui/first_run.c`, `components/ui/include/first_run.h`
- Modify: `components/ui/launcher.c`, `components/ui/include/launcher.h`, `components/ui/app_manager.c`, `components/ui/include/tokens.h`

**Interfaces:**
- Consumes:
  - `settings_first_run_done()` and `settings_set_first_run_done()` (Task 3);
  - `cat_variant_name()`, `cat_set_variant()` (reloads live cats) and `assets_release_prefix()` (Task 4);
  - `bedtime_take_over()`, `launcher_show()`, `ui_cut_touch()`.
- Produces (used by Tasks 7-9):
  - `void first_run_picker(lv_obj_t *parent, void (*done)(void))`
  - `bool first_run_due(void)`
  - `void first_run_show(void)`
  - `bool first_run_showing(void)`
  - `void first_run_skip(void)`
  - `void launcher_cat_changed(void)`

- [ ] **Step 1: Tokens**

Append to `components/ui/include/tokens.h`:

```c
/* Cat picker (Screen.dc.html pick view, PICK and V; spec §3.13) */
#define PICK_PAD          156  /* the touch target */
#define PICK_RING_R       72   /* the drawn circle, to the middle of its stroke */
#define PICK_RING_THIN    5
#define PICK_RING_THICK   10   /* the chosen cat */
#define PICK_FILL_OPA     48   /* the chosen pad: its fur color at 0x30 */
#define PICK_CAT_DX       13   /* scale S origin inside the pad */
#define PICK_CAT_DY       8
#define PICK_CELEBRATE_MS 1400
#define PICK_CENTERS      {{233, 128}, {143, 300}, {323, 300}} /* orange, grey, tux */
#define COLOR_FUR_ORANGE  0xFF9F43
#define COLOR_FUR_GREY    0xA9B6C9
#define COLOR_FUR_TUX     0x3B3B52
```

- [ ] **Step 2: The menu's paws follow the variant**

The menu's two paw images are the chosen variant's `paw` part (`launcher_create()`). `cat_set_variant()` reloads cats, not these.

In `components/ui/launcher.c`:
- add `s_paw_a, *s_paw_b` to the first `static lv_obj_t *` line;
- in `launcher_create()`, assign the two `ui_image(s_scr, paw, ...)` results to `s_paw_a` and `s_paw_b`;
- at the end of the file:

```c
void launcher_cat_changed(void)
{
    char paw[48];
    cat_part_asset(paw, sizeof(paw), CAT_SCALE_M, "paw");
    lv_image_set_src(s_paw_a, assets_image(paw));
    lv_image_set_src(s_paw_b, assets_image(paw));
}
```

In `components/ui/include/launcher.h`:

```c
/* The chosen cat changed (cat_set_variant() already switched the cat itself): the paws follow. */
void launcher_cat_changed(void);
```

- [ ] **Step 3: `first_run.h`**

```c
#pragma once
#include <stdbool.h>
#include "lvgl.h"

/* The cat picker (design "Pick your cat"; spec §3.13), and the first run that shows it after the boot animation.
 * LVGL task only. */

/* Builds the three pads into parent (a full-screen object). The current cat's pad has the thick ring. A tap
 * chooses at once (the thick ring, the proud face, a meow; the choice and "first run done" are saved, and the live
 * cats switch), and done runs PICK_CELEBRATE_MS later. Once the picker is deleted, the other variants' images are
 * released. */
void first_run_picker(lv_obj_t *parent, void (*done)(void));
/* "first run done" is missing (never in the soak build, which opens apps from boot on). */
bool first_run_due(void);
/* Shows the picker on its own screen, fading in over the boot animation. When it ends, the lock shows if it is
 * night, else the menu. */
void first_run_show(void);
bool first_run_showing(void);
/* Home on the first-run picker: the menu (or the lock) now. It saves nothing unless a cat was already chosen. */
void first_run_skip(void);
```

- [ ] **Step 4: `first_run.c`**

```c
#include "first_run.h"
#include <stdint.h>
#include <stdio.h>
#include "assets.h"
#include "audio.h"
#include "bedtime.h"
#include "cat.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "launcher.h"
#include "sdkconfig.h"
#include "settings.h"
#include "tokens.h"
#include "ui_util.h"

static const char *TAG = "first_run";
static const uint32_t FUR[CAT_VARIANT_COUNT] = {COLOR_FUR_ORANGE, COLOR_FUR_GREY, COLOR_FUR_TUX};
static const int16_t CENTER[CAT_VARIANT_COUNT][2] = PICK_CENTERS;

static lv_obj_t *s_root; /* the picker's container; NULL when none is built */
static lv_obj_t *s_ring[CAT_VARIANT_COUNT], *s_cat[CAT_VARIANT_COUNT];
static lv_timer_t *s_timer; /* the celebration, then done */
static void (*s_done)(void);
static bool s_chosen;
static lv_obj_t *s_screen; /* the first-run screen while it shows */

/* The design's pad circle: r 72 with its stroke centered on it (5 px, 10 px and the tinted fill when chosen). */
static void style_ring(int v, bool chosen)
{
    const int32_t sw = chosen ? PICK_RING_THICK : PICK_RING_THIN;
    const int32_t d = 2 * PICK_RING_R + sw;
    lv_obj_set_size(s_ring[v], d, d);
    lv_obj_set_pos(s_ring[v], (PICK_PAD - d) / 2, (PICK_PAD - d) / 2);
    lv_obj_set_style_border_width(s_ring[v], sw, 0);
    lv_obj_set_style_bg_opa(s_ring[v], chosen ? PICK_FILL_OPA : LV_OPA_TRANSP, 0);
}

static void finish(lv_timer_t *t)
{
    (void)t;
    s_timer = NULL; /* one-shot timers delete themselves */
    if (s_done) {
        s_done();
    }
}

static void pad_clicked(lv_event_t *e)
{
    if (!s_root || s_chosen) {
        return; /* gone, or already chosen: one choice per picker */
    }
    const int v = (int)(uintptr_t)lv_event_get_user_data(e);
    s_chosen = true;
    for (int i = 0; i < CAT_VARIANT_COUNT; i++) {
        style_ring(i, i == v);
    }
    cat_set_state(s_cat[v], CAT_PROUD);
    audio_play(SND_MEOW);
    settings_set_cat_variant((uint8_t)v);
    settings_set_first_run_done();
    cat_set_variant((cat_variant_t)v); /* the menu's cat, and the lock's if it is built, switch now */
    launcher_cat_changed();
    ESP_LOGI(TAG, "cat chosen: %s", cat_variant_name((cat_variant_t)v));
    s_timer = lv_timer_create(finish, PICK_CELEBRATE_MS, NULL);
    lv_timer_set_repeat_count(s_timer, 1);
}

/* Runs after the picker's deletion has finished, so no object shows the other variants any more (spec §4.6:
 * one variant in memory). */
static void release_others(void *arg)
{
    (void)arg;
    const int keep = settings_cat_variant();
    char prefix[16];
    for (int v = 0; v < CAT_VARIANT_COUNT; v++) {
        if (v != keep) {
            snprintf(prefix, sizeof(prefix), "cat/%s/", cat_variant_name((cat_variant_t)v));
            assets_release_prefix(prefix);
        }
    }
    ESP_LOGI(TAG, "free psram after the cat choice: %u", (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
}

static void picker_deleted(lv_event_t *e)
{
    (void)e;
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    s_root = NULL;
    for (int i = 0; i < CAT_VARIANT_COUNT; i++) {
        s_ring[i] = s_cat[i] = NULL;
    }
    lv_async_call(release_others, NULL);
}

void first_run_picker(lv_obj_t *parent, void (*done)(void))
{
    s_done = done;
    s_chosen = false;
    s_root = ui_plain(parent);
    lv_obj_set_size(s_root, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_add_event_cb(s_root, picker_deleted, LV_EVENT_DELETE, NULL);
    const int current = settings_cat_variant();
    for (int i = 0; i < CAT_VARIANT_COUNT; i++) {
        lv_obj_t *pad = ui_plain(s_root);
        lv_obj_set_size(pad, PICK_PAD, PICK_PAD);
        lv_obj_set_pos(pad, CENTER[i][0] - PICK_PAD / 2, CENTER[i][1] - PICK_PAD / 2);
        lv_obj_add_flag(pad, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_add_event_cb(pad, pad_clicked, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        s_ring[i] = ui_plain(pad);
        lv_obj_set_style_radius(s_ring[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_color(s_ring[i], lv_color_hex(FUR[i]), 0);
        lv_obj_set_style_bg_color(s_ring[i], lv_color_hex(FUR[i]), 0);
        style_ring(i, i == current);
        s_cat[i] = cat_create_colored(pad, CAT_SCALE_S, cat_variant_name((cat_variant_t)i));
        cat_set_origin(s_cat[i], PICK_CAT_DX, PICK_CAT_DY);
    }
}

bool first_run_due(void)
{
#if CONFIG_CAT_DEV_SOAK
    return false;
#else
    return !settings_first_run_done();
#endif
}

static void end_first_run(void)
{
    s_screen = NULL;
    if (!bedtime_take_over()) { /* night: the lock, which deletes this screen */
        launcher_show();        /* deletes this screen once the menu has faded in */
    }
}

void first_run_show(void)
{
    ESP_LOGI(TAG, "first run: the cat picker");
    s_screen = ui_screen();
    first_run_picker(s_screen, end_first_run);
    lv_screen_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_HOME_MS, 0, true); /* the boot screen goes */
}

bool first_run_showing(void) { return s_screen != NULL; }

void first_run_skip(void)
{
    if (!s_screen) {
        return;
    }
    ui_cut_touch(s_screen);
    end_first_run();
}
```

- [ ] **Step 5: The first run in the app manager**

In `components/ui/app_manager.c`, add `#include "first_run.h"`. In `app_manager_start()`, replace the `bedtime_take_over()`/`launcher_show()` lines with:

```c
    if (first_run_due()) {
        first_run_show(); /* the picker; its end shows the lock or the menu */
    } else if (!bedtime_take_over()) { /* a start inside the night shows the lock after the boot animation */
        launcher_show();
    }
```

In `do_home()`, after the `idle_wake_if_asleep()` block:

```c
    if (first_run_showing()) {
        first_run_skip(); /* Home works everywhere but on the lock (spec §3.6, §3.13) */
        return;
    }
```

While the first run shows, the bedtime tick's `start_night()` keeps returning early (the active screen is not the menu), and `end_first_run()` then shows the lock itself. Nothing else changes.

- [ ] **Step 6: Build and test**

```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py -B build-m7 -D SDKCONFIG=build-m7/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" reconfigure build 2>&1 | tail -1
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" build 2>&1 | tail -1
sh test/run.sh 2>&1 | grep "tests passed"
```

Expected: both builds complete with no new warnings. Host tests `100% tests passed out of 14`.

- [ ] **Step 7: Commit**

```bash
git add components/ui/first_run.c components/ui/include/first_run.h components/ui/launcher.c \
        components/ui/include/launcher.h components/ui/app_manager.c components/ui/include/tokens.h
git commit -m "feat: the first-run cat picker"
```

---

### Task 7: The parent area: four tiles, the battery, Show apps, the Cat tile, Fredoka

**Files:**
- Modify: `components/ui/parent.c` (rewritten, below), `components/ui/include/parent.h` (comment), `components/ui/include/tokens.h`

**Interfaces:**
- Consumes:
  - `fonts.h` (Task 5);
  - `power_state()` (Task 3) and `POWER_LOW_PCT` (Task 2);
  - `first_run_picker()` (Task 6);
  - the images `ui/p_grid`, `ui/p_cat` and `ui/p_eye_off` (Task 4);
  - `settings_hidden_apps()`, `settings_set_hidden_apps()`, `APPS`, `APP_COUNT` and `app_t.icon_small`.
- Produces: `COLOR_CHARGE` and `COLOR_BATT_LOW` in `tokens.h` (also used by Task 8).

- [ ] **Step 1: Tokens**

In `components/ui/include/tokens.h`, in the "Parent home and Sleep times" block, replace the three lines `PARENT_TILE_Y`, `PARENT_TILE_VOL_X` and `PARENT_TILE_SLEEP_X` with:

```c
#define PARENT_TILE_X1       88       /* the home's 2 x 2 tiles: Volume, Sleep / Apps, Cat */
#define PARENT_TILE_X2       238
#define PARENT_TILE_Y1       88
#define PARENT_TILE_Y2       238
```

Append a new block:

```c
/* Parent home battery and Show apps (design "Parent · home", "Parent · show apps"; spec §3.13) */
#define COLOR_CHARGE        0x5CD67A /* the battery ring, the charging glyph */
#define COLOR_BATT_LOW      0xFF4D4D /* the low-battery glyph; the home's battery at 10 % or less */
#define COLOR_P_BATT_TEXT   0xBDF5C9
#define COLOR_P_OFF         0x77738C /* a hidden app's label */
#define PARENT_BATT_X       197      /* the 72 px battery circle in the middle of the tiles */
#define PARENT_BATT_Y       197
#define PARENT_BATT_D       72
#define PARENT_BATT_BORDER  3
#define PARENT_APPS_TITLE_Y 96
#define PARENT_APPS_MAX     6        /* the 3 x 2 grid */
#define PARENT_APP_X0       83
#define PARENT_APP_Y0       128
#define PARENT_APP_DX       100
#define PARENT_APP_DY       124
#define PARENT_APP_W        100
#define PARENT_APP_H        126
#define PARENT_APP_ICON     96
#define PARENT_APP_LABEL_Y  100
#define PARENT_APP_OFF_OPA  77       /* a hidden app's icon at 30 % */
#define PARENT_EYE_X        66       /* the crossed-eye badge, from the app's top-left */
#define PARENT_EYE_Y        (-2)
```

- [ ] **Step 2: Rewrite `components/ui/parent.c`**

The whole file becomes the code below. What changes from M6:
- Fredoka everywhere;
- the 2 × 2 tiles with the battery in the middle;
- `PAGE_APPS` and `PAGE_CAT`;
- one page timer, covering both Now and the battery;
- the "‹ Parents" label.

The Sleep times logic is unchanged.

If Task 5 reported that the font lacks U+2039, use `"< Parents"` for `BACK_LABEL`.

```c
#include "parent.h"
#include <stdint.h>
#include <string.h>
#include "audio.h"
#include "cat.h"
#include "clock.h"
#include "first_run.h"
#include "fonts.h"
#include "power.h"
#include "power_rules.h"
#include "schedule.h"
#include "settings.h"
#include "tokens.h"
#include "ui_util.h"
#include "volume.h"

/* The parent area (spec §3.1, §3.12, §3.13): a home screen with the Volume, Sleep, Apps and Cat tiles and the
 * battery in the middle, then Max volume, Sleep times, Show apps and the cat picker. The pages share the one app
 * screen: a page switch cleans it and builds the next page. Home (BOOT) leaves the whole area. */

#define BACK_LABEL "\xE2\x80\xB9 Parents" /* "‹ Parents" */

typedef enum { PAGE_HOME, PAGE_VOLUME, PAGE_TIMES, PAGE_APPS, PAGE_CAT } page_t;
typedef enum { ROW_BED, ROW_WAKE, ROW_NOW, ROW_COUNT } row_t;

static const struct {
    const char *icon, *label;
} ROWS[ROW_COUNT] = {
    [ROW_BED] = {"ui/p_moon_row", "Bedtime"},
    [ROW_WAKE] = {"ui/p_sun_row", "Wake up"},
    [ROW_NOW] = {"ui/p_clock_row", "Now"},
};

/* Show apps' labels (the design's APPS), by app id; an app missing here shows its id. */
static const struct {
    const char *id, *label;
} APP_LABELS[] = {
    {"voices", "Voices"}, {"pet", "Pet"}, {"draw", "Draw"}, {"yarn", "Yarn"}, {"clock", "Clock"}, {"piano", "Piano"},
};

static const char *const CAT_NAMES[CAT_VARIANT_COUNT] = {"Orange", "Grey", "Tuxedo"}; /* the design's catNames */

static lv_obj_t *s_scr; /* NULL once destroyed: every callback checks it */
static page_t s_page;
static lv_obj_t *s_arc, *s_value;       /* Max volume */
static lv_obj_t *s_times[ROW_COUNT];    /* Sleep times: the three values; NULL off that page */
static lv_obj_t *s_batt, *s_batt_text;  /* the home's battery; NULL off the home */
static lv_timer_t *s_page_timer;        /* every second: Now on Sleep times, the battery on the home */
static uint16_t s_bed, s_wake;          /* edited on Sleep times, saved when a button is let go */
static uint32_t s_press_ms;             /* when the current ± press began */

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
    lv_obj_center(label_in(b, &font_fredoka_20, COLOR_P_TEXT, BACK_LABEL));
    lv_obj_add_event_cb(b, go_page, LV_EVENT_CLICKED, (void *)(uintptr_t)PAGE_HOME);
}

static uint8_t shown_apps(uint32_t hidden)
{
    uint8_t n = 0;
    for (size_t i = 0; i < APP_COUNT; i++) {
        n += (hidden & (1u << i)) ? 0 : 1;
    }
    return n;
}

/* ---- home ---- */

static void tile(int32_t x, int32_t y, const char *icon, const char *label, const char *value, page_t page)
{
    lv_obj_t *b = surface_button(x, y, PARENT_TILE_SIZE, PARENT_TILE_SIZE);
    lv_obj_add_event_cb(b, go_page, LV_EVENT_CLICKED, (void *)(uintptr_t)page);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(b, PARENT_TILE_PAD_ROW, 0);
    ui_image(b, icon, 0, 0); /* the flex layout places it */
    label_in(b, &font_fredoka_20, COLOR_P_TEXT, label);
    label_in(b, &font_fredoka_17, COLOR_P_VALUE, value);
}

/* The battery in the middle (spec §3.13): hidden without a gauge (§4.7), red at 10 % or less. */
static void show_battery(void)
{
    const power_state_t ps = power_state();
    ui_set_hidden(s_batt, ps.pct < 0);
    if (ps.pct < 0) {
        return;
    }
    const bool low = ps.pct <= POWER_LOW_PCT;
    lv_obj_set_style_border_color(s_batt, lv_color_hex(low ? COLOR_BATT_LOW : COLOR_CHARGE), 0);
    lv_obj_set_style_text_color(s_batt_text, lv_color_hex(low ? COLOR_BATT_LOW : COLOR_P_BATT_TEXT), 0);
    lv_label_set_text_fmt(s_batt_text, "%d%%", ps.pct);
}

static void build_home(void)
{
    text(s_scr, &font_fredoka_22, COLOR_P_MUTED, PARENT_TITLE_X, PARENT_HOME_TITLE_Y, PARENT_TITLE_W, "Parents");
    char vol[8], bed[6], apps[12];
    lv_snprintf(vol, sizeof(vol), "%u%%", (unsigned)settings_volume_cap());
    schedule_format(settings_bedtime(), bed);
    lv_snprintf(apps, sizeof(apps), "%u of %u", (unsigned)shown_apps(settings_hidden_apps()), (unsigned)APP_COUNT);
    tile(PARENT_TILE_X1, PARENT_TILE_Y1, "ui/p_vol", "Volume", vol, PAGE_VOLUME);
    tile(PARENT_TILE_X2, PARENT_TILE_Y1, "ui/p_moon", "Sleep", bed, PAGE_TIMES);
    tile(PARENT_TILE_X1, PARENT_TILE_Y2, "ui/p_grid", "Apps", apps, PAGE_APPS);
    tile(PARENT_TILE_X2, PARENT_TILE_Y2, "ui/p_cat", "Cat", CAT_NAMES[settings_cat_variant() % CAT_VARIANT_COUNT],
         PAGE_CAT);
    s_batt = ui_plain(s_scr);
    lv_obj_set_pos(s_batt, PARENT_BATT_X, PARENT_BATT_Y);
    lv_obj_set_size(s_batt, PARENT_BATT_D, PARENT_BATT_D);
    lv_obj_set_style_radius(s_batt, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_batt, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_batt, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_batt, PARENT_BATT_BORDER, 0);
    s_batt_text = label_in(s_batt, &font_fredoka_20, COLOR_P_BATT_TEXT, "");
    lv_obj_center(s_batt_text);
    show_battery();
    text(s_scr, &font_fredoka_17, COLOR_P_MUTED, PARENT_HINT_X, PARENT_HINT_Y, PARENT_HINT_W, "Home button exits");
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
    text(s_scr, &font_fredoka_20, COLOR_P_MUTED, PARENT_TITLE_X, PARENT_VOL_TITLE_Y, PARENT_TITLE_W, "Max volume");
    s_arc = ui_arc(s_scr, PARENT_ARC_CX, PARENT_ARC_CY, PARENT_ARC_R, PARENT_ARC_W);
    lv_arc_set_bg_angles(s_arc, 135, 45); /* 270 degrees, gap at the bottom */
    lv_arc_set_range(s_arc, 0, VOLUME_MAX);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_ARC), LV_PART_INDICATOR);
    s_value = text(s_scr, &font_fredoka_54, COLOR_P_TEXT, PARENT_VALUE_X, PARENT_VOL_VALUE_Y, PARENT_VALUE_W, "");
    lv_obj_t *m = surface_button(PARENT_MINUS_X, PARENT_BTN_Y, PARENT_BTN_SIZE, PARENT_BTN_SIZE);
    lv_obj_center(label_in(m, &font_fredoka_46, COLOR_P_TEXT, "-"));
    lv_obj_add_event_cb(m, on_minus, LV_EVENT_CLICKED, NULL);
    lv_obj_t *p = surface_button(PARENT_PLUS_X, PARENT_BTN_Y, PARENT_BTN_SIZE, PARENT_BTN_SIZE);
    lv_obj_center(label_in(p, &font_fredoka_46, COLOR_P_TEXT, "+"));
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

static void build_times(void)
{
    back_button();
    s_bed = settings_bedtime();
    s_wake = settings_wake();
    for (int r = 0; r < ROW_COUNT; r++) {
        const int32_t y = PARENT_ROW_Y0 + r * PARENT_ROW_STEP;
        lv_obj_t *head = ui_plain(s_scr); /* icon and label, centered as a pair */
        lv_obj_set_pos(head, PARENT_ROW_HEAD_X, y);
        lv_obj_set_size(head, PARENT_ROW_HEAD_W, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(head, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(head, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_column(head, PARENT_ROW_HEAD_PAD, 0);
        ui_image(head, ROWS[r].icon, 0, 0);
        label_in(head, &font_fredoka_20, COLOR_P_MUTED, ROWS[r].label);
        s_times[r] = text(s_scr, &font_fredoka_46, COLOR_P_TEXT, PARENT_VALUE_X, y + PARENT_ROW_VALUE_DY,
                          PARENT_VALUE_W, "");
        for (int d = 0; d < 2; d++) {
            lv_obj_t *b = surface_button(d ? PARENT_ROW_PLUS_X : PARENT_ROW_MINUS_X, y + PARENT_ROW_BTN_DY,
                                         PARENT_ROW_BTN, PARENT_ROW_BTN);
            lv_obj_center(label_in(b, &font_fredoka_46, COLOR_P_TEXT, d ? "+" : "-"));
            lv_obj_add_event_cb(b, time_button, LV_EVENT_ALL, (void *)(uintptr_t)(r * 2 + d));
        }
    }
    text(s_scr, &font_fredoka_17, COLOR_P_MUTED, PARENT_TIMES_NOTE_X, PARENT_TIMES_NOTE_Y, PARENT_TIMES_NOTE_W,
         "Outside these hours\nthe toy sleeps");
    show_times();
}

/* ---- Show apps ---- */

static const char *app_label(const app_t *a)
{
    for (size_t i = 0; i < sizeof(APP_LABELS) / sizeof(APP_LABELS[0]); i++) {
        if (strcmp(APP_LABELS[i].id, a->id) == 0) {
            return APP_LABELS[i].label;
        }
    }
    return a->id;
}

static void app_toggled(lv_event_t *e)
{
    if (!s_scr || s_page != PAGE_APPS) {
        return;
    }
    const uint32_t bit = 1u << (uintptr_t)lv_event_get_user_data(e);
    const uint32_t hidden = settings_hidden_apps();
    if (!(hidden & bit) && shown_apps(hidden) <= 1) {
        return; /* the last shown app stays (design: at most 5 of 6 hidden) */
    }
    settings_set_hidden_apps(hidden ^ bit); /* the menu shows it when the parent leaves (launcher_show) */
    lv_async_call(page_async, (void *)(uintptr_t)PAGE_APPS); /* rebuilt: the button this came from goes */
}

static void build_apps(void)
{
    back_button();
    text(s_scr, &font_fredoka_20, COLOR_P_MUTED, PARENT_TITLE_X, PARENT_APPS_TITLE_Y, PARENT_TITLE_W, "Show apps");
    const uint32_t hidden = settings_hidden_apps();
    for (size_t i = 0; i < APP_COUNT && i < PARENT_APPS_MAX; i++) {
        const bool off = (hidden & (1u << i)) != 0;
        lv_obj_t *b = ui_plain(s_scr);
        lv_obj_set_pos(b, PARENT_APP_X0 + (int32_t)(i % 3) * PARENT_APP_DX,
                       PARENT_APP_Y0 + (int32_t)(i / 3) * PARENT_APP_DY);
        lv_obj_set_size(b, PARENT_APP_W, PARENT_APP_H);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_OVERFLOW_VISIBLE);
        lv_obj_add_event_cb(b, app_toggled, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        lv_obj_t *icon = ui_image(b, APPS[i]->icon_small, (PARENT_APP_W - PARENT_APP_ICON) / 2, 0);
        lv_obj_set_style_image_opa(icon, off ? PARENT_APP_OFF_OPA : LV_OPA_COVER, 0);
        text(b, &font_fredoka_17, off ? COLOR_P_OFF : COLOR_P_TEXT, 0, PARENT_APP_LABEL_Y, PARENT_APP_W,
             app_label(APPS[i]));
        if (off) {
            ui_image(b, "ui/p_eye_off", PARENT_EYE_X, PARENT_EYE_Y);
        }
    }
}

/* ---- Cat ---- */

static void cat_chosen(void)
{
    lv_async_call(page_async, (void *)(uintptr_t)PAGE_HOME); /* the page switch deletes the picker */
}

/* ---- pages ---- */

static void page_tick(lv_timer_t *t)
{
    (void)t;
    if (!s_scr) {
        return;
    }
    if (s_times[ROW_NOW]) {
        show_times();
    }
    if (s_batt) {
        show_battery();
    }
}

static void leave_page(void)
{
    if (s_times[ROW_BED]) {
        save_times();
    }
    s_arc = s_value = NULL;
    s_batt = s_batt_text = NULL;
    for (int r = 0; r < ROW_COUNT; r++) {
        s_times[r] = NULL;
    }
}

static void open_page(page_t page)
{
    leave_page();
    lv_obj_clean(s_scr);
    s_page = page;
    switch (page) {
    case PAGE_VOLUME: build_volume(); break;
    case PAGE_TIMES: build_times(); break;
    case PAGE_APPS: build_apps(); break;
    case PAGE_CAT: first_run_picker(s_scr, cat_chosen); break;
    default: build_home(); break;
    }
}

static void parent_create(lv_obj_t *scr)
{
    s_scr = scr;
    s_page_timer = lv_timer_create(page_tick, 1000, NULL);
    open_page(PAGE_HOME);
}

static void parent_destroy(void)
{
    leave_page(); /* saves the times: Home in the middle of a hold never delivers the release */
    if (s_page_timer) {
        lv_timer_delete(s_page_timer);
        s_page_timer = NULL;
    }
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

The Cat page has no back button. The design's pick view has none, and "‹ Parents" would overlap the orange pad. Tapping any cat (even the current one) returns to the home after the celebration, and Home leaves the area.

- [ ] **Step 3: `parent.h`**

Replace its comment with:

```c
/* The parent area (spec §3.1, §3.12, §3.13). Opened by a two-finger hold on the menu or on the bedtime lock,
 * never from the carousel, so it is not in APPS. Home tiles (Volume, Sleep, Apps, Cat) and the battery; Max
 * volume, Sleep times, Show apps and the cat picker. */
```

- [ ] **Step 4: Build and test**

```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py -B build-m7 -D SDKCONFIG=build-m7/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" build 2>&1 | tail -1
sh test/run.sh 2>&1 | grep "tests passed"
grep -n "montserrat" components/ui/parent.c
grep -nwE "PARENT_TILE_VOL_X|PARENT_TILE_SLEEP_X|PARENT_TILE_Y" components components/ui/include/tokens.h -r
```

Expected:
- the build completes with no new warnings;
- `100% tests passed out of 14`;
- the grep prints nothing: no Montserrat in the parent area, and no old tile tokens left.

- [ ] **Step 5: Commit**

```bash
git add components/ui/parent.c components/ui/include/parent.h components/ui/include/tokens.h
git commit -m "feat: the parent home's four tiles and battery, Show apps and the Cat tile, in Fredoka"
```

---

### Task 8: The hungry and eating cat, battery events and the critical sleep

**Files:**
- Create: `components/ui/overlays.c`, `components/ui/include/overlays.h`, `components/ui/power_watch.c`, `components/ui/include/power_watch.h`
- Modify: `components/ui/app_manager.c`, `components/ui/bedtime.c`, `components/ui/include/bedtime.h`, `components/ui/include/tokens.h`

**Interfaces:**
- Consumes:
  - `battery_rules_*` (Task 2);
  - `power_state()`, `deepsleep_start()` and `SENSOR_EV_USB_IN` (Task 3);
  - `SND_HUNGRY`, `SND_CHARGING` and the battery and bowl images (Task 4);
  - `first_run_showing()` (Task 6);
  - `COLOR_CHARGE` and `COLOR_BATT_LOW` (Task 7);
  - `idle_level()`, `idle_wake_if_asleep()`, `bedtime_showing()`, `app_manager_started()` and `app_manager_current()`.
- Produces (used by Task 9):
  - `overlays.h`: `overlay_kind_t {OVERLAY_HUNGRY, OVERLAY_CHARGING}`, `void overlay_show(overlay_kind_t kind, void (*done)(void))`, `bool overlay_showing(void)`, `void overlay_close(void)`;
  - `power_watch.h`: `void power_watch_init(void)`, `void power_watch_usb_in(void)`;
  - `bedtime.h`: `bool bedtime_panel_lit(void)`.

- [ ] **Step 1: Tokens**

Append to `components/ui/include/tokens.h`:

```c
/* Low battery and charging screens (Screen.dc.html lowbatt and charging views; spec §3.13) */
#define OVERLAY_MS            4000
#define OVERLAY_BATT_X        197  /* the 72 x 38 battery glyph */
#define OVERLAY_BATT_Y        26
#define OVERLAY_CAT_X         133  /* scale M origin: the design's 220 px cat, centered */
#define OVERLAY_CAT_Y         86
#define OVERLAY_BOWL_X        153
#define OVERLAY_BOWL_EMPTY_Y  316
#define OVERLAY_BOWL_FULL_Y   304
#define OVERLAY_BLINK_MS      1600 /* the red battery: 30 % -> 100 % -> 30 % (.blink-slow) */
#define OVERLAY_BLINK_LOW_OPA 77
#define CHARGE_FILL_X         11   /* the growing fill, inside the glyph */
#define CHARGE_FILL_Y         11
#define CHARGE_FILL_W         42
#define CHARGE_FILL_MIN_W     6    /* the design's scaleX(0.15) */
#define CHARGE_FILL_H         16
#define CHARGE_FILL_R         3
#define CHARGE_FILL_MS        2000
```

- [ ] **Step 2: `overlays.h` and `overlays.c`**

`components/ui/include/overlays.h`:

```c
#pragma once
#include <stdbool.h>

/* The low-battery and charging screens (design lowbatt and charging; spec §3.13): OVERLAY_MS over whatever shows,
 * on LVGL's top layer, taking every touch. LVGL task only. */
typedef enum {
    OVERLAY_HUNGRY,   /* the sleepy cat, the empty bowl, the blinking red battery, "Gladna sam…" */
    OVERLAY_CHARGING, /* the eating cat, the full bowl, the filling green battery, munching and "Njam njam!" */
} overlay_kind_t;

/* Shows it for OVERLAY_MS; done (may be NULL) runs when it ends, early or not. Nothing if one already shows. */
void overlay_show(overlay_kind_t kind, void (*done)(void));
bool overlay_showing(void);
/* Ends it now (Home, PWR); done still runs. */
void overlay_close(void);
```

`components/ui/overlays.c`:

```c
#include "overlays.h"
#include <stddef.h>
#include "audio.h"
#include "cat.h"
#include "esp_log.h"
#include "tokens.h"
#include "ui_util.h"

static const char *TAG = "overlay";
static lv_obj_t *s_root; /* NULL when none shows */
static lv_timer_t *s_timer;
static void (*s_done)(void);

static void set_image_opa(void *obj, int32_t v) { lv_obj_set_style_image_opa(obj, (lv_opa_t)v, 0); }
static void set_width(void *obj, int32_t v) { lv_obj_set_width(obj, v); }

/* A looping animation; `back` plays it back as well (the design's alternating keyframes). Deleting the object
 * deletes it. */
static void loop_anim(void *var, lv_anim_exec_xcb_t exec, int32_t from, int32_t to, uint32_t ms, bool back)
{
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, var);
    lv_anim_set_exec_cb(&a, exec);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    if (back) {
        lv_anim_set_reverse_duration(&a, ms);
    }
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

void overlay_close(void)
{
    if (!s_root) {
        return;
    }
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    lv_obj_delete(s_root);
    s_root = NULL;
    void (*done)(void) = s_done;
    s_done = NULL;
    if (done) {
        done();
    }
}

static void timed_out(lv_timer_t *t)
{
    (void)t;
    s_timer = NULL; /* one-shot timers delete themselves */
    overlay_close();
}

void overlay_show(overlay_kind_t kind, void (*done)(void))
{
    if (s_root) {
        return;
    }
    const bool hungry = kind == OVERLAY_HUNGRY;
    ESP_LOGI(TAG, "%s", hungry ? "low battery" : "charging");
    s_done = done;
    s_root = ui_plain(lv_layer_top());
    lv_obj_set_size(s_root, SCREEN_SIZE, SCREEN_SIZE);
    lv_obj_set_style_bg_color(s_root, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_CLICKABLE); /* takes every touch: nothing reaches the app underneath */
    lv_obj_t *cat = cat_create(s_root, CAT_SCALE_M);
    cat_set_origin(cat, OVERLAY_CAT_X, OVERLAY_CAT_Y);
    cat_set_state(cat, hungry ? CAT_SLEEPY : CAT_EATING);
    ui_image(s_root, hungry ? "ui/bowl_empty" : "ui/bowl_full", OVERLAY_BOWL_X,
             hungry ? OVERLAY_BOWL_EMPTY_Y : OVERLAY_BOWL_FULL_Y);
    if (hungry) {
        lv_obj_t *batt = ui_image(s_root, "ui/batt_low", OVERLAY_BATT_X, OVERLAY_BATT_Y);
        loop_anim(batt, set_image_opa, OVERLAY_BLINK_LOW_OPA, LV_OPA_COVER, OVERLAY_BLINK_MS / 2, true);
    } else {
        ui_image(s_root, "ui/batt_charge", OVERLAY_BATT_X, OVERLAY_BATT_Y);
        lv_obj_t *fill = ui_plain(s_root);
        lv_obj_set_pos(fill, OVERLAY_BATT_X + CHARGE_FILL_X, OVERLAY_BATT_Y + CHARGE_FILL_Y);
        lv_obj_set_size(fill, CHARGE_FILL_MIN_W, CHARGE_FILL_H);
        lv_obj_set_style_radius(fill, CHARGE_FILL_R, 0);
        lv_obj_set_style_bg_color(fill, lv_color_hex(COLOR_CHARGE), 0);
        lv_obj_set_style_bg_opa(fill, LV_OPA_COVER, 0);
        loop_anim(fill, set_width, CHARGE_FILL_MIN_W, CHARGE_FILL_W, CHARGE_FILL_MS, false);
        ui_image(s_root, "ui/batt_bolt", OVERLAY_BATT_X, OVERLAY_BATT_Y); /* over the fill */
    }
    audio_loop_stop(); /* a purr or rumble stops too: the app gets no touch or tilt meanwhile */
    audio_play(hungry ? SND_HUNGRY : SND_CHARGING);
    lv_display_trigger_activity(NULL);
    s_timer = lv_timer_create(timed_out, OVERLAY_MS, NULL);
    lv_timer_set_repeat_count(s_timer, 1);
}

bool overlay_showing(void) { return s_root != NULL; }
```

- [ ] **Step 3: `bedtime_panel_lit()`**

In `components/ui/include/bedtime.h`, after `bedtime_showing()`:

```c
/* The lock's panel is lit (the sunrise always is). Meaningful while bedtime_showing(). */
bool bedtime_panel_lit(void);
```

In `components/ui/bedtime.c`, after `bedtime_showing()`:

```c
bool bedtime_panel_lit(void) { return s_state == BT_SUNRISE || s_lit; }
```

- [ ] **Step 4: `power_watch.h` and `power_watch.c`**

`components/ui/include/power_watch.h`:

```c
#pragma once

/* The battery screens, the critical sleep and deep sleep (spec §3.13), checked every second on the LVGL task. */
void power_watch_init(void);   /* app_manager_init() calls it */
void power_watch_usb_in(void); /* USB power was just connected: the charging screen, where it may show */
```

`components/ui/power_watch.c`:

```c
#include "power_watch.h"
#include "app_manager.h"
#include "bedtime.h"
#include "deepsleep.h"
#include "esp_log.h"
#include "first_run.h"
#include "idle.h"
#include "overlays.h"
#include "parent.h"
#include "power.h"
#include "power_rules.h"

#define POWER_WATCH_MS 1000

static const char *TAG = "power_watch";
static battery_rules_t s_rules;

/* Where the battery screens may show (spec §3.13): over the menu or an open app, never over the boot animation,
 * the cat picker, the parent area, the lock or the sunrise. */
static bool can_show(void)
{
    return app_manager_started() && !overlay_showing() && !first_run_showing() && !bedtime_showing() &&
           app_manager_current() != &PARENT_APP;
}

/* The panel is off: the menu's idle off or PWR, or the lock's dark panel. */
static bool panel_dark(void) { return bedtime_showing() ? !bedtime_panel_lit() : idle_level() == 3; }

static void sleep_for_battery(void) { deepsleep_start(0, true); }

static void tick(lv_timer_t *t)
{
    (void)t;
    if (!app_manager_started()) {
        return; /* the boot animation */
    }
    const power_state_t ps = power_state();
    if (battery_rules_update(&s_rules, ps.pct, ps.usb) == BATTERY_CRITICAL) {
        if (overlay_showing()) {
            return; /* a screen is up: the next tick takes over once it has gone (or its done sleeps) */
        }
        ESP_LOGW(TAG, "battery %d %%: sleeping until it is charged", ps.pct);
        if (panel_dark()) {
            sleep_for_battery();
        } else {
            overlay_show(OVERLAY_HUNGRY, sleep_for_battery); /* from anywhere: the clock must survive */
        }
        return;
    }
    if (can_show() && idle_level() == 0 && battery_rules_take_pending(&s_rules)) {
        overlay_show(OVERLAY_HUNGRY, NULL);
    }
}

void power_watch_init(void)
{
    battery_rules_init(&s_rules);
    lv_timer_create(tick, POWER_WATCH_MS, NULL);
}

void power_watch_usb_in(void)
{
    if (power_state().pct < 0 || !can_show()) {
        return; /* no gauge, or somewhere it never shows, and it is not shown later (spec §3.13) */
    }
    idle_wake_if_asleep(); /* connecting USB counts as a touch: a dim or dark menu lights up first */
    overlay_show(OVERLAY_CHARGING, NULL);
}
```

- [ ] **Step 5: Route around the overlays in the app manager**

In `components/ui/app_manager.c`:
- add `#include "overlays.h"` and `#include "power_watch.h"`;
- in `app_manager_init()`, after `bedtime_init();`, add `power_watch_init();`;
- in `poll_sensors()`, replace the Task 3 `SENSOR_EV_USB_IN` block's body with `power_watch_usb_in();` (keep the `continue;`);
- in `poll_sensors()`, add `&& !overlay_showing()` to both the shake condition (`if (s_current && s_current->on_shake && idle_level() == 0)`) and the tilt condition: the app gets neither while a screen shows (spec §3.13);
- in `handle_button()`, right after `lv_display_trigger_activity(NULL);`:

```c
    if (overlay_showing()) {
        overlay_close(); /* Home and PWR work as always; the battery screen ends first (spec §3.13) */
    }
```

- [ ] **Step 6: Build and test**

```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py -B build-m7 -D SDKCONFIG=build-m7/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" reconfigure build 2>&1 | tail -1
idf.py -B build-drain -D SDKCONFIG=build-drain/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.drain" reconfigure build 2>&1 | tail -1
sh test/run.sh 2>&1 | grep "tests passed"
```

Expected: both builds complete with no new warnings. Host tests `100% tests passed out of 14`.

- [ ] **Step 7: Commit**

```bash
git add components/ui/overlays.c components/ui/include/overlays.h components/ui/power_watch.c \
        components/ui/include/power_watch.h components/ui/app_manager.c components/ui/bedtime.c \
        components/ui/include/bedtime.h components/ui/include/tokens.h
git commit -m "feat: the hungry-cat and eating-cat screens, and the critical-battery sleep"
```

---

### Task 9: Deep sleep and what the restart shows

**Files:**
- Modify: `components/ui/power_watch.c`, `components/ui/app_manager.c`, `components/ui/include/app_manager.h`, `components/ui/bedtime.c`, `components/ui/include/bedtime.h`, `components/ui/include/tokens.h`, `main/app_main.c`

**Interfaces:**
- Consumes:
  - `power_wake_plan()`, `battery_critical()`, `schedule_sleep_allowed()` and `schedule_wake_timer_min()` (Task 2);
  - `deepsleep_start()`, `deepsleep_cause()`, `deepsleep_was_for_battery()`, `clock_secs_until()`, `clock_sync_running()` and `power_state()` (Task 3);
  - `overlay_show()`, `overlay_showing()`, `bedtime_panel_lit()` and the `panel_dark()` helper in `power_watch.c` (Task 8).
- Produces:
  - `void app_manager_boot(void)`
  - `bool bedtime_is_night(void)`
  - `void bedtime_wake(bool dark)`
  - `void bedtime_sunrise_now(void)`

A deep-sleep wake is a restart, so `app_main` runs again, including its boot NTP sync (spec §3.2 "at boot"). After the timer wake, 10 minutes before wake time, that boot sync is the daily one. It also corrects the RTC slow clock's drift before the sunrise.

- [ ] **Step 1: The token**

In `components/ui/include/tokens.h`, in the "Idle and burn-in" block, add:

```c
#define DEEP_SLEEP_AFTER_DARK_MS 600000 /* on battery, 10 minutes after the panel went off (spec §3.13) */
```

- [ ] **Step 2: The countdown in `power_watch.c`**

Add the includes `clock.h`, `schedule.h`, `sdkconfig.h`, `settings.h` and `tokens.h`, then after `#define POWER_WATCH_MS`:

```c
#if CONFIG_CAT_DEV_FAST_SLEEP
#define SLEEP_AFTER_DARK_MS 20000 /* the power and drain dev builds */
#else
#define SLEEP_AFTER_DARK_MS DEEP_SLEEP_AFTER_DARK_MS
#endif
```

Add the state `static bool s_dark; static uint32_t s_dark_since;` and, before `tick()`:

```c
/* Deep sleep on battery, SLEEP_AFTER_DARK_MS after the panel went off (spec §3.13): never on USB power, during a
 * battery screen or an NTP sync, or in the last quarter hour before wake time. The timer wakes the toy before
 * wake time while the clock is valid and there is a night. */
static void sleep_check(bool usb)
{
    if (!panel_dark()) {
        s_dark = false;
        return;
    }
    if (!s_dark) {
        s_dark = true;
        s_dark_since = lv_tick_get();
        return;
    }
    if (lv_tick_elaps(s_dark_since) < SLEEP_AFTER_DARK_MS || usb || overlay_showing() || clock_sync_running()) {
        return;
    }
    const bool valid = clock_valid();
    const uint16_t now = clock_now_min(), bed = settings_bedtime(), wake = settings_wake();
    if (!schedule_sleep_allowed(valid, now, bed, wake)) {
        return;
    }
    const int32_t at = schedule_wake_timer_min(valid, bed, wake);
    deepsleep_start(at < 0 ? 0 : clock_secs_until((uint16_t)at), false);
}
```

In `tick()`, as its last line (after the pending-screen `if`), add `sleep_check(ps.usb);`.

- [ ] **Step 3: Waking into the lock or the sunrise (`bedtime.c`)**

In `components/ui/include/bedtime.h`, add:

```c
/* The schedule's night now (a valid clock, between bedtime and wake time). */
bool bedtime_is_night(void);
/* A deep-sleep wake inside the night (spec §3.13): the lock at once, without the boot animation. It is lit for
 * BEDTIME_TOUCH_ON_MS, as after a touch, or dark (the timer before wake time) unless USB keeps the night light. */
void bedtime_wake(bool dark);
/* The deep-sleep timer woke the toy after wake time: the sunrise at once. */
void bedtime_sunrise_now(void);
```

In `components/ui/bedtime.c`, at the end:

```c
bool bedtime_is_night(void) { return night_now(); }

void bedtime_wake(bool dark)
{
    show_lock(true); /* the start's blank screen goes */
    s_on_until = lv_tick_get() + (dark ? 0 : BEDTIME_TOUCH_ON_MS);
    update_panel();
}

void bedtime_sunrise_now(void)
{
    show_lock(true);
    start_sunrise();
}
```

- [ ] **Step 4: `app_manager_boot()`**

In `components/ui/include/app_manager.h`, add:

```c
/* What a (re)start shows (spec §3.7, §3.13): the boot animation, or, after a deep sleep, the lock, the sunrise or
 * the hungry cat at once. Call once from app_main, LVGL lock held, after app_manager_init(). */
void app_manager_boot(void);
```

In `components/ui/app_manager.c`, add the includes `boot.h`, `deepsleep.h`, `esp_heap_caps.h`, `power.h` and `power_rules.h`, and:

```c
static woke_t woke(void)
{
    switch (deepsleep_cause()) {
    case DEEPSLEEP_BY_PIN: return WOKE_TOUCH;
    case DEEPSLEEP_BY_TIMER: return WOKE_TIMER;
    default: return WOKE_COLD;
    }
}

static void sleep_again(void) { deepsleep_start(0, true); }

void app_manager_boot(void)
{
    const power_state_t ps = power_state(); /* power_init() took a first reading */
    const wake_plan_t plan = power_wake_plan(woke(), bedtime_is_night(), deepsleep_was_for_battery(),
                                             battery_critical(ps.pct, ps.usb));
    static const char *const NAMES[] = {"boot animation", "lock, lit", "lock, dark", "sunrise", "hungry cat"};
    ESP_LOGI(TAG, "start: %s (free psram %u)", NAMES[plan], (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    switch (plan) {
    case WAKE_LOCK_LIT:
    case WAKE_LOCK_DARK:
        bedtime_wake(plan == WAKE_LOCK_DARK);
        s_started = true;
        break;
    case WAKE_SUNRISE:
        bedtime_sunrise_now();
        s_started = true;
        break;
    case WAKE_HUNGRY:
        overlay_show(OVERLAY_HUNGRY, sleep_again); /* the buttons stay ignored: s_started is false */
        break;
    default: boot_run(app_manager_start); break;
    }
}
```

In `main/app_main.c`, replace `boot_run(app_manager_start);` with `app_manager_boot();`.

- [ ] **Step 5: Build and test**

```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py -B build-m7 -D SDKCONFIG=build-m7/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" build 2>&1 | tail -1
for v in power drain soak; do
  idf.py -B build-$v -D SDKCONFIG=build-$v/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.$v" build 2>&1 | tail -1
done
idf.py build 2>&1 | tail -1
sh test/run.sh 2>&1 | grep "tests passed"
```

Expected: every build completes with no new warnings. Host tests `100% tests passed out of 14`.

- [ ] **Step 6: Commit**

```bash
git add components/ui/power_watch.c components/ui/app_manager.c components/ui/include/app_manager.h \
        components/ui/bedtime.c components/ui/include/bedtime.h components/ui/include/tokens.h main/app_main.c
git commit -m "feat: deep sleep on battery, and the lock, sunrise or hungry cat on waking"
```

---

### Task 10: The menu icon breathes

**Files:**
- Modify: `components/ui/launcher.c`, `components/ui/include/tokens.h`

**Interfaces:**
- Consumes: the launcher's `set_scale`, `run_anim`, `s_icon`, `s_icon_next` and `s_idle_level`.
- Produces: nothing new outside the file.

Design: Spec.dc.html "Big color fills (icons, mic, fish) breathe by 2.5 %", and Screens.dc.html `.icon-breathe` (the `breathe` keyframes: scale 1 → 1.025 → 1 over 3 s), on the menu icon unless dimmed. `lv_anim_start()` replaces any animation with the same variable and callback. The swipe's settle and the open zoom use `set_scale` on `s_icon` too, so the breathing must stop before them and restart only after the settle ends.

- [ ] **Step 1: Tokens**

In `components/ui/include/tokens.h`, in the "Idle and burn-in" block:

```c
#define ICON_BREATHE_SCALE 262  /* 1.025 x 256: the menu icon breathes by 2.5 % (Spec: Burn-in) */
#define ICON_BREATHE_MS    1500 /* up, then as long back down: the design's 3 s `breathe` */
```

- [ ] **Step 2: Breathe while the menu is awake and still**

In `components/ui/launcher.c`, after `run_anim()`:

```c
/* The menu icon breathes while the menu is awake and no swipe or open is running (spec §3.13, burn-in). */
static void breathe(bool on)
{
    lv_anim_delete(s_icon, set_scale);
    lv_anim_delete(s_icon_next, set_scale);
    lv_image_set_scale(s_icon, LV_SCALE_NONE);
    if (!on) {
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_icon);
    lv_anim_set_exec_cb(&a, set_scale);
    lv_anim_set_values(&a, LV_SCALE_NONE, ICON_BREATHE_SCALE);
    lv_anim_set_duration(&a, ICON_BREATHE_MS);
    lv_anim_set_reverse_duration(&a, ICON_BREATHE_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static void settle_done(lv_anim_t *a)
{
    (void)a;
    breathe(s_idle_level == 0 && !s_busy);
}
```

Then:
- in `slide_done()`, pass `settle_done` as the last argument of the settle `run_anim(...)` (instead of `NULL`);
- in `go()`, call `breathe(false);` as its first statement after the `s_car.count < 2` early return;
- in `open_current()`, call `breathe(false);` as its first statement;
- in `launcher_show()`, add `breathe(s_idle_level == 0);` after `s_busy = false;`;
- in `launcher_set_idle_level()`, add `breathe(level == 0 && !s_busy);` as its last statement.

`breathe()` is defined after `run_anim()`. `go()`, `open_current()`, `slide_done()`, `launcher_show()` and `launcher_set_idle_level()` all come after it in the file, so no forward declaration is needed. If one comes earlier, declare `static void breathe(bool on);` near the top.

- [ ] **Step 3: Build and test**

```bash
. ~/esp/esp-idf/export.sh >/dev/null 2>&1
idf.py -B build-m7 -D SDKCONFIG=build-m7/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" build 2>&1 | tail -1
idf.py -B build-perf -D SDKCONFIG=build-perf/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.perf" build 2>&1 | tail -1
sh test/run.sh 2>&1 | grep "tests passed"
```

Expected: both builds complete with no new warnings. Host tests `100% tests passed out of 14`.

- [ ] **Step 4: Commit**

```bash
git add components/ui/launcher.c components/ui/include/tokens.h
git commit -m "feat: the menu icon breathes against burn-in"
```

---

### Task 11: Device session, soak and docs (controller)

The controller runs this task itself: it needs the board and the user's eyes and hands. Never guess a result that needs eyes or hands on the board. Ask with AskUserQuestion, and put the full instructions inside the question text. Log every run under `build-logs/m7-*.log` (git-ignored). When grepping a Wi-Fi build's log, leave out the lines matching `connected with|ssid|bssid`.

Deep sleep takes USB down with the chip, so any run that sleeps is logged with the reconnecting logger:
```bash
python tools/serial_log.py /dev/cu.usbmodem101 <seconds> -t > build-logs/m7-<name>.log
```
started right after a `tools/flash_and_log.sh 5 ...` flash.

- [ ] **Step 1: Verify the branch**

```bash
sh test/run.sh 2>&1 | grep "tests passed"
. ~/esp/esp-idf/export.sh >/dev/null 2>&1 && idf.py -B build-m7 -D SDKCONFIG=build-m7/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults" build 2>&1 | tail -1
```

- [ ] **Step 2: The normal build (no Wi-Fi): first run, the parent area, charging**

Flash `build-m7` and log 60 s. Then log the rest of the checks with the reconnecting logger for as long as they take. Ask the user, one AskUserQuestion per group:

- **A. First run** (this board has no "first run done" yet, so the picker shows once):
  - after the boot animation: three cats on round pads. The orange one's pad has the thick ring and the tint;
  - tap Grey: its ring thickens, it looks proud and meows; about 1.5 s later, the menu with the grey cat, and grey paws on the icon's rim;
  - reset the board (`python -m esptool --chip esp32s3 --port /dev/cu.usbmodem101 chip_id`): no picker;
  - the log shows `cat chosen: grey`, `released ... under cat/orange/` and `cat/tux/`, and `free psram after the cat choice`.

  Home on the first-run picker needs the flag gone. Ask the user whether they want that checked. It means erasing the settings partition, which also resets the volume, sleep times and hidden apps. Only on a yes:
  ```bash
  python -m esptool --chip esp32s3 --port /dev/cu.usbmodem101 erase_region 0x9000 0x6000
  ```
  The `nvs` offset and size come from `partitions.csv`: check them there first. Then: Home on the picker goes to the menu with the orange cat, and the next reset shows the picker again.
- **B. The parent area** (Review Focus 1):
  - the two-finger hold opens the home: Volume, Sleep, Apps "6 of 6" and Cat "Grey". The battery percentage sits in the green circle in the middle, and "Home button exits" is at the bottom. Compare the Fredoka text with the design;
  - Apps: tap Draw: it fades to about a third, its label greys and a red crossed eye appears. Hide all but one: the last cannot be hidden. "‹ Parents" shows "1 of 6"; Home shows only that app on the menu. Show them all again;
  - Cat: the picker with Grey's thick ring. Tap Tuxedo: back to the home after about 1.5 s, showing "Tuxedo"; Home: the menu cat and paws are tuxedo;
  - switch the cat twice more (orange, then grey). Record every `free psram after the cat choice` line. The third must be within about 0.5 MB of the first;
  - at night (set Bedtime a few minutes back, as in M6), open the parent area over the lock and switch the cat. Leaving shows the lock with the new cat, and nothing magenta;
  - Max volume and Sleep times look and work as in M6, now in Fredoka.
- **C. Charging** (Review Focus 4):
  - unplug USB; the board runs on the battery, and the log stops;
  - plug USB back in: within about a second, the eating cat with the full bowl and the filling green battery, munching and "Njam njam!". After 4 s, back to where it was;
  - in Yarn roll: plug in while the yarn rolls. The screen shows, the yarn stops, and play goes on afterwards;
  - on a dim menu: plug in. The menu lights, then the screen shows;
  - in the parent area, and on the lock at night: plug in. No screen (on the lock, the night light comes on).

- [ ] **Step 3: Low battery (the `drain` build)**

Flash `build-drain` (USB counts as battery; a fake battery at 12 %, one point less every 10 s). Log 300 s with the reconnecting logger. Ask:
- **D. Low battery** (Review Focus 3, 4):
  - about 20 s after boot (10 %), the hungry cat: sleepy face, empty bowl, the blinking red battery and "Gladna sam…", for 4 s, then back;
  - open an app before 70 s: at 5 % it shows again over the app, and the app is as it was afterwards;
  - Home during the screen closes it and the app;
  - at 2 % (about 100 s) the hungry cat, then the screen goes dark: deep sleep. The log shows `deep sleep: timer 0 s, for the battery`, and the port goes;
  - touch the screen: the hungry cat for 4 s, dark again. The same with BOOT;
  - reset the board to end it.

- [ ] **Step 4: Deep sleep (the `power` build)**

Flash `build-power` (USB counts as battery; deep sleep 20 s after the panel goes off). Log 1800 s with the reconnecting logger. Ask:
- **E. Deep sleep** (Review Focus 2, 5):
  - leave the menu: it dims at 20 s and goes dark at 120 s; about 20 s later, `deep sleep: timer ...`;
  - touch: the boot animation, then the menu (`start: boot animation`). Repeat with BOOT, and with PWR off instead of waiting;
  - Magic paws: draw, Home, let it sleep, wake it, open Magic paws: the drawing is gone (spec §3.13);
  - while it sleeps: is the panel fully dark, and is the speaker silent?
  - **the night:**
    - set Bedtime a few minutes back and Wake up 20 minutes ahead (Sleep times), then Home;
    - the lock goes dark after 30 s and sleeps 20 s later, with the log's timer at about 10 minutes;
    - touch: the lock lit for about 5 s, with no boot animation (`start: lock, lit`), then dark and asleep again;
    - leave it: about 10 minutes before wake time it wakes by itself with the panel dark (`start: lock, dark`) and stays awake. At wake time the sunrise plays, with the chime and "Dobro jutro!".

  Record the timer wake's host time against the planned minute, which gives the drift over the sleep.
- **F. The icon breathes:** on the awake menu the icon swells and shrinks slightly (2.5 %, 3 s). It stops while dimmed, and sliding and opening look as before.

- [ ] **Step 5: FPS, soak, and NTP across a deep sleep**

- **FPS:** build `build-perf` and log the awake menu (breathing) for 20 s. Read the `sysmon` lines: the menu stays at 24 fps or more (spec §7), and the CPU stays near the M6 menu figure.
- **Soak:**
  ```bash
  idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" build
  tools/flash_and_log.sh 420 -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" > build-logs/m7-soak.log
  grep SOAK build-logs/m7-soak.log | tail -3
  ```
  Expected: `SOAK: PASS`. The soak skips the first run.
- **NTP and drift** (only if the user wants it; the Wi-Fi build reads their `sdkconfig.secrets`):
  ```bash
  idf.py -B build-ntppower -D SDKCONFIG=build-ntppower/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.secrets;sdkconfig.power" build
  ```
  Repeat E's night with it. After the timer wake, `NTP: corrected by X ms` is the drift accumulated over the sleep (the log shows the sleep's timer). Record ms per hour.

- [ ] **Step 6: Record and commit**

Write "System and parent (M7) — <date>" in `docs/device-checks.md`: a results table for A-F, the soak, FPS, the drift, and anything user-reported but not in the log (say so). Update `README.md`:
- the first run (the picker at first boot; erasing the settings brings it back);
- deep sleep on battery;
- the charging and low-battery screens.

Commit: `docs: M7 device results`.

Anything that fails goes to `superpowers:systematic-debugging` before any fix, and gets its own `fix:` commit and a re-check.
