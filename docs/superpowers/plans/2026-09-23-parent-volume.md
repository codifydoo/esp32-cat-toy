# Parent Volume Setting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A parent can hold two fingers on the menu for 5 s to open a "Max volume" screen, change the volume cap in 10 % steps, and press Home to go back. This is the first slice of the M7 parent area.

**Architecture:**
- **Detecting two fingers.** LVGL keeps its single pointer. Our display setup registers touch with the adapter's `custom_touch_read` hook, so `touch_raw` records how many fingers are down on every read.
- **The hold rule.** A host-tested classifier in `logic` decides when two steady fingers have been held 5 s.
- **Opening the screen.** The menu polls the classifier and draws the design's progress ring. On success it opens `PARENT_APP`, an `app_t` that is not in the carousel registry, so the app manager's Home, 60 s timeout and fades apply unchanged.

**Tech Stack:** ESP-IDF v5.5.5 (C11), LVGL 9.4 (built-in Montserrat fonts), `waveshare/esp32_s3_touch_amoled_1_75c` 3.0.0, `espressif/esp_lvgl_adapter` 0.6.4, `esp_lcd_touch` 1.2.1 (CST9217), CMake + Unity host tests.

**Spec:** `docs/superpowers/specs/2026-09-22-cat-toy-design.md` (§3.1 Parent entry; the design files are the UI source of truth: `design/claude-design/Screen.dc.html` views `menu` with `hold` and `p-volume`, and `Screens.dc.html` "Menu · parent hold" and "Parent · max volume").

## Global Constraints

- **Toolchain:** ESP-IDF v5.5.5, target `esp32s3`. Run every `idf.py` command in a shell that has sourced `~/esp/esp-idf/export.sh`. The port is `/dev/cu.usbmodem101`.
- **Parent entry (spec §3.1), verbatim:**
  - "two fingers held on the screen for **5 s**, anywhere on the screen, with neither point moving more than **40 px**";
  - "A one-finger hold does nothing special";
  - "The progress ring from the design appears around the cat after **1 s** of a valid two-finger hold";
  - two-finger hold "reset when a finger lifts".
- **Hold ring (design):**
  - circle centered at (233, 112), radius 92, stroke 6 px;
  - white at 55 % opacity, round caps;
  - fills clockwise from 12 o'clock.
- **Max volume screen (design `p-volume`):**
  - label "Max volume" at (133, 98), width 200, 20 px, color `#A9A5BF`;
  - 270° arc from 135°, radius 110, centered at (233, 254), 22 px wide, round caps; track `#2A2838`, fill `#FFB547`;
  - value "N%" at (153, 222), width 160, 54 px;
  - "−" and "+" buttons 84 px round at (106, 332) and (276, 332): fill `#1E1D2A`, 3 px border `#3A3850`, text `#F2EFFA` 40 px;
  - steps of 10 %.
- **Fonts:** LVGL's built-in Montserrat, not the design's Fredoka, for this slice. Fredoka needs a font download plus `lv_font_conv`. Sizes: 16 (hint), 20 (label), 40 (−/+), 48 (value, nearest to 54).
- **Volume range:** 10–100 %. The floor keeps the toy from going silent by accident, which would look broken to a 5-year-old. The default stays 100 % (`SETTINGS_VOLUME_DEFAULT`).
- **Text:** "the parent area is the only place with text". Child screens stay text-free.
- **Sounds:** the parent area plays "UI clicks only". Each −/+ press plays `SND_BLIP_DOWN` / `SND_BLIP_UP` at the new level.
- **Home:** the Home button returns to the menu from every screen within 300 ms (spec success criterion 2). The parent app gets this from `app_manager`.
- **LVGL access:** LVGL objects are touched only on the LVGL task or under `display_lock()`. Brightness changes only on the LVGL task. `touch_raw_read()` runs on the LVGL task, because the adapter calls it from the indev read.
- **`app_t` contract** (`components/ui/include/app.h`): after `destroy()`, every callback the app registered must no-op.
- **Host tests:** `sh test/run.sh` must pass before any commit that touches `components/logic/`.
- **Commits:** `feat:`, `fix:`, `test:`, `chore:`, `docs:` prefixes, no attribution trailers.
- **Human steps** are marked **Human step**. Ask the user and wait. Never guess a result that needs eyes or hands on the board.

## Review Focus

1. **A child's two-finger or palm touch** that is shorter than 5 s, or that moves, must never open the parent area. Task 1's tests pin short, moving and one-finger holds.
2. **The release after a completed hold** must not also count as a tap on the cat or the icon. Otherwise it would say the app name or open an app behind the parent screen. Task 4 swallows the touch (`lv_indev_wait_release`) as soon as two fingers are seen, and Step 6 checks it on the device.
3. **Fingers reported in the other order between reads.** The CST9217 driver leaves `track_id` at 0. This must not look like a big jump that cancels the hold. Task 1 pins swapped order.
4. **Volume at the ends and odd stored values.** "−" at 10 % stays at 10 %, "+" at 100 % stays at 100 %, and a stored 65 steps to 70 or 60. Task 3 pins these.
5. **Lifting both fingers** must drop the count to 0 so no later hold fires from a stale two-finger reading. Task 1 pins restart-after-lift. Task 2 Step 5 checks on the device that the diag log returns to `touch points=0`.

---

### Task 1: Two-finger hold classifier

**Files:**
- Modify: `components/logic/include/gestures.h`
- Modify: `components/logic/gestures.c`
- Test: `test/test_gestures.c`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  ```c
  #define GESTURE_HOLD_MS      5000u
  #define GESTURE_HOLD_SHOW_MS 1000u
  #define GESTURE_HOLD_MAX_PX  40
  typedef enum { HOLD_IDLE = 0, HOLD_WAITING, HOLD_PROGRESS, HOLD_DONE } hold_state_t;
  typedef struct { bool active; bool spent; int16_t x0[2], y0[2]; uint32_t t0_ms; } hold_track_t;
  void hold_reset(hold_track_t *h);
  hold_state_t hold_update(hold_track_t *h, uint8_t n, const int16_t x[2], const int16_t y[2],
                           uint32_t t_ms, uint16_t *progress);
  ```

- [ ] **Step 1: Write the failing tests**

Append these tests to `test/test_gestures.c` above `int main(void)`:

```c
static hold_state_t two(hold_track_t *h, int16_t ax, int16_t ay, int16_t bx, int16_t by, uint32_t t,
                        uint16_t *p)
{
    const int16_t x[2] = {ax, bx}, y[2] = {ay, by};
    return hold_update(h, 2, x, y, t, p);
}

static hold_state_t lift(hold_track_t *h, uint32_t t)
{
    const int16_t z[2] = {0, 0};
    return hold_update(h, 0, z, z, t, NULL);
}

static void test_two_steady_fingers_fire_once_at_five_seconds(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p = 0;
    TEST_ASSERT_EQUAL(HOLD_WAITING, two(&h, 100, 200, 300, 200, 0, &p));
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 100, 200, 300, 200, 4999, &p));
    TEST_ASSERT_EQUAL(HOLD_DONE, two(&h, 100, 200, 300, 200, 5000, &p));
    TEST_ASSERT_EQUAL(HOLD_IDLE, two(&h, 100, 200, 300, 200, 9000, &p)); /* no second fire */
}

static void test_ring_shows_after_one_second_and_fills_over_four(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p = 77;
    two(&h, 100, 200, 300, 200, 0, &p);
    TEST_ASSERT_EQUAL(HOLD_WAITING, two(&h, 100, 200, 300, 200, 999, &p));
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 100, 200, 300, 200, 1000, &p));
    TEST_ASSERT_EQUAL_UINT16(0, p);
    two(&h, 100, 200, 300, 200, 3000, &p);
    TEST_ASSERT_EQUAL_UINT16(500, p);
    two(&h, 100, 200, 300, 200, 4999, &p);
    TEST_ASSERT_EQUAL_UINT16(999, p);
}

static void test_moving_forty_px_is_allowed(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p;
    two(&h, 100, 200, 300, 200, 0, &p);
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 124, 232, 300, 200, 2000, &p)); /* 24-32-40 triangle */
}

static void test_moving_more_than_forty_px_cancels_until_a_finger_lifts(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p;
    two(&h, 100, 200, 300, 200, 0, &p);
    TEST_ASSERT_EQUAL(HOLD_IDLE, two(&h, 100, 200, 341, 200, 2000, &p));
    TEST_ASSERT_EQUAL(HOLD_IDLE, two(&h, 100, 200, 341, 200, 8000, &p)); /* still cancelled */
    TEST_ASSERT_EQUAL(HOLD_IDLE, lift(&h, 8100));
    TEST_ASSERT_EQUAL(HOLD_WAITING, two(&h, 100, 200, 341, 200, 8200, &p)); /* fresh start */
}

static void test_lifting_a_finger_restarts_from_zero(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p;
    two(&h, 100, 200, 300, 200, 0, &p);
    two(&h, 100, 200, 300, 200, 4000, &p);
    const int16_t x[2] = {100, 0}, y[2] = {200, 0};
    TEST_ASSERT_EQUAL(HOLD_IDLE, hold_update(&h, 1, x, y, 4100, &p));
    TEST_ASSERT_EQUAL(HOLD_WAITING, two(&h, 100, 200, 300, 200, 4200, &p));
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 100, 200, 300, 200, 9100, &p)); /* 4.9 s since restart */
    TEST_ASSERT_EQUAL(HOLD_DONE, two(&h, 100, 200, 300, 200, 9200, &p));
}

static void test_one_finger_never_fires(void)
{
    hold_track_t h;
    hold_reset(&h);
    const int16_t x[2] = {233, 0}, y[2] = {100, 0};
    for (uint32_t t = 0; t <= 20000; t += 50) {
        TEST_ASSERT_EQUAL(HOLD_IDLE, hold_update(&h, 1, x, y, t, NULL));
    }
}

static void test_fingers_reported_in_swapped_order_are_not_a_move(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p;
    two(&h, 100, 200, 300, 200, 0, &p);
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 302, 201, 101, 199, 2000, &p));
    TEST_ASSERT_EQUAL(HOLD_DONE, two(&h, 100, 200, 300, 200, 5000, &p));
}

static void test_hold_survives_the_millisecond_counter_wrapping(void)
{
    hold_track_t h;
    hold_reset(&h);
    uint16_t p;
    const uint32_t t0 = UINT32_MAX - 1000u;
    two(&h, 100, 200, 300, 200, t0, &p);
    TEST_ASSERT_EQUAL(HOLD_PROGRESS, two(&h, 100, 200, 300, 200, t0 + 4999u, &p));
    TEST_ASSERT_EQUAL(HOLD_DONE, two(&h, 100, 200, 300, 200, t0 + 5000u, &p));
}
```

In the same file, add these lines to `main()` after the existing `RUN_TEST` calls:

```c
    RUN_TEST(test_two_steady_fingers_fire_once_at_five_seconds);
    RUN_TEST(test_ring_shows_after_one_second_and_fills_over_four);
    RUN_TEST(test_moving_forty_px_is_allowed);
    RUN_TEST(test_moving_more_than_forty_px_cancels_until_a_finger_lifts);
    RUN_TEST(test_lifting_a_finger_restarts_from_zero);
    RUN_TEST(test_one_finger_never_fires);
    RUN_TEST(test_fingers_reported_in_swapped_order_are_not_a_move);
    RUN_TEST(test_hold_survives_the_millisecond_counter_wrapping);
```

- [ ] **Step 2: Run the tests to verify they fail**

Run: `sh test/run.sh`
Expected: the build fails with `call to undeclared function 'hold_update'` (and `hold_reset`, `HOLD_WAITING`).

- [ ] **Step 3: Implement the classifier**

Append to `components/logic/include/gestures.h`:

```c
/* Parent entry (spec 3.1): two fingers held still for 5 s. */
#define GESTURE_HOLD_MS      5000u
#define GESTURE_HOLD_SHOW_MS 1000u /* the progress ring appears after this */
#define GESTURE_HOLD_MAX_PX  40    /* per point, from where it went down */

typedef enum {
    HOLD_IDLE = 0, /* fewer than two fingers, or this hold was cancelled or already fired */
    HOLD_WAITING,  /* two steady fingers, under 1 s: show nothing yet */
    HOLD_PROGRESS, /* ring visible: *progress runs 0..1000 over the remaining 4 s */
    HOLD_DONE,     /* returned once, at 5 s */
} hold_state_t;

typedef struct {
    bool active;  /* two fingers down and steady since t0_ms */
    bool spent;   /* fired or cancelled: stays idle until fewer than two fingers are down */
    int16_t x0[2], y0[2];
    uint32_t t0_ms;
} hold_track_t;

void hold_reset(hold_track_t *h);
/* Feed one touch sample with n points (only the first two are read). Finger order may change
 * between samples; each point is matched to the nearer start point. */
hold_state_t hold_update(hold_track_t *h, uint8_t n, const int16_t x[2], const int16_t y[2],
                         uint32_t t_ms, uint16_t *progress);
```

Append to `components/logic/gestures.c`:

```c
void hold_reset(hold_track_t *h)
{
    *h = (hold_track_t){0};
}

static uint32_t dist2(int16_t ax, int16_t ay, int16_t bx, int16_t by)
{
    int32_t dx = ax - bx;
    int32_t dy = ay - by;
    return (uint32_t)(dx * dx + dy * dy);
}

static uint32_t max2(uint32_t a, uint32_t b) { return a > b ? a : b; }

hold_state_t hold_update(hold_track_t *h, uint8_t n, const int16_t x[2], const int16_t y[2],
                         uint32_t t_ms, uint16_t *progress)
{
    if (n < 2) {
        h->active = false;
        h->spent = false; /* a lifted finger ends this hold; the next two-finger touch starts fresh */
        return HOLD_IDLE;
    }
    if (h->spent) {
        return HOLD_IDLE;
    }
    if (!h->active) {
        h->active = true;
        for (int i = 0; i < 2; i++) {
            h->x0[i] = x[i];
            h->y0[i] = y[i];
        }
        h->t0_ms = t_ms;
        return HOLD_WAITING;
    }
    /* The controller may report the two fingers in either order, so use the pairing that moved less. */
    uint32_t same = max2(dist2(x[0], y[0], h->x0[0], h->y0[0]), dist2(x[1], y[1], h->x0[1], h->y0[1]));
    uint32_t swapped = max2(dist2(x[0], y[0], h->x0[1], h->y0[1]), dist2(x[1], y[1], h->x0[0], h->y0[0]));
    uint32_t moved = same < swapped ? same : swapped;
    if (moved > (uint32_t)(GESTURE_HOLD_MAX_PX * GESTURE_HOLD_MAX_PX)) {
        h->active = false;
        h->spent = true;
        return HOLD_IDLE;
    }
    uint32_t elapsed = t_ms - h->t0_ms; /* unsigned: correct across the counter wrapping */
    if (elapsed >= GESTURE_HOLD_MS) {
        h->active = false;
        h->spent = true;
        return HOLD_DONE;
    }
    if (elapsed < GESTURE_HOLD_SHOW_MS) {
        return HOLD_WAITING;
    }
    if (progress) {
        *progress = (uint16_t)((elapsed - GESTURE_HOLD_SHOW_MS) * 1000u / (GESTURE_HOLD_MS - GESTURE_HOLD_SHOW_MS));
    }
    return HOLD_PROGRESS;
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `sh test/run.sh`
Expected: `100% tests passed out of 6`.

- [ ] **Step 5: Commit**

```bash
git add components/logic/include/gestures.h components/logic/gestures.c test/test_gestures.c
git commit -m "feat: two-finger hold classifier for the parent entry"
```

---

### Task 2: Two-point touch reads

**Files:**
- Create: `components/platform/include/touch_raw.h`
- Create: `components/platform/touch_raw.c`
- Modify: `components/platform/display.c` (whole file)
- Modify: `main/diag.c` (the `tick()` timer)

**Interfaces:**
- Consumes: BSP `bsp_display_new()`, `bsp_touch_new()` (public in `waveshare__esp32_s3_touch_amoled_1_75c`); adapter `esp_lv_adapter_init/register_display/register_touch/start`.
- Produces:
  ```c
  typedef struct { uint8_t count; int16_t x[2]; int16_t y[2]; } touch_raw_t;
  touch_raw_t touch_raw_get(void); /* LVGL task only */
  ```
  `display_init()`, `display_set_brightness()`, `display_brightness()`, `display_lock()` and `display_unlock()` keep their signatures in `display.h`.

Why: `bsp_display_start_with_config()` registers touch with the adapter's single-pointer default, and the BSP keeps its touch handle private. So `display_init()` now composes the same public steps itself and adds the `custom_touch_read` hook. The BSP's `bsp_display_brightness_set()` only works after its own start function (it checks a private panel handle), so brightness is sent directly: the CO5300 command 0x51, exactly as the BSP does it.

- [ ] **Step 1: Write `touch_raw`**

`components/platform/include/touch_raw.h`:

```c
#pragma once
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_touch.h"

/* The latest touch reading with up to two points, for the parent gesture. LVGL itself still sees
 * one pointer (the first point). */
typedef struct {
    uint8_t count; /* 0, 1 or 2 fingers */
    int16_t x[2];
    int16_t y[2];
} touch_raw_t;

/* The adapter's custom_touch_read hook (runs on the LVGL task during the indev read). */
esp_err_t touch_raw_read(esp_lcd_touch_handle_t tp, esp_lcd_touch_point_data_t *points, uint8_t *count,
                         uint8_t max_count, void *user_ctx);
/* LVGL task only. The value holds until the next read, which the controller triggers on every
 * change, including the last finger lifting. */
touch_raw_t touch_raw_get(void);
```

`components/platform/touch_raw.c`:

```c
#include "touch_raw.h"

static touch_raw_t s_last;

esp_err_t touch_raw_read(esp_lcd_touch_handle_t tp, esp_lcd_touch_point_data_t *points, uint8_t *count,
                         uint8_t max_count, void *user_ctx)
{
    (void)user_ctx;
    esp_lcd_touch_point_data_t all[2] = {0};
    uint8_t n = 0;
    esp_lcd_touch_read_data(tp); /* same as the adapter's own read: a failed read leaves no points */
    esp_err_t err = esp_lcd_touch_get_data(tp, all, &n, 2);
    if (err != ESP_OK) {
        n = 0;
    }
    s_last.count = n;
    for (uint8_t i = 0; i < n; i++) {
        s_last.x[i] = (int16_t)all[i].x;
        s_last.y[i] = (int16_t)all[i].y;
    }
    uint8_t give = n < max_count ? n : max_count;
    for (uint8_t i = 0; i < give; i++) {
        points[i] = all[i];
    }
    *count = give;
    return err;
}

touch_raw_t touch_raw_get(void) { return s_last; }
```

- [ ] **Step 2: Replace `components/platform/display.c`**

```c
#include "display.h"
#include "bsp/display.h"
#include "bsp/esp-bsp.h"
#include "bsp/touch.h"
#include "esp_check.h"
#include "esp_lcd_panel_io.h"
#include "touch_raw.h"

static const char *TAG = "display";
static esp_lcd_panel_io_handle_t s_io;
static uint8_t s_brightness = 100;

/* The CO5300 needs even start and odd end coordinates (same rounding as the BSP). */
static void rounder_cb(lv_event_t *e)
{
    lv_area_t *a = (lv_area_t *)lv_event_get_param(e);
    a->x1 = (a->x1 >> 1) << 1;
    a->y1 = (a->y1 >> 1) << 1;
    a->x2 = ((a->x2 >> 1) << 1) + 1;
    a->y2 = ((a->y2 >> 1) << 1) + 1;
}

/* bsp_display_start_with_config() with one change: touch goes through touch_raw_read() so the
 * parent gesture can see two fingers. */
esp_err_t display_init(void)
{
    esp_lv_adapter_config_t adapter = ESP_LV_ADAPTER_DEFAULT_CONFIG();
    adapter.task_core_id = 1; /* audio and sensors run on core 0 */
    ESP_RETURN_ON_ERROR(esp_lv_adapter_init(&adapter), TAG, "adapter init");

    const bsp_display_config_t panel_cfg = {
        .max_transfer_sz = BSP_LCD_H_RES * BSP_LCD_V_RES * BSP_LCD_BITS_PER_PIXEL / 8,
    };
    esp_lcd_panel_handle_t panel = NULL;
    ESP_RETURN_ON_ERROR(bsp_display_new(&panel_cfg, &panel, &s_io), TAG, "panel");
    const esp_lv_adapter_display_config_t disp_cfg = {
        .panel = panel,
        .panel_io = s_io,
        .profile = {
            .interface = ESP_LV_ADAPTER_PANEL_IF_OTHER,
            .rotation = ESP_LV_ADAPTER_ROTATE_0,
            .hor_res = BSP_LCD_H_RES,
            .ver_res = BSP_LCD_V_RES,
            .buffer_height = 50,
            .use_psram = true,
            .enable_ppa_accel = false,
            .require_double_buffer = true,
        },
        .tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_NONE,
    };
    lv_display_t *disp = esp_lv_adapter_register_display(&disp_cfg);
    ESP_RETURN_ON_FALSE(disp, ESP_FAIL, TAG, "register display");
    lv_display_add_event_cb(disp, rounder_cb, LV_EVENT_INVALIDATE_AREA, NULL);

    const bsp_display_cfg_t bsp_touch_cfg = { /* bsp_touch_new() reads only touch_flags */
        .touch_flags = {.swap_xy = 0, .mirror_x = 1, .mirror_y = 1}, /* verified in the Task 13 touch check */
    };
    esp_lcd_touch_handle_t tp = NULL;
    ESP_RETURN_ON_ERROR(bsp_touch_new(&bsp_touch_cfg, &tp), TAG, "touch");
    esp_lv_adapter_touch_config_t touch = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(disp, tp);
    touch.callbacks.custom_touch_read = touch_raw_read;
    ESP_RETURN_ON_FALSE(esp_lv_adapter_register_touch(&touch), ESP_FAIL, TAG, "register touch");

    display_set_brightness(100);
    ESP_RETURN_ON_ERROR(esp_lv_adapter_start(), TAG, "adapter start");
    return ESP_OK;
}

void display_set_brightness(uint8_t pct)
{
    if (pct > 100) {
        pct = 100;
    }
    /* CO5300 "write display brightness" (0x51) in the QSPI command format, as the BSP sends it. */
    uint8_t level = (uint8_t)(pct * 255 / 100);
    if (s_io && esp_lcd_panel_io_tx_param(s_io, (0x02u << 24) | (0x51u << 8), &level, 1) == ESP_OK) {
        s_brightness = pct;
    }
}

uint8_t display_brightness(void) { return s_brightness; }
bool display_lock(int32_t timeout_ms) { return esp_lv_adapter_lock(timeout_ms) == ESP_OK; }
void display_unlock(void) { esp_lv_adapter_unlock(); }
```

- [ ] **Step 3: Show the finger count in the diagnostics build**

In `main/diag.c`, add `#include "touch_raw.h"` with the other includes. Add a file-scope variable next to the other statics:

```c
static uint8_t s_points; /* last logged finger count */
```

Add this at the top of `tick()`, after `(void)t;`:

```c
    touch_raw_t raw = touch_raw_get();
    if (raw.count != s_points) {
        s_points = raw.count;
        ESP_LOGI(TAG, "touch points=%u", (unsigned)raw.count);
    }
```

- [ ] **Step 4: Build all three variants**

Run:

```bash
idf.py build
idf.py -B build-diag -D SDKCONFIG=build-diag/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.diag" build
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" build
```

Expected: `Project build complete.` for each, with no warnings from `components/` or `main/`. If a header is missing (for example `esp_lcd_panel_io.h` or `esp_lcd_touch.h`), add the owning component (`esp_lcd`, `espressif__esp_lcd_touch`) to `PRIV_REQUIRES` or `REQUIRES` in `components/platform/CMakeLists.txt`.

- [ ] **Step 5: Human step. Two fingers in the diagnostics build**

Flash the diagnostics build and keep the log running:

```bash
idf.py -p /dev/cu.usbmodem101 -B build-diag flash
python tools/serial_log.py /dev/cu.usbmodem101 60
```

Ask the user to:
1. touch with one finger;
2. add a second finger and hold both still for 5 s;
3. lift both.

Expected in the log:
- `touch points=1`, then `touch points=2`, then `touch points=0`;
- the touch x/y lines still follow the finger;
- the screen looks as before.

- [ ] **Step 6: Human step. Nothing else changed**

Flash the normal build: `idf.py -p /dev/cu.usbmodem101 flash`. Ask the user to swipe the menu, open and close an app, and leave the menu until it dims at about 20 s, then tap to wake. Expected: swipes and taps work as before, and the dim and wake brightness change as before.

- [ ] **Step 7: Commit**

```bash
git add components/platform/include/touch_raw.h components/platform/touch_raw.c components/platform/display.c main/diag.c
git commit -m "feat: read two touch points for the parent gesture"
```

---

### Task 3: Max volume screen

**Files:**
- Create: `components/logic/include/volume.h`
- Create: `components/logic/volume.c`
- Test: `test/test_volume.c`
- Create: `components/ui/include/parent.h`
- Create: `components/ui/parent.c`
- Modify: `components/ui/include/tokens.h` (append the parent block)
- Modify: `components/ui/include/ui_util.h`, `components/ui/ui_util.c` (add `ui_arc`)
- Modify: `sdkconfig.defaults` (fonts)

**Interfaces:**
- Consumes: `settings_volume_cap()`, `settings_set_volume_cap(uint8_t)` (`settings.h`); `audio_set_volume(uint8_t)`, `audio_play(sound_id_t)` (`audio.h`); `app_t` (`app.h`).
- Produces:
  ```c
  #define VOLUME_MIN 10
  #define VOLUME_MAX 100
  uint8_t volume_step(uint8_t current, int dir);          /* logic/volume.h */
  extern const app_t PARENT_APP;                          /* ui/parent.h */
  lv_obj_t *ui_arc(lv_obj_t *parent, int32_t cx, int32_t cy, int32_t r, int32_t width); /* ui_util.h */
  ```

- [ ] **Step 1: Write the failing test**

`test/test_volume.c`:

```c
#include "unity.h"
#include "volume.h"

void setUp(void) {}
void tearDown(void) {}

static void test_steps_by_ten(void)
{
    TEST_ASSERT_EQUAL_UINT8(70, volume_step(60, +1));
    TEST_ASSERT_EQUAL_UINT8(50, volume_step(60, -1));
}

static void test_stays_within_ten_and_a_hundred(void)
{
    TEST_ASSERT_EQUAL_UINT8(100, volume_step(100, +1));
    TEST_ASSERT_EQUAL_UINT8(10, volume_step(10, -1));
}

static void test_odd_values_snap_to_the_next_step(void)
{
    TEST_ASSERT_EQUAL_UINT8(70, volume_step(65, +1));
    TEST_ASSERT_EQUAL_UINT8(60, volume_step(65, -1));
}

static void test_out_of_range_values_come_back_in(void)
{
    TEST_ASSERT_EQUAL_UINT8(10, volume_step(0, +1));
    TEST_ASSERT_EQUAL_UINT8(10, volume_step(5, -1));
    TEST_ASSERT_EQUAL_UINT8(100, volume_step(255, +1));
    TEST_ASSERT_EQUAL_UINT8(100, volume_step(255, -1)); /* out-of-range input clamps to 100 first */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_steps_by_ten);
    RUN_TEST(test_stays_within_ten_and_a_hundred);
    RUN_TEST(test_odd_values_snap_to_the_next_step);
    RUN_TEST(test_out_of_range_values_come_back_in);
    return UNITY_END();
}
```

The last assertion pins the rule for a corrupt stored value above 100: it is clamped into range first, so `255` becomes `100` for both directions, and the parent presses "−" again to go lower.

- [ ] **Step 2: Run the test to verify it fails**

Run: `sh test/run.sh`
Expected: the build fails with `'volume.h' file not found`.

- [ ] **Step 3: Implement `volume_step`**

`components/logic/include/volume.h`:

```c
#pragma once
#include <stdint.h>

#define VOLUME_MIN  10 /* never fully silent: a silent toy looks broken to her */
#define VOLUME_MAX  100
#define VOLUME_STEP 10

/* The parent screen's -/+ step: the next multiple of 10 below (dir < 0) or above (dir > 0),
 * kept within VOLUME_MIN..VOLUME_MAX. A value outside that range is clamped into it instead. */
uint8_t volume_step(uint8_t current, int dir);
```

`components/logic/volume.c`:

```c
#include "volume.h"

uint8_t volume_step(uint8_t current, int dir)
{
    if (current < VOLUME_MIN) {
        return VOLUME_MIN;
    }
    if (current > VOLUME_MAX) {
        return VOLUME_MAX;
    }
    int v = current;
    if (dir > 0) {
        v = (v / VOLUME_STEP + 1) * VOLUME_STEP;
    } else if (dir < 0) {
        v = ((v + VOLUME_STEP - 1) / VOLUME_STEP - 1) * VOLUME_STEP;
    }
    return (uint8_t)(v < VOLUME_MIN ? VOLUME_MIN : v > VOLUME_MAX ? VOLUME_MAX : v);
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `sh test/run.sh`
Expected: `100% tests passed out of 7`.

- [ ] **Step 5: Tokens, fonts and the arc helper**

Append to `components/ui/include/tokens.h`:

```c
/* Parent area (design: "Parent area" colors; "Parent · max volume") */
#define COLOR_P_SURFACE 0x1E1D2A
#define COLOR_P_BORDER  0x3A3850
#define COLOR_P_TEXT    0xF2EFFA
#define COLOR_P_MUTED   0xA9A5BF
#define COLOR_P_TRACK   0x2A2838
#define COLOR_P_ARC     0xFFB547
#define PARENT_ARC_CX   233
#define PARENT_ARC_CY   254
#define PARENT_ARC_R    110
#define PARENT_ARC_W    22
#define PARENT_BTN_SIZE 84
#define PARENT_BTN_Y    332
#define PARENT_MINUS_X  106
#define PARENT_PLUS_X   276

/* Parent entry ring (design: "Menu · parent hold") */
#define HOLD_RING_CX  233
#define HOLD_RING_CY  112
#define HOLD_RING_R   92
#define HOLD_RING_W   6
#define HOLD_RING_OPA 140 /* 55 % */
#define HOLD_POLL_MS  50
```

Append to `sdkconfig.defaults`:

```
# Parent area text (built-in Montserrat; the design's Fredoka comes with the full parent area)
CONFIG_LV_FONT_MONTSERRAT_16=y
CONFIG_LV_FONT_MONTSERRAT_20=y
CONFIG_LV_FONT_MONTSERRAT_40=y
CONFIG_LV_FONT_MONTSERRAT_48=y
```

An existing `sdkconfig` keeps "is not set" for these, so move the generated configs aside (do not delete them) before the next build. `build-sdkconfig-backup/` is git-ignored by the `build-*/` rule:

```bash
mkdir -p build-sdkconfig-backup
for f in sdkconfig build-*/sdkconfig; do [ -f "$f" ] && mv "$f" "build-sdkconfig-backup/$(echo "$f" | tr / _)"; done
```

Add to `components/ui/include/ui_util.h`:

```c
/* A non-clickable arc without a knob, centered on (cx, cy) with the given radius to the middle of
 * the stroke; main (track) and indicator both get `width` and round caps. */
lv_obj_t *ui_arc(lv_obj_t *parent, int32_t cx, int32_t cy, int32_t r, int32_t width);
```

Add to `components/ui/ui_util.c`:

```c
lv_obj_t *ui_arc(lv_obj_t *parent, int32_t cx, int32_t cy, int32_t r, int32_t width)
{
    lv_obj_t *a = lv_arc_create(parent);
    int32_t d = 2 * r + width;
    lv_obj_set_size(a, d, d);
    lv_obj_set_pos(a, cx - d / 2, cy - d / 2);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a, 0, LV_PART_KNOB);
    lv_obj_set_style_pad_all(a, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, width, LV_PART_MAIN);
    lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    return a;
}
```

- [ ] **Step 6: Write the parent app**

`components/ui/include/parent.h`:

```c
#pragma once
#include "app.h"

/* The parent area (spec 3.1). Opened from the menu by a two-finger hold, never from the carousel,
 * so it is not in APPS. This first slice is the Max volume screen. */
extern const app_t PARENT_APP;
```

`components/ui/parent.c`:

```c
#include "parent.h"
#include "audio.h"
#include "settings.h"
#include "tokens.h"
#include "ui_util.h"
#include "volume.h"

static lv_obj_t *s_arc, *s_value; /* NULL once destroyed: callbacks check s_arc */

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

static void show_volume(void)
{
    uint8_t v = settings_volume_cap();
    lv_arc_set_value(s_arc, v);
    lv_label_set_text_fmt(s_value, "%u%%", (unsigned)v);
}

static void step(int dir)
{
    if (!s_arc) {
        return; /* destroy() already ran */
    }
    uint8_t v = volume_step(settings_volume_cap(), dir);
    settings_set_volume_cap(v);
    audio_set_volume(v);
    audio_play(dir > 0 ? SND_BLIP_UP : SND_BLIP_DOWN); /* a click at the new level */
    show_volume();
}

static void on_minus(lv_event_t *e) { (void)e; step(-1); }
static void on_plus(lv_event_t *e) { (void)e; step(+1); }

static void round_button(lv_obj_t *parent, int32_t x, const char *sym, lv_event_cb_t cb)
{
    lv_obj_t *b = lv_button_create(parent);
    lv_obj_set_pos(b, x, PARENT_BTN_Y);
    lv_obj_set_size(b, PARENT_BTN_SIZE, PARENT_BTN_SIZE);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_P_SURFACE), 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(COLOR_P_BORDER), LV_STATE_PRESSED);
    lv_obj_set_style_border_width(b, 3, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(COLOR_P_BORDER), 0);
    lv_obj_set_style_shadow_width(b, 0, 0);
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l = lv_label_create(b);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(COLOR_P_TEXT), 0);
    lv_label_set_text(l, sym);
    lv_obj_center(l);
}

static void parent_create(lv_obj_t *scr)
{
    /* The design's "< Parents" button leads to the tile grid, which this slice doesn't have yet. */
    text(scr, &lv_font_montserrat_16, COLOR_P_MUTED, 113, 52, 240, "Home button exits");
    text(scr, &lv_font_montserrat_20, COLOR_P_MUTED, 133, 98, 200, "Max volume");
    s_arc = ui_arc(scr, PARENT_ARC_CX, PARENT_ARC_CY, PARENT_ARC_R, PARENT_ARC_W);
    lv_arc_set_bg_angles(s_arc, 135, 45); /* 270 degrees, gap at the bottom */
    lv_arc_set_range(s_arc, 0, VOLUME_MAX);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_arc, lv_color_hex(COLOR_P_ARC), LV_PART_INDICATOR);
    s_value = text(scr, &lv_font_montserrat_48, COLOR_P_TEXT, 153, 226, 160, "");
    round_button(scr, PARENT_MINUS_X, "-", on_minus);
    round_button(scr, PARENT_PLUS_X, "+", on_plus);
    show_volume();
}

static void parent_destroy(void)
{
    s_arc = NULL;
    s_value = NULL;
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

- [ ] **Step 7: Build**

Run: `idf.py build`
Expected: `Project build complete.` with no warnings from `components/` or `main/`. (`parent.c` is compiled but not reachable until Task 4.)

- [ ] **Step 8: Commit**

```bash
git add components/logic/include/volume.h components/logic/volume.c test/test_volume.c \
        components/ui/include/parent.h components/ui/parent.c components/ui/include/tokens.h \
        components/ui/include/ui_util.h components/ui/ui_util.c sdkconfig.defaults
git commit -m "feat: parent Max volume screen"
```

---

### Task 4: Open the parent area from the menu

**Files:**
- Modify: `components/ui/launcher.c`
- Modify: `docs/superpowers/specs/2026-09-22-cat-toy-design.md` (§3.1 note)
- Modify: `docs/device-checks.md` (append results)

**Interfaces:**
- Consumes: `hold_reset`, `hold_update`, `hold_state_t`, `GESTURE_*` (Task 1); `touch_raw_get()` (Task 2); `PARENT_APP`, `ui_arc`, `HOLD_*` tokens (Task 3); `app_manager_open()`, `app_manager_current()` (`app_manager.h`); `idle_wake()` (`idle.h`).
- Produces: nothing new for other code.

- [ ] **Step 1: Add the hold ring and the poll timer**

In `components/ui/launcher.c`, add these includes next to the others:

```c
#include "idle.h"
#include "parent.h"
#include "touch_raw.h"
```

Add file-scope state below `static lv_timer_t *s_react_timer;`:

```c
static lv_obj_t *s_hold_ring;
static hold_track_t s_hold;
```

Add these functions above `launcher_create()`:

```c
/* Two fingers are down: make LVGL ignore the rest of this touch, so letting go is not also a tap
 * on the cat or the icon (the same call the app manager uses before closing an app). */
static void swallow_touch(void)
{
    for (lv_indev_t *i = lv_indev_get_next(NULL); i; i = lv_indev_get_next(i)) {
        if (lv_indev_get_type(i) == LV_INDEV_TYPE_POINTER && lv_indev_get_state(i) == LV_INDEV_STATE_PRESSED) {
            lv_indev_wait_release(i);
        }
    }
}

static void hold_tick(lv_timer_t *t)
{
    (void)t;
    if (app_manager_current() || lv_screen_active() != s_scr) {
        hold_reset(&s_hold); /* only on the menu */
        ui_set_hidden(s_hold_ring, true);
        return;
    }
    touch_raw_t raw = touch_raw_get();
    uint16_t progress = 0;
    hold_state_t st = hold_update(&s_hold, raw.count, raw.x, raw.y, lv_tick_get(), &progress);
    if (st != HOLD_IDLE) {
        swallow_touch();
    }
    ui_set_hidden(s_hold_ring, st != HOLD_PROGRESS);
    if (st == HOLD_PROGRESS) {
        lv_arc_set_value(s_hold_ring, progress);
    } else if (st == HOLD_DONE) {
        idle_wake(); /* the hold may have started on a dimmed or dark menu */
        s_busy = true; /* launcher_show() clears it when the menu comes back */
        app_manager_open(&PARENT_APP);
    }
}
```

At the end of `launcher_create()`, after `cat_set_state(s_cat, current()->react);`, add:

```c
    s_hold_ring = ui_arc(s_scr, HOLD_RING_CX, HOLD_RING_CY, HOLD_RING_R, HOLD_RING_W);
    lv_arc_set_rotation(s_hold_ring, 270); /* start at 12 o'clock */
    lv_arc_set_bg_angles(s_hold_ring, 0, 360);
    lv_arc_set_range(s_hold_ring, 0, 1000);
    lv_obj_set_style_arc_opa(s_hold_ring, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_hold_ring, lv_color_white(), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(s_hold_ring, HOLD_RING_OPA, LV_PART_INDICATOR);
    ui_set_hidden(s_hold_ring, true);
    lv_timer_create(hold_tick, HOLD_POLL_MS, NULL);
```

- [ ] **Step 2: Build and flash**

Run:

```bash
idf.py build
idf.py -p /dev/cu.usbmodem101 flash
```

Expected: `Project build complete.` and a successful flash.

- [ ] **Step 3: Human step. Open, adjust, leave**

Keep a log running: `python tools/serial_log.py /dev/cu.usbmodem101 120`. Ask the user to:
1. On the menu, hold two fingers still anywhere. Expected: nothing for 1 s, then a faint white ring fills around the cat, and at 5 s the Max volume screen fades in.
2. Press "−" three times. Expected: the arc and number drop 100 → 70 %, and each press clicks, a little quieter each time.
3. Press "+" once (80 %), then press **BOOT**. Expected: back to the menu at once; the mew plays at the new level.
4. Swipe and tap the cat. Expected: sounds are at the new level.

Expected in the log: no Guru Meditation, abort or reboot.

- [ ] **Step 4: Human step. Things that must not open it**

Ask the user to:
1. hold one finger on the cat for 10 s;
2. hold two fingers for about 3 s and lift;
3. hold two fingers and slide one of them about 2 cm;
4. do the full 5 s hold again and let go.

Expected:
- 1–3 never open the parent area. With 2, the ring appears after 1 s and disappears on lift.
- In 4, letting go does not also say the app name or open an app.

- [ ] **Step 5: Human step. Persistence and the timeout**

1. With the volume at 80 %, reset the board (`python -c "import serial,time; s=serial.Serial('/dev/cu.usbmodem101'); s.dtr=False; s.rts=True; time.sleep(0.1); s.rts=False"`). Expected: the log line `settings: volume cap 80%` and the boot sounds at that level.
2. Open the parent area and don't touch it. Expected: after about 60 s it returns to the menu by itself.
3. Set the volume back to what the user wants to keep.

- [ ] **Step 6: Record and commit**

In the spec, append this paragraph at the end of §3.1 in `docs/superpowers/specs/2026-09-22-cat-toy-design.md`:

```markdown
The first slice of the parent area (2026-09-23) opens straight on **Max volume**: the design's
270° arc, 10 % steps from 10 % to 100 %, and a "Home button exits" hint in place of the "‹ Parents"
button. It uses LVGL's built-in Montserrat until the full parent area brings Fredoka and the
four-tile home screen (M7).
```

Append a `## Parent volume (2026-09-23)` section to `docs/device-checks.md`. It holds a `| Check | Result |` table with one row per numbered item in Steps 3–5, filled in with what the user reported.

```bash
git add components/ui/launcher.c docs/superpowers/specs/2026-09-22-cat-toy-design.md docs/device-checks.md
git commit -m "feat: open the parent volume screen with a two-finger hold on the menu"
```
