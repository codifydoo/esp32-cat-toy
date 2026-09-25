# M5 Magic Paws, Yarn Roll and Meow Piano Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The last three placeholder apps become real: Magic paws (finger painting in rainbow dots or paw prints, shaken clean by a sweeping cat), Yarn roll (tilt the yarn into a basket while a cat chases it, with a confetti win) and Meow piano (five colored cats that sing C, D, E, G and A).

**Architecture:**
- **Host-tested logic (`components/logic`):**
  - `paint.c`: antialiased circles blended into an RGB565 picture, with a dirty rectangle;
  - `brush.c`: turns touch samples into dots every 7 px or paw prints every 40 px, including between samples;
  - `yarn_rules.c`: the basket spots, the win test, and where the chasing cat goes.
- **Cat widget (`components/cat`):** `cat_create_colored()` draws a cat in an extra exported palette (the five piano cats, scale S only). The big surprised eyes get movable pupils, so the yarn cat can look at the yarn.
- **Assets:** the exporter renders the piano palettes and the new UI images; the sound list gains a "pat" and the yarn win jingle.
- **The apps:** `components/ui/apps/draw.c`, `yarn.c` and `piano.c`, one file each. Each app task removes its line from `placeholder.c` (the registry already lists them).
  - Magic paws paints into one 466 × 466 RGB565 picture (434 KB PSRAM) shown by an `lv_canvas`. The picture is allocated on first open and kept, so the drawing survives closing the app.
  - Yarn roll uses the M4 tilt path (`roll_step`) unchanged.

**Tech Stack:** ESP-IDF v5.5.5 (C11), LVGL 9.4, `waveshare/esp32_s3_touch_amoled_1_75c` 3.0.0, CMake + Unity host tests under UBSan, Node + resvg for images, sox for sounds.

**Spec:** `docs/superpowers/specs/2026-09-22-cat-toy-design.md`, mainly:
- §3.10: the tilt rules Yarn roll reuses;
- §3.11: Magic paws, Yarn roll and Meow piano details, the decisions this plan adds;
- §4.4: the app contract, and its one exception (the kept drawing);
- §7: gestures, the leak soak and the FPS target.

The design is the UI source of truth:
- `design/claude-design/Screen.dc.html`: the `draw`, `erase`, `yarn`, `yarnwin` and `piano` views;
- `Main.dc.html`: `act('draw' | 'tool' | 'piano')`, `shake()`, `tilt()` and the CSS keyframes (`twinkle`, `sweep`, `fall`, `bounce`, `catwobble`);
- `Cat.dc.html`: the `fur`/`line` recoloring;
- `Spec.dc.html`: "Piano pitches", "Rainbow brush", "Motion" and "Sound map";
- `Assets.dc.html`: the Magic paws, Yarn roll and Meow piano asset rows.

## Global Constraints

- **Toolchain:** ESP-IDF v5.5.5, target `esp32s3`.
  - Run every `idf.py` command in a shell that has sourced `~/esp/esp-idf/export.sh`. The port is `/dev/cu.usbmodem101`.
  - After adding a `.c` file to a component, reconfigure each build directory you build (`idf.py reconfigure`, or `-B <dir> ... reconfigure`): sources are globbed at configure time.
- **Build variants:** each is `idf.py -B build-X -D SDKCONFIG=build-X/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.X" build`. They are `diag`, `soak` and `perf`, plus `gallery`, which uses `"sdkconfig.defaults;sdkconfig.gallery;sdkconfig.perf"`. The normal build is `idf.py build`.
- **Assets:**
  - `assets/` is generated and git-ignored. After changing anything under `tools/export_images/` or `tools/build_sounds/`, run `tools/build_assets.sh`. It runs the host tests, exports images, builds sounds and prints the size of `assets/`.
  - The generated manifest `components/assets/assets_manifest.{c,h}` is committed.
  - The assets partition is 12,517,376 bytes. `idf.py build` fails if `assets/` does not fit.
- **Board quirks** (`docs/device-checks.md`):
  - opening the serial port resets the board;
  - a second reset within ~1 s of the first can hang early boot.

  Always flash with `tools/flash_and_log.sh`. It waits 3 s after flashing, then listens passively. Only one listener can hold the port, so stop the previous logger before flashing. Flashing rewrites the assets partition (~60 s).
- **Spec §3.11 (added for M5), summarized:**
  - **Magic paws:**
    - the drawing is kept when the app closes, until a shake clears it or the toy restarts. It is one 466 × 466 RGB565 picture, allocated on the first open and never freed;
    - rainbow dots every 7 px and paw prints every 40 px along the stroke, including between touch samples;
    - each dot is painted with its 28 % halo just before it;
    - prints are five round blobs turned along the stroke;
    - nothing is painted beyond r 212;
    - a sparkle every 8 dots and a "pat" per print, each at most 8 per second;
    - the empty canvas shows the twinkling hint;
    - shake: the drawing dims to 30 %, the M cat sweeps ±60 px (three swings in 1.6 s, broom not rotated) to a swish, "Čisto!" at 0.9 s, clean at 1.7 s. Touches and shakes are ignored meanwhile, and Home or the timeout during the sweep still leaves the canvas clean.
  - **Yarn roll:**
    - the §3.10 tilt rules, with the yarn within r 185;
    - four roll frames that advance with distance, and a rumble loop while it rolls;
    - the S cat, surprised, follows 92 px behind on the center side, eases into place and looks at the yarn;
    - the tilt hint shows for the first 4 s;
    - a shake recenters the yarn, except during the win;
    - baskets at (330, 318), (136, 318), (330, 150) and (136, 150), in turn. A win is the yarn within 62 px of the point 10 px above the basket's center. The yarn stops, and 0.32 s later the win scene shows: full basket, proud M cat, 24 confetti, fanfare + "Bravo!". The next round comes 3.5 s later;
    - confetti spins first; the fallback drops the spin below 24 fps.
  - **Meow piano:**
    - five cats on r 140 from the top, notes C D E G A, recolored per note (fur, outline, light muzzle, no stripes), exported at S only;
    - a tap plays the meow, the cat sings and bounces (up 22 px, down, 6 px rebound, 400 ms), its pad fills at 25 % and the center note takes its color;
    - back to happy after 700 ms; a new tap replaces the previous meow.
- **Tilt (spec §7 and §3.10), verbatim:** "Tilt: 8° dead zone. Roll speed grows with angle up to 25°, capped at 240 px/s." Level is the grip when the app opens (`app_manager` recenters). Rolling counts as activity; resting against the rim does not.
- **Design geometry:**
  - **Magic paws** (Screen.dc.html `draw`/`erase`):
    - dots 16 px with a 30 px halo at 28 %, in the rainbow `#FF5A5A #FF9F3A #FFD84A #5CD67A #4FD8FF #5A8CFF #B07CFF`, the color stepping every 3 dots;
    - the brush button is a 150 px image at (158, 300): a black disc r 70 with a 6 px white ring at 45 %, showing the current brush (rainbow arcs, or a `#FFB0C8` paw). Tapping it switches brush with a blip;
    - the empty hint is 160 × 120 at (153, 120);
    - the sweeping cat is M at origin (133, 196), with grey `#8A8AA0` dust 266 × 60 at (100, 360).
  - **Yarn roll** (Screen.dc.html `yarn`/`yarnwin`):
    - the yarn is 64 px (`#FF7FB0`, stroke `#A8326A`, strands `#B8356F`);
    - the basket is 120 × 96, top-left at (bx − 60, by − 48);
    - the tilt hint is 60 × 44 at (203, 386), wobbling ±7° over 0.6 s around 50 %/80 %;
    - the win shows the proud M cat at origin (133, 68) and the 120 × 110 full basket at (173, 296);
    - confetti: 12 × 22 pieces, radius 3, x = 60 + (i·53 mod 346), top −30, delay (i·97 mod 1400) ms, falling 520 px with 540° of spin over 1.6 s ease-in, repeating, colored in rainbow order.
  - **Meow piano** (Screen.dc.html `piano`):
    - key i sits at angle −90° + 72°·i on r 140 around (233, 233);
    - its pad is a circle r 70, stroke 5 px in the key color at 70 %, filled at 25 % while singing;
    - the cat is S at origin (cx − 65, cy − 70);
    - the center note is 100 × 100 at (183, 183), white, recolored to the singing key.
  - **Piano pitches (Spec.dc.html), fur / outline:** C5 `#FF5A5A`/`#6E1616`, D5 `#FF9F3A`/`#6B3A06`, E5 `#FFD84A`/`#6B5400`, G5 `#5CD67A`/`#1D5A2B`, A5 `#5AA8FF`/`#173F74`.
- **Sounds (design "Sound map"), verbatim:**
  - Magic paws, Drawing: "Sparkle tick, max 8 per second / soft "pat" per print".
  - Magic paws, Shake: "Broom swish" and "Čisto! (All clean!)".
  - Yarn roll, Rolling / basket: "Soft rumble with speed / fanfare" and "Bravo!".
  - Meow piano, Tap a cat: "Pitched meow C5 D5 E5 G5 A5".

  Every sound goes through `audio.h`, so the volume cap applies. `audio_play()` has one voice, so a new sound replaces the one playing; `audio_loop_*` is a separate voice.
- **Home:** BOOT returns to the menu from every screen within 300 ms. `app_manager` calls `destroy()` and then `audio_stop_all()`.
- **`app_t` contract** (`components/ui/include/app.h`):
  - `destroy()` leaves no timers, animations or audio behind. The kept Magic paws picture is the only PSRAM an app may keep;
  - every callback the app registered checks that the app is still open and no-ops otherwise;
  - `on_shake`/`on_tilt` run on the LVGL task, only while the app is open and the screen is awake.
- **Text:** child screens have no text.
- **Host tests:** `sh test/run.sh` must pass before any commit that touches `components/logic/`.
- **Commits:** `feat:`, `fix:`, `test:`, `chore:`, `docs:` prefixes, no attribution trailers.
- **Human steps** are marked **Human step**. Ask with AskUserQuestion and put the full instructions inside the question text: the user may not see the message around it. Never guess a result that needs eyes or hands on the board.

## Review Focus

1. **Fast scribbles and jumpy touch samples.** A 5-year-old draws fast, with gaps of 50+ px between touch samples, and sometimes the reported point jumps. The trail must stay continuous, and nothing may be written outside the picture's memory. Task 1 pins:
   - dots between samples;
   - short moves adding up;
   - a 1000 px jump capped at 32 stamps, with the stroke carrying on from the finger;
   - clipping at every edge, with guard words around the picture;
   - nothing painted outside r 212.
2. **Home or the 60 s timeout in the middle of something**: mid-sweep, mid-roll (rumble loop), between the basket hit and the win scene, during the win, and mid-song. Nothing may keep running, and a sweep cut short must still leave the canvas clean (spec §3.11). Each app's `destroy()` handles its own timers and animations; `app_manager` stops the rumble with `audio_stop_all()`. The existing soak shakes and tilts every app right before Home, 50 times (Task 8 Step 6). Task 8 Step 4 checks BOOT in each state on the device.
3. **A toy left tilted with the yarn against the rim.** The rumble must stop, and the 60 s timeout must still close the app. `roll_step` reports 0 against the rim (M4's test). The app stops the rumble 150 ms after the last real roll and counts only real rolls as activity. Task 8 Step 3 checks it on the device.
4. **A win that fires twice, or at once in the next round.** The hit must freeze play until the win scene, and a new round must not start inside a basket. Task 2 pins that the restart point (the center) is inside none of the four baskets. Task 6's phase guard ignores tilt and shake after the hit. Task 8 Step 3 plays three rounds in a row.
5. **Rapid taps across several piano cats.** Only the last cat sings, no cat stays stuck singing or bounced, and the center note always ends white. Task 7's `quiet()` resets the previous singer before every tap. Task 8 Step 3 checks fast taps on the device.

---

### Task 1: Paint and brush logic (Magic paws)

**Files:**
- Create: `components/logic/include/paint.h`, `components/logic/paint.c`
- Create: `components/logic/include/brush.h`, `components/logic/brush.c`
- Test: `test/test_paint.c`, `test/test_brush.c`

**Interfaces:**
- Consumes: nothing.
- Produces (used by Task 5):
  - `typedef struct { int16_t x0, y0, x1, y1; } paint_rect_t;` (inclusive; empty while `x1 < x0`)
  - `void paint_rect_reset(paint_rect_t *r);`
  - `uint16_t paint_rgb565(uint32_t rgb);`
  - `void paint_circle(uint16_t *px, int w, int h, float cx, float cy, float r, uint16_t color, uint8_t opa, paint_rect_t *dirty);` Pixel (x, y) is the square from (x, y) to (x + 1, y + 1), so a touch at pixel (x, y) is painted around (x + 0.5, y + 0.5).
  - `BRUSH_*` defines, `brush_kind_t` (`BRUSH_RAINBOW`, `BRUSH_PAWS`), `stamp_t {int16_t x, y; uint8_t color; int16_t angle_deg;}`, `brush_t` (fields `kind`, `down`, `lx`, `ly`, `dots`, `paws`, `cx`, `cy`), `blob_t {float x, y, r;}`
  - `void brush_init(brush_t *b, brush_kind_t kind, int16_t cx, int16_t cy);`
  - `void brush_clear(brush_t *b);`
  - `size_t brush_down(brush_t *b, int16_t x, int16_t y, stamp_t out[BRUSH_MAX_STAMPS]);`
  - `size_t brush_move(brush_t *b, int16_t x, int16_t y, stamp_t out[BRUSH_MAX_STAMPS]);`
  - `void brush_up(brush_t *b);`
  - `void paw_blobs(const stamp_t *s, blob_t out[5]);`

- [ ] **Step 1: Write the failing paint test**

Create `test/test_paint.c`:

```c
#include <stdbool.h>
#include <stddef.h>
#include "paint.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define W     40
#define H     30
#define GUARD 64
static uint16_t s_buf[GUARD + W * H + GUARD];
static uint16_t *const px = s_buf + GUARD;

static void fill(uint16_t v)
{
    for (size_t i = 0; i < sizeof(s_buf) / sizeof(s_buf[0]); i++) {
        s_buf[i] = 0xBEEF; /* guard value outside the picture */
    }
    for (int i = 0; i < W * H; i++) {
        px[i] = v;
    }
}

static bool guards_intact(void)
{
    for (int i = 0; i < GUARD; i++) {
        if (s_buf[i] != 0xBEEF || s_buf[GUARD + W * H + i] != 0xBEEF) {
            return false;
        }
    }
    return true;
}

static uint16_t at(int x, int y) { return px[y * W + x]; }

static void test_rgb565_packs_the_design_colors(void)
{
    TEST_ASSERT_EQUAL_HEX16(0xF800, paint_rgb565(0xFF0000));
    TEST_ASSERT_EQUAL_HEX16(0x07E0, paint_rgb565(0x00FF00));
    TEST_ASSERT_EQUAL_HEX16(0x001F, paint_rgb565(0x0000FF));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, paint_rgb565(0xFFFFFF));
    TEST_ASSERT_EQUAL_HEX16(0xFACB, paint_rgb565(0xFF5A5A)); /* rainbow r1 */
}

static void test_a_solid_dot_covers_its_middle_and_nothing_beyond_its_edge(void)
{
    fill(0x0000);
    paint_circle(px, W, H, 20.5f, 15.5f, 8.0f, 0xFFFF, 255, NULL); /* centered on pixel (20, 15) */
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(20, 15));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(27, 15)); /* 7 px out */
    TEST_ASSERT_EQUAL_HEX16(0x0000, at(29, 15)); /* 9 px out */
    TEST_ASSERT_EQUAL_HEX16(0x0000, at(20, 6));
    TEST_ASSERT_TRUE(guards_intact());
}

static void test_the_edge_is_antialiased(void)
{
    fill(0x0000);
    paint_circle(px, W, H, 20.5f, 15.5f, 8.0f, 0xFFFF, 255, NULL);
    TEST_ASSERT_EQUAL_HEX16(0x7BEF, at(28, 15)); /* 8 px out, right on the edge: half covered */
}

static void test_opacity_blends_over_what_is_there(void)
{
    fill(0x0000);
    paint_circle(px, W, H, 20.0f, 15.0f, 15.0f, 0xFFFF, 72, NULL); /* a 28 % halo */
    TEST_ASSERT_EQUAL_HEX16(0x4228, at(20, 15));
    paint_circle(px, W, H, 20.0f, 15.0f, 8.0f, 0xF800, 255, NULL); /* then a solid red dot on top */
    TEST_ASSERT_EQUAL_HEX16(0xF800, at(20, 15));
}

static void test_painting_is_clipped_to_the_picture(void)
{
    fill(0x0000);
    paint_circle(px, W, H, -3.0f, -3.0f, 8.0f, 0xFFFF, 255, NULL);  /* over the top-left corner */
    paint_circle(px, W, H, 43.0f, 33.0f, 8.0f, 0xFFFF, 255, NULL);  /* over the bottom-right corner */
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(0, 0));
    TEST_ASSERT_EQUAL_HEX16(0xFFFF, at(W - 1, H - 1));
    TEST_ASSERT_TRUE(guards_intact());
}

static void test_the_dirty_rect_covers_what_changed(void)
{
    fill(0x0000);
    paint_rect_t d;
    paint_rect_reset(&d);
    paint_circle(px, W, H, -50.0f, -50.0f, 8.0f, 0xFFFF, 255, &d); /* entirely outside: nothing */
    TEST_ASSERT_TRUE(d.x1 < d.x0);
    paint_circle(px, W, H, 20.0f, 15.0f, 8.0f, 0xFFFF, 255, &d);
    TEST_ASSERT_EQUAL_INT16(12, d.x0);
    TEST_ASSERT_EQUAL_INT16(27, d.x1);
    TEST_ASSERT_EQUAL_INT16(7, d.y0);
    TEST_ASSERT_EQUAL_INT16(22, d.y1);
    paint_circle(px, W, H, 38.0f, 2.0f, 4.0f, 0xFFFF, 255, &d); /* grows, clipped at the edges */
    TEST_ASSERT_EQUAL_INT16(12, d.x0);
    TEST_ASSERT_EQUAL_INT16(W - 1, d.x1);
    TEST_ASSERT_EQUAL_INT16(0, d.y0);
    TEST_ASSERT_EQUAL_INT16(22, d.y1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rgb565_packs_the_design_colors);
    RUN_TEST(test_a_solid_dot_covers_its_middle_and_nothing_beyond_its_edge);
    RUN_TEST(test_the_edge_is_antialiased);
    RUN_TEST(test_opacity_blends_over_what_is_there);
    RUN_TEST(test_painting_is_clipped_to_the_picture);
    RUN_TEST(test_the_dirty_rect_covers_what_changed);
    return UNITY_END();
}
```

- [ ] **Step 2: Write the failing brush test**

Create `test/test_brush.c`:

```c
#include "brush.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static stamp_t st[BRUSH_MAX_STAMPS];

static void test_a_touch_paints_one_dot_where_it_lands(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    TEST_ASSERT_EQUAL(1, brush_down(&b, 233, 233, st));
    TEST_ASSERT_EQUAL_INT16(233, st[0].x);
    TEST_ASSERT_EQUAL_INT16(233, st[0].y);
    TEST_ASSERT_EQUAL_UINT8(0, st[0].color);
}

static void test_dots_come_every_7px_even_between_samples(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    brush_down(&b, 233, 233, st);
    TEST_ASSERT_EQUAL(10, brush_move(&b, 303, 233, st)); /* one 70 px sample */
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT16(240 + 7 * i, st[i].x);
        TEST_ASSERT_EQUAL_INT16(233, st[i].y);
    }
}

static void test_the_color_steps_every_3_dots_and_wraps_after_7(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    brush_down(&b, 233, 100, st); /* dot 0 */
    size_t n = brush_move(&b, 233, 100 + 7 * 21, st); /* dots 1..21 */
    TEST_ASSERT_EQUAL(21, n);
    TEST_ASSERT_EQUAL_UINT8(0, st[1].color);  /* dot 2 */
    TEST_ASSERT_EQUAL_UINT8(1, st[2].color);  /* dot 3 */
    TEST_ASSERT_EQUAL_UINT8(6, st[19].color); /* dot 20 */
    TEST_ASSERT_EQUAL_UINT8(0, st[20].color); /* dot 21 wraps */
}

static void test_short_moves_add_up(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    brush_down(&b, 233, 233, st);
    TEST_ASSERT_EQUAL(0, brush_move(&b, 236, 233, st));
    TEST_ASSERT_EQUAL(0, brush_move(&b, 239, 233, st));
    TEST_ASSERT_EQUAL(1, brush_move(&b, 241, 233, st));
    TEST_ASSERT_EQUAL_INT16(240, st[0].x);
}

static void test_nothing_is_painted_outside_the_round_area(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    TEST_ASSERT_EQUAL(1, brush_down(&b, 438, 233, st));    /* 205 px out: inside */
    TEST_ASSERT_EQUAL(1, brush_move(&b, 460, 233, st));    /* 445 is inside, 452 and 459 are not */
    TEST_ASSERT_EQUAL_INT16(445, st[0].x);
    TEST_ASSERT_EQUAL(0, brush_down(&b, 20, 20, st));      /* a touch near the rim paints nothing */
    TEST_ASSERT_EQUAL_UINT32(2, b.dots);                   /* only painted dots count for color */
}

static void test_moving_without_a_touch_paints_nothing(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    TEST_ASSERT_EQUAL(0, brush_move(&b, 300, 233, st));
    brush_down(&b, 233, 233, st);
    brush_up(&b);
    TEST_ASSERT_EQUAL(0, brush_move(&b, 300, 233, st));
}

static void test_a_huge_jump_is_capped_and_the_stroke_goes_on_from_the_finger(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    brush_down(&b, 233, 30, st);
    size_t n = brush_move(&b, 233, 1030, st); /* 1000 px in one sample */
    TEST_ASSERT_TRUE(n <= BRUSH_MAX_STAMPS);
    TEST_ASSERT_EQUAL_FLOAT(1030.0f, b.ly);
}

static void test_paw_prints_alternate_sides_and_turn_along_the_stroke(void)
{
    brush_t b;
    brush_init(&b, BRUSH_PAWS, 233, 233);
    TEST_ASSERT_EQUAL(1, brush_down(&b, 233, 233, st)); /* first print: toes up, to the left */
    TEST_ASSERT_EQUAL_INT16(224, st[0].x);
    TEST_ASSERT_EQUAL_INT16(233, st[0].y);
    TEST_ASSERT_EQUAL_INT16(0, st[0].angle_deg);
    TEST_ASSERT_EQUAL(2, brush_move(&b, 333, 233, st)); /* moving right: prints at 40 and 80 px */
    TEST_ASSERT_EQUAL_INT16(90, st[0].angle_deg);       /* toes point right */
    TEST_ASSERT_EQUAL_INT16(273, st[0].x);
    TEST_ASSERT_EQUAL_INT16(242, st[0].y);               /* right-hand side of the stroke */
    TEST_ASSERT_EQUAL_INT16(313, st[1].x);
    TEST_ASSERT_EQUAL_INT16(224, st[1].y);               /* then the left */
    TEST_ASSERT_EQUAL_UINT8(1, st[0].color);
    TEST_ASSERT_EQUAL_UINT8(2, st[1].color);
}

static void test_clearing_starts_the_colors_over(void)
{
    brush_t b;
    brush_init(&b, BRUSH_RAINBOW, 233, 233);
    brush_down(&b, 233, 100, st);
    brush_move(&b, 233, 170, st); /* 10 more dots */
    brush_up(&b);
    brush_clear(&b);
    brush_down(&b, 233, 233, st);
    TEST_ASSERT_EQUAL_UINT8(0, st[0].color);
}

static void test_paw_blobs_turn_with_the_print(void)
{
    blob_t bl[5];
    const stamp_t up = {100, 100, 0, 0};
    paw_blobs(&up, bl);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, bl[0].x); /* pad below the middle */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 104.96f, bl[0].y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.93f, bl[2].y); /* toes above */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 7.0f, bl[0].r);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 3.4f, bl[1].r);
    const stamp_t right = {100, 100, 0, 90};
    paw_blobs(&right, bl);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 95.04f, bl[0].x); /* pad behind, to the left */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, bl[0].y);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 103.54f, bl[1].x); /* toes ahead, to the right */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.08f, bl[1].y);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_a_touch_paints_one_dot_where_it_lands);
    RUN_TEST(test_dots_come_every_7px_even_between_samples);
    RUN_TEST(test_the_color_steps_every_3_dots_and_wraps_after_7);
    RUN_TEST(test_short_moves_add_up);
    RUN_TEST(test_nothing_is_painted_outside_the_round_area);
    RUN_TEST(test_moving_without_a_touch_paints_nothing);
    RUN_TEST(test_a_huge_jump_is_capped_and_the_stroke_goes_on_from_the_finger);
    RUN_TEST(test_paw_prints_alternate_sides_and_turn_along_the_stroke);
    RUN_TEST(test_clearing_starts_the_colors_over);
    RUN_TEST(test_paw_blobs_turn_with_the_print);
    return UNITY_END();
}
```

- [ ] **Step 3: Run the tests to verify they fail**

Run: `sh test/run.sh`
Expected: the build fails with `paint.h: No such file or directory` (and `brush.h`).

- [ ] **Step 4: Write paint**

Create `components/logic/include/paint.h`:

```c
#pragma once
#include <stdint.h>

/* Painting into an RGB565 picture (Magic paws, spec §3.11). Pure C, no LVGL. */

typedef struct {
    int16_t x0, y0, x1, y1; /* inclusive; empty while x1 < x0 */
} paint_rect_t;

void paint_rect_reset(paint_rect_t *r);
/* 0xRRGGBB -> RGB565 */
uint16_t paint_rgb565(uint32_t rgb);
/* Fills the circle of radius r centered on (cx, cy) with an antialiased edge, blending color at opa
 * (0..255) over what is there. Pixel (x, y) is the square from (x, y) to (x + 1, y + 1). Clipped to the
 * w x h picture; grows *dirty (if not NULL) to cover every pixel it changed. */
void paint_circle(uint16_t *px, int w, int h, float cx, float cy, float r, uint16_t color, uint8_t opa,
                  paint_rect_t *dirty);
```

Create `components/logic/paint.c`:

```c
#include "paint.h"
#include <math.h>

void paint_rect_reset(paint_rect_t *r) { *r = (paint_rect_t){.x0 = INT16_MAX, .y0 = INT16_MAX, .x1 = -1, .y1 = -1}; }

uint16_t paint_rgb565(uint32_t rgb)
{
    return (uint16_t)(((rgb >> 8) & 0xF800u) | ((rgb >> 5) & 0x07E0u) | ((rgb >> 3) & 0x001Fu));
}

/* a = 0..256 (256 = all src) */
static uint16_t blend(uint16_t dst, uint16_t src, uint32_t a)
{
    const uint32_t na = 256u - a;
    const uint32_t r = ((uint32_t)(src >> 11) * a + (uint32_t)(dst >> 11) * na) >> 8;
    const uint32_t g = ((uint32_t)((src >> 5) & 0x3Fu) * a + (uint32_t)((dst >> 5) & 0x3Fu) * na) >> 8;
    const uint32_t b = ((uint32_t)(src & 0x1Fu) * a + (uint32_t)(dst & 0x1Fu) * na) >> 8;
    return (uint16_t)((r << 11) | (g << 5) | b);
}

static void grow(paint_rect_t *r, int x, int y)
{
    if (x < r->x0) {
        r->x0 = (int16_t)x;
    }
    if (y < r->y0) {
        r->y0 = (int16_t)y;
    }
    if (x > r->x1) {
        r->x1 = (int16_t)x;
    }
    if (y > r->y1) {
        r->y1 = (int16_t)y;
    }
}

static int clampi(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

void paint_circle(uint16_t *px, int w, int h, float cx, float cy, float r, uint16_t color, uint8_t opa,
                  paint_rect_t *dirty)
{
    if (r <= 0.0f || opa == 0) {
        return;
    }
    const int x0 = clampi((int)floorf(cx - r - 1.0f), 0, w - 1), x1 = clampi((int)ceilf(cx + r + 1.0f), 0, w - 1);
    const int y0 = clampi((int)floorf(cy - r - 1.0f), 0, h - 1), y1 = clampi((int)ceilf(cy + r + 1.0f), 0, h - 1);
    if (cx + r + 1.0f < 0.0f || cy + r + 1.0f < 0.0f || cx - r - 1.0f > (float)(w - 1) ||
        cy - r - 1.0f > (float)(h - 1)) {
        return; /* entirely outside */
    }
    for (int y = y0; y <= y1; y++) {
        for (int x = x0; x <= x1; x++) {
            const float dx = (float)x + 0.5f - cx, dy = (float)y + 0.5f - cy;
            float cover = r + 0.5f - sqrtf(dx * dx + dy * dy);
            if (cover <= 0.0f) {
                continue;
            }
            if (cover > 1.0f) {
                cover = 1.0f;
            }
            const uint32_t a = (uint32_t)(cover * (float)opa * (256.0f / 255.0f) + 0.5f);
            uint16_t *p = &px[y * w + x];
            *p = blend(*p, color, a > 256u ? 256u : a);
            if (dirty) {
                grow(dirty, x, y);
            }
        }
    }
}
```

- [ ] **Step 5: Write brush**

Create `components/logic/include/brush.h`:

```c
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Magic paws strokes (design Main.dc.html act('draw'); spec §3.11). */
#define BRUSH_DOT_STEP_PX 7   /* a rainbow dot every 7 px along the stroke */
#define BRUSH_COLOR_EVERY 3   /* the rainbow color steps every 3 dots */
#define BRUSH_PAW_STEP_PX 40  /* a paw print every 40 px */
#define BRUSH_PAW_SIDE_PX 9   /* prints alternate left and right of the stroke */
#define BRUSH_AREA_R      212 /* painted only within this radius of the screen center */
#define BRUSH_COLORS      7   /* the design's rainbow r1..r7 */
#define BRUSH_MAX_STAMPS  32  /* per touch sample; a longer jump restarts the stroke at the new point */

typedef enum { BRUSH_RAINBOW, BRUSH_PAWS } brush_kind_t;

typedef struct {
    int16_t x, y;      /* where to paint: a dot's center, or a paw print's center (already moved to its side) */
    uint8_t color;     /* 0..BRUSH_COLORS-1 */
    int16_t angle_deg; /* paws: turn, 0 = toes up, positive = clockwise on screen */
} stamp_t;

typedef struct {
    brush_kind_t kind;
    bool down;
    float lx, ly;       /* the stroke point of the last stamp */
    uint32_t dots, paws; /* stamps painted since the canvas was last cleared (they pick the colors) */
    int16_t cx, cy;     /* screen center, for BRUSH_AREA_R */
} brush_t;

void brush_init(brush_t *b, brush_kind_t kind, int16_t cx, int16_t cy);
void brush_clear(brush_t *b); /* the canvas was wiped: colors start over */
/* A finger went down: one stamp there (unless outside the area). */
size_t brush_down(brush_t *b, int16_t x, int16_t y, stamp_t out[BRUSH_MAX_STAMPS]);
/* The finger moved: stamps every BRUSH_DOT_STEP_PX / BRUSH_PAW_STEP_PX along the way from the last one. */
size_t brush_move(brush_t *b, int16_t x, int16_t y, stamp_t out[BRUSH_MAX_STAMPS]);
void brush_up(brush_t *b);

typedef struct {
    float x, y, r;
} blob_t;
/* The five round parts of a paw print (pad first, then four toes), turned by the stamp's angle. */
void paw_blobs(const stamp_t *s, blob_t out[5]);
```

Create `components/logic/brush.c`:

```c
#include "brush.h"
#include <math.h>

#define DEG_PER_RAD 57.2957795f

void brush_init(brush_t *b, brush_kind_t kind, int16_t cx, int16_t cy)
{
    *b = (brush_t){.kind = kind, .cx = cx, .cy = cy};
}

void brush_clear(brush_t *b)
{
    b->dots = 0;
    b->paws = 0;
    b->down = false;
}

void brush_up(brush_t *b) { b->down = false; }

static bool inside(const brush_t *b, float x, float y)
{
    const float dx = x - b->cx, dy = y - b->cy;
    return dx * dx + dy * dy <= (float)(BRUSH_AREA_R * BRUSH_AREA_R);
}

/* One stamp for the stroke point (x, y); ux, uy is the stroke direction (0, 0 at the first touch). */
static size_t stamp(brush_t *b, float x, float y, float ux, float uy, stamp_t *out)
{
    if (!inside(b, x, y)) {
        return 0;
    }
    if (b->kind == BRUSH_RAINBOW) {
        *out = (stamp_t){(int16_t)lroundf(x), (int16_t)lroundf(y),
                         (uint8_t)((b->dots / BRUSH_COLOR_EVERY) % BRUSH_COLORS), 0};
        b->dots++;
        return 1;
    }
    /* Design: the angle follows the stroke (0 at the first print), and prints alternate sides. */
    const float angle = (ux == 0.0f && uy == 0.0f) ? 0.0f : atan2f(uy, ux) * DEG_PER_RAD + 90.0f;
    const float side = (b->paws % 2) ? 1.0f : -1.0f;
    const float rad = angle / DEG_PER_RAD;
    *out = (stamp_t){(int16_t)lroundf(x + cosf(rad) * BRUSH_PAW_SIDE_PX * side),
                     (int16_t)lroundf(y + sinf(rad) * BRUSH_PAW_SIDE_PX * side), (uint8_t)(b->paws % BRUSH_COLORS),
                     (int16_t)lroundf(angle)};
    b->paws++;
    return 1;
}

size_t brush_down(brush_t *b, int16_t x, int16_t y, stamp_t out[BRUSH_MAX_STAMPS])
{
    b->down = true;
    b->lx = x;
    b->ly = y;
    return stamp(b, x, y, 0.0f, 0.0f, out);
}

size_t brush_move(brush_t *b, int16_t x, int16_t y, stamp_t out[BRUSH_MAX_STAMPS])
{
    if (!b->down) {
        return 0;
    }
    const float step = b->kind == BRUSH_RAINBOW ? BRUSH_DOT_STEP_PX : BRUSH_PAW_STEP_PX;
    float dx = (float)x - b->lx, dy = (float)y - b->ly;
    float d = sqrtf(dx * dx + dy * dy);
    size_t n = 0;
    int steps = 0;
    while (d >= step) {
        if (steps == BRUSH_MAX_STAMPS) {
            b->lx = x; /* a jump too long to fill: carry on from where the finger is */
            b->ly = y;
            return n;
        }
        const float ux = dx / d, uy = dy / d;
        b->lx += ux * step;
        b->ly += uy * step;
        steps++;
        n += stamp(b, b->lx, b->ly, ux, uy, &out[n]);
        dx = (float)x - b->lx;
        dy = (float)y - b->ly;
        d = sqrtf(dx * dx + dy * dy);
    }
    return n;
}

void paw_blobs(const stamp_t *s, blob_t out[5])
{
    /* The design's 34 px paw (a 24-unit path scaled by 34/24), relative to its center, toes up. */
    static const blob_t PAW[5] = {
        {0.0f, 4.96f, 7.0f}, {-9.92f, -3.54f, 3.4f}, {-3.83f, -9.07f, 3.4f}, {3.83f, -9.07f, 3.4f}, {9.92f, -3.54f, 3.4f},
    };
    const float rad = (float)s->angle_deg / DEG_PER_RAD, c = cosf(rad), sn = sinf(rad);
    for (int i = 0; i < 5; i++) {
        out[i].x = (float)s->x + PAW[i].x * c - PAW[i].y * sn;
        out[i].y = (float)s->y + PAW[i].x * sn + PAW[i].y * c;
        out[i].r = PAW[i].r;
    }
}
```

- [ ] **Step 6: Run the tests to verify they pass**

Run: `sh test/run.sh`
Expected: every suite passes, including `test_paint` (6 tests) and `test_brush` (10 tests), with no UBSan report.

- [ ] **Step 7: Commit**

```bash
git add components/logic/include/paint.h components/logic/paint.c components/logic/include/brush.h components/logic/brush.c test/test_paint.c test/test_brush.c
git commit -m "feat: painted dots and paw prints for Magic paws, with host tests"
```

---

### Task 2: Yarn roll rules

**Files:**
- Create: `components/logic/include/yarn_rules.h`, `components/logic/yarn_rules.c`
- Test: `test/test_yarn_rules.c`

**Interfaces:**
- Consumes: nothing.
- Produces (used by Task 6):
  - `YARN_RANGE` (185), `YARN_CHASE_PX` (92), `YARN_WIN_PX` (62), `YARN_BASKETS` (4), `YARN_FOLLOW_MS` (130)
  - `void yarn_basket(uint8_t i, int16_t *x, int16_t *y);`
  - `void yarn_chase_target(float yx, float yy, float cx, float cy, float *tx, float *ty);`
  - `bool yarn_in_basket(float yx, float yy, int16_t bx, int16_t by);`
  - `float yarn_follow(float pos, float target, uint32_t dt_ms);`

- [ ] **Step 1: Write the failing test**

Create `test/test_yarn_rules.c`:

```c
#include "unity.h"
#include "yarn_rules.h"

void setUp(void) {}
void tearDown(void) {}

static void test_the_four_basket_spots_come_round_again(void)
{
    int16_t x, y;
    yarn_basket(0, &x, &y);
    TEST_ASSERT_EQUAL_INT16(330, x);
    TEST_ASSERT_EQUAL_INT16(318, y);
    yarn_basket(3, &x, &y);
    TEST_ASSERT_EQUAL_INT16(136, x);
    TEST_ASSERT_EQUAL_INT16(150, y);
    yarn_basket(5, &x, &y); /* wraps to spot 1 */
    TEST_ASSERT_EQUAL_INT16(136, x);
    TEST_ASSERT_EQUAL_INT16(318, y);
}

static void test_the_cat_follows_on_the_center_side(void)
{
    float tx, ty;
    yarn_chase_target(333.0f, 233.0f, 233.0f, 233.0f, &tx, &ty); /* yarn to the right */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 241.0f, tx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 233.0f, ty);
    yarn_chase_target(233.0f, 133.0f, 233.0f, 233.0f, &tx, &ty); /* yarn above */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 233.0f, tx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 225.0f, ty);
}

static void test_near_the_center_the_cat_waits_above_the_yarn(void)
{
    float tx, ty;
    yarn_chase_target(240.0f, 233.0f, 233.0f, 233.0f, &tx, &ty);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 240.0f, tx);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 141.0f, ty);
}

static void test_in_the_basket_within_62px_of_its_mouth(void)
{
    /* basket 0 at (330, 318): its mouth is (330, 308) */
    TEST_ASSERT_TRUE(yarn_in_basket(330.0f, 308.0f, 330, 318));
    TEST_ASSERT_TRUE(yarn_in_basket(330.0f, 369.9f, 330, 318));
    TEST_ASSERT_FALSE(yarn_in_basket(330.0f, 370.0f, 330, 318));
    TEST_ASSERT_FALSE(yarn_in_basket(233.0f, 233.0f, 330, 318)); /* the start, in the middle */
}

static void test_a_new_round_never_starts_in_a_basket(void)
{
    for (uint8_t i = 0; i < YARN_BASKETS; i++) {
        int16_t x, y;
        yarn_basket(i, &x, &y);
        TEST_ASSERT_FALSE(yarn_in_basket(233.0f, 233.0f, x, y)); /* the yarn restarts in the middle */
    }
}

static void test_the_cat_eases_toward_its_spot(void)
{
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 10.0f, yarn_follow(10.0f, 110.0f, 0));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 73.21f, yarn_follow(10.0f, 110.0f, YARN_FOLLOW_MS)); /* 63 % of the gap */
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 110.0f, yarn_follow(10.0f, 110.0f, 2000));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -53.21f, yarn_follow(10.0f, -90.0f, YARN_FOLLOW_MS)); /* either way */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_the_four_basket_spots_come_round_again);
    RUN_TEST(test_the_cat_follows_on_the_center_side);
    RUN_TEST(test_near_the_center_the_cat_waits_above_the_yarn);
    RUN_TEST(test_in_the_basket_within_62px_of_its_mouth);
    RUN_TEST(test_a_new_round_never_starts_in_a_basket);
    RUN_TEST(test_the_cat_eases_toward_its_spot);
    return UNITY_END();
}
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `sh test/run.sh -R yarn_rules`
Expected: the build fails with `yarn_rules.h: No such file or directory`.

- [ ] **Step 3: Write the rules**

Create `components/logic/include/yarn_rules.h`:

```c
#pragma once
#include <stdbool.h>
#include <stdint.h>

/* Yarn roll rules (design Screen.dc.html yarn view and Main.dc.html tilt(); spec §3.11). */
#define YARN_RANGE     185 /* the yarn's center stays within this radius of the screen center */
#define YARN_CHASE_PX  92  /* the cat stays this far behind the yarn */
#define YARN_WIN_PX    62  /* the yarn is in when its center is this close to the basket's mouth */
#define YARN_BASKETS   4
#define YARN_FOLLOW_MS 130 /* the cat closes 63 % of the gap in this time: the design's 400 ms ease-out */

/* Center of basket i (i wraps): the design's four spots, used round after round. */
void yarn_basket(uint8_t i, int16_t *x, int16_t *y);
/* Where the chasing cat's center goes: YARN_CHASE_PX from the yarn toward (cx, cy), or straight above the
 * yarn when the yarn is within 30 px of (cx, cy). */
void yarn_chase_target(float yx, float yy, float cx, float cy, float *tx, float *ty);
/* True when the yarn's center is within YARN_WIN_PX of the point 10 px above the basket's center. */
bool yarn_in_basket(float yx, float yy, int16_t bx, int16_t by);
/* One ease-out step from pos toward target: after dt_ms the gap has shrunk by exp(-dt_ms / YARN_FOLLOW_MS). */
float yarn_follow(float pos, float target, uint32_t dt_ms);
```

Create `components/logic/yarn_rules.c`:

```c
#include "yarn_rules.h"
#include <math.h>

void yarn_basket(uint8_t i, int16_t *x, int16_t *y)
{
    static const int16_t B[YARN_BASKETS][2] = {{330, 318}, {136, 318}, {330, 150}, {136, 150}};
    *x = B[i % YARN_BASKETS][0];
    *y = B[i % YARN_BASKETS][1];
}

void yarn_chase_target(float yx, float yy, float cx, float cy, float *tx, float *ty)
{
    float dx = cx - yx, dy = cy - yy;
    float d = sqrtf(dx * dx + dy * dy);
    if (d < 30.0f) {
        dx = 0.0f;
        dy = -1.0f;
        d = 1.0f;
    }
    *tx = yx + dx / d * YARN_CHASE_PX;
    *ty = yy + dy / d * YARN_CHASE_PX;
}

bool yarn_in_basket(float yx, float yy, int16_t bx, int16_t by)
{
    const float dx = yx - (float)bx, dy = yy - (float)(by - 10);
    return dx * dx + dy * dy < (float)(YARN_WIN_PX * YARN_WIN_PX);
}

float yarn_follow(float pos, float target, uint32_t dt_ms)
{
    return target + (pos - target) * expf(-(float)dt_ms / (float)YARN_FOLLOW_MS);
}
```

- [ ] **Step 4: Run the tests to verify they pass**

Run: `sh test/run.sh`
Expected: every suite passes, including `test_yarn_rules` (6 tests).

- [ ] **Step 5: Commit**

```bash
git add components/logic/include/yarn_rules.h components/logic/yarn_rules.c test/test_yarn_rules.c
git commit -m "feat: Yarn roll rules: baskets, the win and the chasing cat, with host tests"
```

---

### Task 3: Note-colored piano cats and movable big-eye pupils

The piano needs five cats recolored per note (Cat.dc.html's `fur`/`line` props), exported at scale S only. Yarn roll's surprised cat must look at the yarn. The design moves the big eyes' pupils too, but our export bakes them into `eyes_big`, so they become separate parts like the open eyes' pupils.

**Files:**
- Modify: `tools/export_images/cat_geometry.mjs`
- Modify: `tools/export_images/export.mjs`
- Modify: `components/cat/include/cat.h`
- Modify: `components/cat/cat.c`
- Regenerated (commit): `components/assets/assets_manifest.c` (`assets_manifest.h` is unchanged)

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - `lv_obj_t *cat_create_colored(lv_obj_t *parent, cat_scale_t scale, const char *palette);` with palettes `"piano_c"`, `"piano_d"`, `"piano_e"`, `"piano_g"` and `"piano_a"` at `CAT_SCALE_S` (Task 7);
  - `CAT_SURPRISED`'s pupils follow `cat_set_look()` (Task 6);
  - image assets `cat/piano_<n>/S/<part>` for every non-shared part, and the shared parts `pupil_big` and `highlight_big`.

- [ ] **Step 1: Split the big eyes and add the piano palettes to the geometry**

In `tools/export_images/cat_geometry.mjs`, after the `VARIANTS` block, add:

```js
// Meow piano: one cat per note (Spec.dc.html "Piano pitches"), recolored the way Cat.dc.html recolors a
// cat given `fur` and `line` (on the orange base). Exported at scale S only (spec §3.11).
export const PIANO_CATS = {
  piano_c: { fur: '#FF5A5A', line: '#6E1616' },
  piano_d: { fur: '#FF9F3A', line: '#6B3A06' },
  piano_e: { fur: '#FFD84A', line: '#6B5400' },
  piano_g: { fur: '#5CD67A', line: '#1D5A2B' },
  piano_a: { fur: '#5AA8FF', line: '#173F74' },
};
export const pianoPalette = ({ fur, line }) => ({
  ...VARIANTS.orange,
  fur,
  line,
  stripes: false,
  blaze: false,
  muzzle: '#FFF3E0',
  iris: '#2A1633',
});
```

In `catParts`, replace the `eyes_big` entry:

```js
    eyes_big:
      irises(17) +
      '<circle cx="72" cy="104" r="12" fill="#140C1C"/><circle cx="128" cy="104" r="12" fill="#140C1C"/>' +
      '<circle cx="76" cy="98" r="5" fill="#FFFFFF"/><circle cx="132" cy="98" r="5" fill="#FFFFFF"/>',
```

with:

```js
    eyes_big: irises(17), // the pupils are separate so they can look around (design: big eyes move too)
```

and directly after the `highlight:` entry add:

```js
    pupil_big: '<circle cx="72" cy="104" r="12" fill="#140C1C"/>',
    highlight_big: '<circle cx="76" cy="98" r="5" fill="#FFFFFF"/>',
```

- [ ] **Step 2: Export the piano palettes at S**

In `tools/export_images/export.mjs`:

Change the import line to:

```js
import { CANVAS, VARIANTS, PIANO_CATS, pianoPalette, catParts } from './cat_geometry.mjs';
```

Replace:

```js
const variants = Object.keys(VARIANTS);
const partsBy = Object.fromEntries(variants.map((v) => [v, catParts(VARIANTS[v])]));
```

with:

```js
const variants = Object.keys(VARIANTS);
const pianos = Object.keys(PIANO_CATS);
const partsBy = Object.fromEntries([
  ...variants.map((v) => [v, catParts(VARIANTS[v])]),
  ...pianos.map((p) => [p, catParts(pianoPalette(PIANO_CATS[p]))]),
]);
```

(`shared` stays computed over `variants` only. A part that is the same for all three variants uses no palette color at all, so it is the same for the piano cats too.)

Replace:

```js
    const owners = row.shared ? ['shared'] : variants;
```

with:

```js
    // Meow piano's cats exist at S only (spec §3.11). They share the crop box, so positions never differ.
    const owners = row.shared ? ['shared'] : tag === 'S' ? [...variants, ...pianos] : variants;
```

- [ ] **Step 3: Regenerate the assets and check the export**

Run:
```bash
tools/build_assets.sh
ls assets/img/cat/orange/S | wc -l
ls assets/img/cat/piano_c/S | wc -l
ls assets/img/cat/piano_a/M 2>&1 | head -1
git diff --stat components/assets/
git diff components/assets/assets_manifest.c
```
Expected:
- the two `wc -l` counts are equal;
- `piano_a/M` does not exist;
- the manifest diff adds exactly two rows, `{"pupil_big", 1, ...}` and `{"highlight_big", 1, ...}`, raises `CAT_PARTS_COUNT` by 2, and changes nothing else. If the S positions of other parts changed, the piano palettes render outside the variants' crop boxes. Stop and report that.
- `du -sh assets` (printed by the script) is under 12 MB.

- [ ] **Step 4: Colored cats and moving big pupils in the widget**

In `components/cat/include/cat.h`, after the `cat_create` line, add:

```c
/* A cat in one of the extra exported palettes instead of the chosen variant: "piano_c", "piano_d",
 * "piano_e", "piano_g" or "piano_a" (Meow piano, spec §3.11), which exist at CAT_SCALE_S only.
 * palette must outlive the cat (use a string literal). */
lv_obj_t *cat_create_colored(lv_obj_t *parent, cat_scale_t scale, const char *palette);
```

In `components/cat/cat.c`:

1. In `cat_t`, after `cat_scale_t scale;`, add:

```c
    const char *palette; /* NULL: the chosen variant (cat_set_variant) */
```

2. Replace `cat_part_asset`:

```c
void cat_part_asset(char *out, size_t n, cat_scale_t scale, const char *part)
{
    const cat_part_pos_t *p = find_part(part);
    snprintf(out, n, "cat/%s/%c/%s", p && p->shared ? "shared" : VARIANT_NAME[s_variant], SCALE_C[scale], part);
}
```

with:

```c
static void asset_name(char *out, size_t n, cat_scale_t scale, const char *palette, const char *part)
{
    const cat_part_pos_t *p = find_part(part);
    const char *owner = p && p->shared ? "shared" : palette ? palette : VARIANT_NAME[s_variant];
    snprintf(out, n, "cat/%s/%c/%s", owner, SCALE_C[scale], part);
}

void cat_part_asset(char *out, size_t n, cat_scale_t scale, const char *part)
{
    asset_name(out, n, scale, NULL, part);
}
```

3. In `place()`, replace `cat_part_asset(name, sizeof(name), c->scale, part);` with:

```c
        asset_name(name, sizeof(name), c->scale, c->palette, part);
```

4. In `apply()`, replace the eyes block, from `const char *eyes;` through the `place(c, L_HL_R, ...)` line, with:

```c
    const char *eyes;
    bool pupils = false, big = false;
    switch (e->eyes) {
    case EYES_OPEN:
        eyes = BLINK_PART[c->blink];
        pupils = c->blink == 0;
        break;
    case EYES_HAPPY: eyes = "eyes_happy"; break;
    case EYES_BIG:
        eyes = "eyes_big"; /* irises only: the big pupils follow cat_set_look like the open ones (design) */
        pupils = big = true;
        break;
    case EYES_HALF: eyes = "eyes_half"; break;
    case EYES_CLOSED: eyes = "eyes_closed"; break;
    default: eyes = SPIRAL_PART[t % 4]; break;
    }
    place(c, L_EYES, eyes, 0, 0);
    float lx = c->look_set ? c->look_x : e->look_x10 / 10.0f;
    float ly = c->look_set ? c->look_y : e->look_y10 / 10.0f;
    int16_t ox = sc(c, lx * LOOK_RANGE), oy = sc(c, ly * LOOK_RANGE), gap = sc(c, EYE_SPACING);
    const char *pupil = big ? "pupil_big" : "pupil", *hl = big ? "highlight_big" : "highlight";
    place(c, L_PUPIL_L, pupils ? pupil : NULL, ox, oy);
    place(c, L_PUPIL_R, pupils ? pupil : NULL, (int16_t)(gap + ox), oy);
    place(c, L_HL_L, pupils ? hl : NULL, ox, oy);
    place(c, L_HL_R, pupils ? hl : NULL, (int16_t)(gap + ox), oy);
```

5. Rename `lv_obj_t *cat_create(lv_obj_t *parent, cat_scale_t scale)` to `lv_obj_t *cat_create_colored(lv_obj_t *parent, cat_scale_t scale, const char *palette)`. In its body, after `c->scale = scale;`, add `c->palette = palette;`. Then add, right after that function:

```c
lv_obj_t *cat_create(lv_obj_t *parent, cat_scale_t scale) { return cat_create_colored(parent, scale, NULL); }
```

- [ ] **Step 5: Build**

Run:
```bash
idf.py build
idf.py -B build-gallery -D SDKCONFIG=build-gallery/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.gallery;sdkconfig.perf" build
```
Expected: both `Project build complete`, no warnings in `components/cat`.

The look change needs eyes on the board. Task 8 checks it:
- the menu's surprised face (the Magic paws icon) looks as before;
- the yarn cat's eyes follow the yarn.

- [ ] **Step 6: Commit**

```bash
git add tools/export_images/cat_geometry.mjs tools/export_images/export.mjs components/cat/include/cat.h components/cat/cat.c components/assets/assets_manifest.c
git commit -m "feat: note-colored piano cats and big-eye pupils that can look around"
```

---

### Task 4: Magic paws, Yarn roll and Meow piano images and sounds

**Files:**
- Modify: `tools/export_images/ui_images.mjs`
- Modify: `tools/build_sounds/synth.py`
- Modify: `tools/build_sounds/sounds.txt`
- Modify: `components/assets/include/sounds.h`
- Modify: `components/assets/sounds.c`
- Regenerated (commit): `components/assets/assets_manifest.c`

**Interfaces:**
- Consumes: nothing new.
- Produces:
  - UI images (all uncropped, drawn at their design boxes):
    - `ui/brush_rainbow`, `ui/brush_paw` (150 × 150);
    - `ui/draw_hint` (160 × 120), `ui/dust` (266 × 60);
    - `ui/yarn_f0` … `ui/yarn_f3` (64 × 64);
    - `ui/yarn_basket` (120 × 96), `ui/yarn_basket_full` (120 × 110);
    - `ui/tilt_hint` (60 × 44);
    - `ui/piano_note` (100 × 100, white, for recoloring).
  - Sound ids, appended before `SND_COUNT`: `SND_SPARKLE`, `SND_PAT`, `SND_SWISH`, `SND_CISTO`, `SND_RUMBLE` (a loop), `SND_YARN_WIN`, `SND_PIANO_C5`, `SND_PIANO_D5`, `SND_PIANO_E5`, `SND_PIANO_G5`, `SND_PIANO_A5`.

- [ ] **Step 1: The new UI images**

In `tools/export_images/ui_images.mjs`, after the `ARCH` constant, add:

```js
// Magic paws brush button: black disc with a white ring at 45 % (Screen.dc.html draw view).
const BRUSH_DISC = '<circle cx="75" cy="75" r="70" fill="#000000" stroke="#FFFFFF" stroke-opacity="0.45" stroke-width="6"/>';
// The yarn with its strands turned by `deg`: four frames make one full roll (Assets.dc.html yarn_ball_f0-f3).
const yarn = (deg) =>
  '<circle cx="32" cy="32" r="27" fill="#FF7FB0" stroke="#A8326A" stroke-width="5"/>' +
  `<g transform="rotate(${deg} 32 32)"><path d="M10 26 Q32 16 54 30 M8 38 Q32 28 56 42 M22 9 Q42 30 34 58" fill="none" stroke="#B8356F" stroke-width="4" stroke-linecap="round"/></g>`;
const BASKET_WEAVE = (y0) =>
  `<path d="M36 ${y0 + 2} L42 ${y0 + 58} M60 ${y0 + 2} L60 ${y0 + 58} M84 ${y0 + 2} L78 ${y0 + 58} M14 ${y0 + 24} L106 ${y0 + 24} M18 ${y0 + 42} L102 ${y0 + 42}" fill="none" stroke="#6B3A0C" stroke-width="4"/>`;
const basket = (y0) =>
  `<path d="M8 ${y0} L112 ${y0} L98 ${y0 + 60} L22 ${y0 + 60} Z" fill="#C98B4A" stroke="#6B3A0C" stroke-width="6" stroke-linejoin="round"/>` +
  BASKET_WEAVE(y0) +
  `<path d="M6 ${y0} L114 ${y0}" fill="none" stroke="#E6A865" stroke-width="10" stroke-linecap="round"/>`;
```

(`basket(30)` reproduces the design's empty basket exactly; `basket(44)` the full one.)

Append to the `UI_IMAGES` array:

```js
  {
    name: 'ui/brush_rainbow',
    viewBox: [0, 0, 150, 150],
    crop: false,
    svg:
      BRUSH_DISC +
      '<path d="M35 95 A40 40 0 0 1 115 95" fill="none" stroke="#FF5A5A" stroke-width="12" stroke-linecap="round"/>' +
      '<path d="M53 95 A22 22 0 0 1 97 95" fill="none" stroke="#FFD84A" stroke-width="12" stroke-linecap="round"/>' +
      '<circle cx="75" cy="95" r="6" fill="#5CE0FF"/>',
  },
  {
    name: 'ui/brush_paw',
    viewBox: [0, 0, 150, 150],
    crop: false,
    svg:
      BRUSH_DISC +
      '<ellipse cx="75" cy="88" rx="22" ry="18" fill="#FFB0C8"/>' +
      '<circle cx="48" cy="62" r="9" fill="#FFB0C8"/><circle cx="65" cy="48" r="9" fill="#FFB0C8"/>' +
      '<circle cx="85" cy="48" r="9" fill="#FFB0C8"/><circle cx="102" cy="62" r="9" fill="#FFB0C8"/>',
  },
  {
    name: 'ui/draw_hint',
    viewBox: [0, 0, 160, 120],
    crop: false,
    svg:
      '<circle cx="20" cy="90" r="8" fill="#FF5A5A"/><circle cx="46" cy="66" r="8" fill="#FF9F3A"/>' +
      '<circle cx="76" cy="54" r="8" fill="#FFD84A"/><circle cx="106" cy="60" r="8" fill="#5CD67A"/>' +
      '<circle cx="132" cy="80" r="8" fill="#4FD8FF"/>',
  },
  {
    name: 'ui/dust',
    viewBox: [0, 0, 266, 60],
    crop: false,
    svg:
      '<circle cx="20" cy="40" r="14" fill="#8A8AA0"/><circle cx="44" cy="28" r="10" fill="#8A8AA0"/>' +
      '<circle cx="230" cy="36" r="14" fill="#8A8AA0"/><circle cx="250" cy="20" r="9" fill="#8A8AA0"/>',
  },
  ...[0, 90, 180, 270].map((deg, i) => ({ name: `ui/yarn_f${i}`, viewBox: [0, 0, 64, 64], crop: false, svg: yarn(deg) })),
  { name: 'ui/yarn_basket', viewBox: [0, 0, 120, 96], crop: false, svg: basket(30) },
  {
    name: 'ui/yarn_basket_full',
    viewBox: [0, 0, 120, 110],
    crop: false,
    svg: '<circle cx="60" cy="38" r="26" fill="#FF7FB0" stroke="#A8326A" stroke-width="5"/>' + basket(44),
  },
  {
    name: 'ui/tilt_hint',
    viewBox: [0, 0, 60, 44],
    crop: false,
    svg:
      '<rect x="6" y="10" width="48" height="24" rx="12" fill="none" stroke="#FFFFFF" stroke-opacity="0.5" stroke-width="5"/>' +
      '<circle cx="30" cy="22" r="5" fill="#FF7FB0"/>',
  },
  {
    name: 'ui/piano_note',
    viewBox: [0, 0, 100, 100],
    crop: false,
    svg:
      '<circle cx="50" cy="50" r="44" fill="none" stroke="#FFFFFF" stroke-opacity="0.6" stroke-width="6" stroke-dasharray="0.1 14" stroke-linecap="round"/>' +
      '<path d="M58 26 L58 62" fill="none" stroke="#FFFFFF" stroke-width="7" stroke-linecap="round"/>' +
      '<ellipse cx="48" cy="64" rx="12" ry="9" fill="#FFFFFF"/>' +
      '<path d="M58 26 Q74 30 76 46 Q68 38 58 38 Z" fill="#FFFFFF"/>',
  },
```

- [ ] **Step 2: The new sounds**

In `tools/build_sounds/synth.py`, in `RECIPES`, after the `"bell_tick"` line, add:

```python
    "pat": lambda: sweep(700, 500, 0.06, harmonics=(1, 0, 0.11, 0, 0.04), release=0.8),  # design: 700->500 Hz triangle
```

In `tools/build_sounds/sounds.txt`, after the `fanfare` synth line, add:

```
pat           synth   pat
```

and after the `swipe_piano` line at the end, add:

```
yarn_win      concat  fanfare+bravo
```

In `components/assets/include/sounds.h`, before `SND_COUNT`, add:

```c
    SND_SPARKLE,
    SND_PAT,
    SND_SWISH,
    SND_CISTO,
    SND_RUMBLE, /* a loop: audio_loop_start() */
    SND_YARN_WIN,
    SND_PIANO_C5,
    SND_PIANO_D5,
    SND_PIANO_E5,
    SND_PIANO_G5,
    SND_PIANO_A5,
```

In `components/assets/sounds.c`, after `[SND_PET_EAT] = "pet_eat",` add:

```c
    [SND_SPARKLE] = "sparkle",
    [SND_PAT] = "pat",
    [SND_SWISH] = "swish",
    [SND_CISTO] = "cisto",
    [SND_RUMBLE] = "rumble",
    [SND_YARN_WIN] = "yarn_win",
    [SND_PIANO_C5] = "piano_c5",
    [SND_PIANO_D5] = "piano_d5",
    [SND_PIANO_E5] = "piano_e5",
    [SND_PIANO_G5] = "piano_g5",
    [SND_PIANO_A5] = "piano_a5",
```

- [ ] **Step 3: Regenerate and check**

Run:
```bash
tools/build_assets.sh
for f in $(sed -n 's/.*\] = "\(.*\)",/\1/p' components/assets/sounds.c); do [ -f "assets/snd/$f.wav" ] || echo "missing sound $f"; done
for n in brush_rainbow brush_paw draw_hint dust yarn_f0 yarn_f1 yarn_f2 yarn_f3 yarn_basket yarn_basket_full tilt_hint piano_note; do [ -f "assets/img/ui/$n.bin" ] || echo "missing image $n"; done
git diff --stat components/assets/
```
Expected:
- no `missing` line;
- the manifest diff adds exactly 12 `UI_IMAGE_POS` rows, all at `0, 0`, raises `UI_IMAGE_POS_COUNT` by 12, and changes nothing else;
- `du -sh assets` is under 12 MB.

Open `build-assets/preview/index.html` in the browser pane (`preview_start` with a `file://` URL) and compare the new `ui/` images with the design's draw, yarn, yarnwin and piano views (`design/claude-design/Screen.dc.html`). Each should match its design box, and the four yarn frames should show the strands turned a quarter turn each.

- [ ] **Step 4: Build**

Run: `idf.py build`
Expected: `Project build complete`.

- [ ] **Step 5: Commit**

```bash
git add tools/export_images/ui_images.mjs tools/build_sounds/synth.py tools/build_sounds/sounds.txt components/assets/include/sounds.h components/assets/sounds.c components/assets/assets_manifest.c
git commit -m "feat: Magic paws, Yarn roll and Meow piano images and sounds"
```

---

### Task 5: Magic paws app

**Files:**
- Create: `components/ui/apps/draw.c`
- Modify: `components/ui/include/tokens.h` (append)
- Modify: `components/ui/apps/placeholder.c` (remove the `app_draw` line)

**Interfaces:**
- Consumes:
  - Task 1's `paint.h` and `brush.h`;
  - Task 4's images `ui/brush_rainbow`, `ui/brush_paw`, `ui/draw_hint`, `ui/dust` and sounds `SND_SPARKLE`, `SND_PAT`, `SND_SWISH`, `SND_CISTO`;
  - `cat_create()` and `CAT_SWEEP`;
  - LVGL 9.4: `lv_canvas_set_buffer(obj, buf, w, h, LV_COLOR_FORMAT_RGB565)` wraps our buffer without copying or freeing it. The image cache is off (`CONFIG_LV_CACHE_DEF_SIZE=0`), so writing pixels and calling `lv_obj_invalidate_area(obj, &area)` (absolute coordinates) shows them.
- Produces: `const app_t app_draw` with `on_shake`, and `RAINBOW_COLORS` in `tokens.h` (Task 6 uses it for confetti).

- [ ] **Step 1: Tokens**

Append to `components/ui/include/tokens.h`:

```c
/* Rainbow brush (Spec.dc.html): Magic paws dots and prints, Yarn roll confetti. An array initializer. */
#define RAINBOW_COLORS {0xFF5A5A, 0xFF9F3A, 0xFFD84A, 0x5CD67A, 0x4FD8FF, 0x5A8CFF, 0xB07CFF}

/* Magic paws (Screen.dc.html draw and erase views; spec §3.11) */
#define DRAW_DOT_R           8.0f  /* 16 px dots */
#define DRAW_HALO_R          15.0f /* 30 px halo */
#define DRAW_HALO_OPA        71    /* 28 % */
#define DRAW_BUTTON_X        158   /* the 150 px brush button */
#define DRAW_BUTTON_Y        300
#define DRAW_HINT_X          153
#define DRAW_HINT_Y          120
#define DRAW_DUST_X          100
#define DRAW_DUST_Y          360
#define DRAW_TWINKLE_MS      3000  /* 30 % -> 100 % -> 30 % */
#define DRAW_TWINKLE_LOW_OPA 77    /* 30 % */
#define DRAW_SPARKLE_EVERY   8     /* dots per sparkle tick */
#define DRAW_SOUND_GAP_MS    125   /* sparkles and pats: at most 8 per second */
#define DRAW_SWEEP_OPA       77    /* the drawing at 30 % while the cat sweeps */
#define DRAW_SWEEP_CAT_X     133   /* scale M origin */
#define DRAW_SWEEP_CAT_Y     196
#define DRAW_SWEEP_PX        60
#define DRAW_SWING_MS        550   /* one swing there and back */
#define DRAW_CISTO_MS        900
#define DRAW_CLEAN_MS        1700
```

- [ ] **Step 2: The app**

Create `components/ui/apps/draw.c`:

```c
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "app.h"
#include "assets.h"
#include "audio.h"
#include "brush.h"
#include "cat.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "paint.h"
#include "tokens.h"
#include "ui_util.h"

/* Magic paws (design: Screen.dc.html draw and erase views, Main.dc.html act('draw' | 'tool') and
 * shake(); spec §3.11). Rainbow dots or paw prints under her finger; a shake sweeps them away. */

static const char *TAG = "draw";
static const uint32_t RAINBOW[BRUSH_COLORS] = RAINBOW_COLORS;

/* The picture and its brush outlive the app on purpose: she finds her drawing again until she
 * shakes it clean or the toy restarts (spec §3.11, the §4.4 exception). */
static uint16_t *s_pixels;
static brush_t s_brush;
static bool s_empty = true;

static lv_obj_t *s_canvas; /* NULL once destroy() has run: every callback checks it first (app.h) */
static lv_obj_t *s_screen, *s_button, *s_hint, *s_dust, *s_sweeper;
static lv_timer_t *s_cisto_timer, *s_clean_timer;
static uint16_t s_colors[BRUSH_COLORS];
static bool s_sweeping;
static uint32_t s_sound_ms; /* last sparkle or pat */

/* ---- helpers ---- */

static lv_timer_t *one_shot(lv_timer_cb_t cb, uint32_t ms)
{
    lv_timer_t *t = lv_timer_create(cb, ms, NULL);
    lv_timer_set_repeat_count(t, 1);
    return t;
}

static void cancel_timers(void)
{
    if (s_cisto_timer) {
        lv_timer_delete(s_cisto_timer);
        s_cisto_timer = NULL;
    }
    if (s_clean_timer) {
        lv_timer_delete(s_clean_timer);
        s_clean_timer = NULL;
    }
}

static void opa_step(void *obj, int32_t v) { lv_obj_set_style_image_opa(obj, (lv_opa_t)v, 0); }

/* Shows or hides an image; shown, it twinkles like the design's .twinkle (30 % -> 100 % -> 30 %, 3 s). */
static void twinkle(lv_obj_t *img, bool on)
{
    lv_anim_delete(img, opa_step);
    ui_set_hidden(img, !on);
    if (!on) {
        return;
    }
    lv_obj_set_style_image_opa(img, DRAW_TWINKLE_LOW_OPA, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, img);
    lv_anim_set_exec_cb(&a, opa_step);
    lv_anim_set_values(&a, DRAW_TWINKLE_LOW_OPA, LV_OPA_COVER);
    lv_anim_set_duration(&a, DRAW_TWINKLE_MS / 2);
    lv_anim_set_reverse_duration(&a, DRAW_TWINKLE_MS / 2);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static const char *button_image(void)
{
    return s_brush.kind == BRUSH_RAINBOW ? "ui/brush_rainbow" : "ui/brush_paw"; /* shows the current brush */
}

/* ---- painting ---- */

static void clear_picture(void)
{
    memset(s_pixels, 0, SCREEN_SIZE * SCREEN_SIZE * sizeof(uint16_t));
    brush_clear(&s_brush);
    s_empty = true;
}

static void paint_stamps(const stamp_t *st, size_t n)
{
    paint_rect_t d;
    paint_rect_reset(&d);
    for (size_t i = 0; i < n; i++) {
        const uint16_t c = s_colors[st[i].color];
        const float x = st[i].x + 0.5f, y = st[i].y + 0.5f; /* the middle of the touched pixel */
        if (s_brush.kind == BRUSH_RAINBOW) {
            paint_circle(s_pixels, SCREEN_SIZE, SCREEN_SIZE, x, y, DRAW_HALO_R, c, DRAW_HALO_OPA, &d);
            paint_circle(s_pixels, SCREEN_SIZE, SCREEN_SIZE, x, y, DRAW_DOT_R, c, LV_OPA_COVER, &d);
        } else {
            blob_t b[5];
            paw_blobs(&st[i], b);
            for (int k = 0; k < 5; k++) {
                paint_circle(s_pixels, SCREEN_SIZE, SCREEN_SIZE, b[k].x + 0.5f, b[k].y + 0.5f, b[k].r, c,
                             LV_OPA_COVER, &d);
            }
        }
    }
    if (d.x1 < d.x0) {
        return; /* nothing changed */
    }
    lv_area_t a;
    lv_obj_get_coords(s_canvas, &a);
    const lv_area_t changed = {a.x1 + d.x0, a.y1 + d.y0, a.x1 + d.x1, a.y1 + d.y1};
    lv_obj_invalidate_area(s_canvas, &changed);
    if (s_empty) {
        s_empty = false;
        twinkle(s_hint, false);
    }
}

/* A sparkle every DRAW_SPARKLE_EVERY dots, a pat per print; each at most 8 per second (spec §3.11). */
static void stroke_sound(uint32_t dots_before, uint32_t paws_before)
{
    const bool rainbow = s_brush.kind == BRUSH_RAINBOW;
    const bool due = rainbow ? s_brush.dots / DRAW_SPARKLE_EVERY != dots_before / DRAW_SPARKLE_EVERY
                             : s_brush.paws != paws_before;
    if (!due || lv_tick_elaps(s_sound_ms) < DRAW_SOUND_GAP_MS) {
        return;
    }
    audio_play(rainbow ? SND_SPARKLE : SND_PAT);
    s_sound_ms = lv_tick_get();
}

/* ---- touch ---- */

static void touch(lv_event_t *e)
{
    if (!s_canvas || !s_pixels || s_sweeping) {
        return; /* closed (app.h), no picture, or the cat is sweeping (spec §3.11) */
    }
    const lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        brush_up(&s_brush);
        return;
    }
    lv_indev_t *indev = lv_indev_active();
    if (!indev) {
        return;
    }
    lv_point_t p;
    lv_indev_get_point(indev, &p);
    lv_area_t a;
    lv_obj_get_coords(s_canvas, &a);
    const int16_t x = (int16_t)(p.x - a.x1), y = (int16_t)(p.y - a.y1);
    const uint32_t dots = s_brush.dots, paws = s_brush.paws;
    stamp_t st[BRUSH_MAX_STAMPS];
    const size_t n = code == LV_EVENT_PRESSED ? brush_down(&s_brush, x, y, st) : brush_move(&s_brush, x, y, st);
    paint_stamps(st, n);
    stroke_sound(dots, paws);
}

static void tool_tapped(lv_event_t *e)
{
    (void)e;
    if (!s_canvas || s_sweeping) {
        return;
    }
    s_brush.kind = s_brush.kind == BRUSH_RAINBOW ? BRUSH_PAWS : BRUSH_RAINBOW;
    lv_image_set_src(s_button, assets_image(button_image()));
    audio_play(SND_BLIP_UP);
}

/* ---- sweep (shake) ---- */

static void sweep_x(void *obj, int32_t v) { lv_obj_set_style_translate_x(obj, v, 0); }

static void say_cisto(lv_timer_t *t)
{
    (void)t;
    s_cisto_timer = NULL; /* one-shot timers delete themselves */
    if (!s_canvas) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    audio_play(SND_CISTO);
}

static void sweep_done(lv_timer_t *t)
{
    (void)t;
    s_clean_timer = NULL;
    if (!s_canvas) {
        return;
    }
    s_sweeping = false;
    if (s_pixels) {
        clear_picture();
    }
    lv_anim_delete(s_sweeper, sweep_x);
    lv_obj_delete(s_sweeper);
    s_sweeper = NULL;
    twinkle(s_dust, false);
    lv_obj_set_style_image_opa(s_canvas, LV_OPA_COVER, 0);
    lv_obj_invalidate(s_canvas);
    ui_set_hidden(s_button, false);
    twinkle(s_hint, true);
}

static void draw_shake(void)
{
    if (!s_canvas || s_sweeping) {
        return;
    }
    s_sweeping = true;
    brush_up(&s_brush);
    lv_obj_set_style_image_opa(s_canvas, DRAW_SWEEP_OPA, 0);
    ui_set_hidden(s_button, true);
    twinkle(s_hint, false);

    s_sweeper = cat_create(s_screen, CAT_SCALE_M);
    cat_set_origin(s_sweeper, DRAW_SWEEP_CAT_X, DRAW_SWEEP_CAT_Y);
    cat_set_state(s_sweeper, CAT_SWEEP);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_sweeper);
    lv_anim_set_exec_cb(&a, sweep_x);
    lv_anim_set_values(&a, -DRAW_SWEEP_PX, DRAW_SWEEP_PX);
    lv_anim_set_duration(&a, DRAW_SWING_MS / 2);
    lv_anim_set_reverse_duration(&a, DRAW_SWING_MS / 2);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);

    twinkle(s_dust, true);
    lv_obj_move_to_index(s_dust, -1); /* design order: the dust over the cat */
    audio_play(SND_SWISH);
    s_cisto_timer = one_shot(say_cisto, DRAW_CISTO_MS);
    s_clean_timer = one_shot(sweep_done, DRAW_CLEAN_MS);
}

/* ---- screen ---- */

static void build(lv_obj_t *scr)
{
    s_canvas = lv_canvas_create(scr);
    lv_obj_remove_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE); /* touches go to the screen */
    lv_obj_set_pos(s_canvas, 0, 0);
    if (s_pixels) {
        lv_canvas_set_buffer(s_canvas, s_pixels, SCREEN_SIZE, SCREEN_SIZE, LV_COLOR_FORMAT_RGB565);
    }
    s_hint = ui_image(scr, "ui/draw_hint", DRAW_HINT_X, DRAW_HINT_Y);
    twinkle(s_hint, s_empty);
    s_dust = ui_image(scr, "ui/dust", DRAW_DUST_X, DRAW_DUST_Y);
    ui_set_hidden(s_dust, true);
    s_button = ui_image(scr, button_image(), DRAW_BUTTON_X, DRAW_BUTTON_Y);
    lv_obj_add_flag(s_button, LV_OBJ_FLAG_CLICKABLE); /* presses on it never reach the screen */
    lv_obj_add_event_cb(s_button, tool_tapped, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(scr, touch, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr, touch, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(scr, touch, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(scr, touch, LV_EVENT_PRESS_LOST, NULL);
}

static void draw_create(lv_obj_t *screen)
{
    s_screen = screen;
    s_sweeping = false;
    s_sound_ms = 0;
    for (size_t i = 0; i < BRUSH_COLORS; i++) {
        s_colors[i] = paint_rgb565(RAINBOW[i]);
    }
    if (!s_pixels) {
        /* The one PSRAM block an app keeps after destroy() (spec §3.11, §4.4). */
        s_pixels = heap_caps_calloc(SCREEN_SIZE * SCREEN_SIZE, sizeof(uint16_t), MALLOC_CAP_SPIRAM);
        if (!s_pixels) {
            ESP_LOGE(TAG, "no PSRAM for the picture: drawing is off until the next open");
        }
        brush_init(&s_brush, BRUSH_RAINBOW, SCREEN_CENTER, SCREEN_CENTER);
        s_empty = true;
    }
    brush_up(&s_brush);
    build(screen);
}

static void draw_destroy(void)
{
    cancel_timers();
    if (s_sweeping) {
        s_sweeping = false;
        if (s_pixels) {
            clear_picture(); /* she already shook it clean (spec §3.11) */
        }
    }
    if (s_sweeper) {
        lv_anim_delete(s_sweeper, sweep_x);
    }
    lv_anim_delete(s_dust, opa_step);
    lv_anim_delete(s_hint, opa_step);
    brush_up(&s_brush);
    s_canvas = NULL;
    s_screen = s_button = s_hint = s_dust = s_sweeper = NULL;
    /* the swish or "Čisto!": app_manager calls audio_stop_all() after this */
}

const app_t app_draw = {
    .id = "draw",
    .icon = "apps/draw",
    .icon_small = "apps/draw_96",
    .color = COLOR_DRAW,
    .name_sound = SND_NAME_DRAW,
    .swipe_sound = SND_SWIPE_DRAW,
    .react = CAT_SURPRISED,
    .create = draw_create,
    .destroy = draw_destroy,
    .on_shake = draw_shake,
};
```

- [ ] **Step 3: Remove the placeholder line**

In `components/ui/apps/placeholder.c`, delete the line:

```c
PLACEHOLDER(app_draw, "draw", COLOR_DRAW, SND_NAME_DRAW, SND_SWIPE_DRAW, CAT_SURPRISED)
```

and change the comment above the remaining lines to:

```c
/* Remove a line when that app's real file lands (voices: Task 17, pet: M4, draw: M5). */
```

- [ ] **Step 4: Build**

Run:
```bash
idf.py reconfigure build
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" reconfigure build
```
Expected: both `Project build complete`, no warnings in `components/ui/apps/draw.c`.

- [ ] **Step 5: Commit**

```bash
git add components/ui/apps/draw.c components/ui/include/tokens.h components/ui/apps/placeholder.c
git commit -m "feat: Magic paws: rainbow dots and paw prints, shaken clean by a sweeping cat"
```

---

### Task 6: Yarn roll app

**Files:**
- Create: `components/ui/apps/yarn.c`
- Modify: `components/ui/include/tokens.h` (append)
- Modify: `components/ui/apps/placeholder.c` (remove the `app_yarn` line)

**Interfaces:**
- Consumes:
  - Task 2's `yarn_rules.h`;
  - M4's `motion.h` (`roll_step`);
  - Task 3's movable big pupils (`cat_set_look` on `CAT_SURPRISED`);
  - Task 4's images `ui/yarn_f0`…`ui/yarn_f3`, `ui/yarn_basket`, `ui/yarn_basket_full`, `ui/tilt_hint` and sounds `SND_RUMBLE`, `SND_YARN_WIN`;
  - Task 5's `RAINBOW_COLORS`.
- Produces: `const app_t app_yarn` with `on_shake` and `on_tilt`.

- [ ] **Step 1: Tokens**

Append to `components/ui/include/tokens.h`:

```c
/* Yarn roll (Screen.dc.html yarn and yarnwin views; spec §3.11). Range, chase and win: yarn_rules.h. */
#define YARN_SIZE               64
#define YARN_FRAMES             4
#define YARN_FRAME_PX           42   /* a quarter turn of the r 27 yarn per 42 px rolled */
#define YARN_ROLL_MIN_PX        0.5f /* less than this per step is not rolling (not activity, no rumble) */
#define YARN_RUMBLE_STOP_MS     150  /* the rumble stops this long after the yarn stops */
#define YARN_BASKET_W           120
#define YARN_BASKET_H           96
#define YARN_CAT_DX             65   /* scale S: from the cat's center to its origin */
#define YARN_CAT_DY             68
#define YARN_LOOK_STEP          0.1f /* re-aim the eyes only when the look moves this much */
#define YARN_HINT_X             203
#define YARN_HINT_Y             386
#define YARN_HINT_MS            4000
#define YARN_HINT_WOBBLE        70   /* ±7°, in 0.1° */
#define YARN_HINT_WOBBLE_MS     600  /* there and back */
#define YARN_HINT_PIVOT_X       30   /* 50 % and 80 % of the 60 x 44 hint */
#define YARN_HINT_PIVOT_Y       35
#define YARN_WIN_DELAY_MS       320
#define YARN_NEXT_ROUND_MS      3500
#define YARN_WIN_CAT_X          133  /* scale M origin, centered where the design's 220 px proud cat is */
#define YARN_WIN_CAT_Y          68
#define YARN_FULL_BASKET_X      173
#define YARN_FULL_BASKET_Y      296
#define YARN_CONFETTI           24
#define YARN_CONFETTI_W         12
#define YARN_CONFETTI_H         22
#define YARN_CONFETTI_R         3
#define YARN_CONFETTI_X0        60   /* x = 60 + (i * 53 mod 346) */
#define YARN_CONFETTI_X_STEP    53
#define YARN_CONFETTI_X_SPAN    346
#define YARN_CONFETTI_TOP       (-30)
#define YARN_CONFETTI_DELAY     97   /* delay = i * 97 mod 1400 ms */
#define YARN_CONFETTI_DELAY_MAX 1400
#define YARN_CONFETTI_FALL      520
#define YARN_CONFETTI_MS        1600
#define YARN_CONFETTI_SPIN      1    /* 0 = the spec §3.11 fallback: confetti falls without spinning */
#define YARN_CONFETTI_TURN      5400 /* 540°, in 0.1° */
```

- [ ] **Step 2: The app**

Create `components/ui/apps/yarn.c`:

```c
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include "app.h"
#include "assets.h"
#include "audio.h"
#include "cat.h"
#include "motion.h"
#include "tokens.h"
#include "ui_util.h"
#include "yarn_rules.h"

/* Yarn roll (design: Screen.dc.html yarn and yarnwin views, Main.dc.html tilt() and shake(); spec §3.10
 * tilt rules and §3.11). Tilt the toy to roll the yarn into the basket while the cat chases it. */

typedef enum { Y_PLAY, Y_HIT, Y_WIN } phase_t;

static const char *const FRAME[YARN_FRAMES] = {"ui/yarn_f0", "ui/yarn_f1", "ui/yarn_f2", "ui/yarn_f3"};
static const uint32_t CONFETTI_COLORS[] = RAINBOW_COLORS;
#define CONFETTI_COLOR_COUNT (sizeof(CONFETTI_COLORS) / sizeof(CONFETTI_COLORS[0]))

static lv_obj_t *s_yarn; /* NULL once destroy() has run: every callback checks it first (app.h) */
static lv_obj_t *s_screen, *s_basket, *s_cat, *s_hint, *s_win;
static lv_obj_t *s_confetti[YARN_CONFETTI];
static lv_timer_t *s_hint_timer, *s_win_timer, *s_round_timer;
static phase_t s_phase;
static uint8_t s_round;        /* which basket spot */
static int16_t s_bx, s_by;     /* the basket's center */
static float s_x, s_y;         /* the yarn's center */
static float s_cat_x, s_cat_y; /* the chasing cat's center */
static float s_look_x, s_look_y;
static uint8_t s_frame;
static float s_frame_px; /* rolled since the last frame change */
static bool s_rumbling;
static uint32_t s_last_tilt_ms, s_last_roll_ms;

/* ---- helpers ---- */

static lv_timer_t *one_shot(lv_timer_cb_t cb, uint32_t ms)
{
    lv_timer_t *t = lv_timer_create(cb, ms, NULL);
    lv_timer_set_repeat_count(t, 1);
    return t;
}

static void cancel_timers(void)
{
    lv_timer_t **all[] = {&s_hint_timer, &s_win_timer, &s_round_timer};
    for (size_t i = 0; i < sizeof(all) / sizeof(all[0]); i++) {
        if (*all[i]) {
            lv_timer_delete(*all[i]);
            *all[i] = NULL;
        }
    }
}

static void rumble(bool on)
{
    if (on == s_rumbling) {
        return;
    }
    s_rumbling = on;
    if (on) {
        audio_loop_start(SND_RUMBLE);
    } else {
        audio_loop_stop();
    }
}

/* ---- tilt hint ---- */

static void wobble_step(void *obj, int32_t v) { lv_image_set_rotation(obj, v); }

static void hint_off(void)
{
    lv_anim_delete(s_hint, wobble_step);
    ui_set_hidden(s_hint, true);
}

static void hint_done(lv_timer_t *t)
{
    (void)t;
    s_hint_timer = NULL; /* one-shot timers delete themselves */
    if (!s_yarn) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    hint_off();
}

/* ---- the yarn and the chasing cat ---- */

static void place_yarn(void)
{
    lv_obj_set_pos(s_yarn, (int32_t)lroundf(s_x) - YARN_SIZE / 2, (int32_t)lroundf(s_y) - YARN_SIZE / 2);
}

/* The strands turn a quarter per YARN_FRAME_PX rolled, one way or the other. */
static void roll_frames(float moved, bool forward)
{
    s_frame_px += moved;
    if (s_frame_px < YARN_FRAME_PX) {
        return;
    }
    s_frame_px -= YARN_FRAME_PX;
    s_frame = (uint8_t)((s_frame + (forward ? 1 : YARN_FRAMES - 1)) % YARN_FRAMES);
    lv_image_set_src(s_yarn, assets_image(FRAME[s_frame]));
}

/* The cat eases toward its spot behind the yarn and looks at the yarn (design yarn view). snap: jump there. */
static void chase(uint32_t dt_ms, bool snap)
{
    float tx, ty;
    yarn_chase_target(s_x, s_y, SCREEN_CENTER, SCREEN_CENTER, &tx, &ty);
    s_cat_x = snap ? tx : yarn_follow(s_cat_x, tx, dt_ms);
    s_cat_y = snap ? ty : yarn_follow(s_cat_y, ty, dt_ms);
    cat_set_origin(s_cat, (int32_t)lroundf(s_cat_x) - YARN_CAT_DX, (int32_t)lroundf(s_cat_y) - YARN_CAT_DY);
    const float lx = (s_x - tx) / YARN_CHASE_PX, ly = (s_y - ty) / YARN_CHASE_PX; /* from its spot to the yarn */
    if (snap || fabsf(lx - s_look_x) + fabsf(ly - s_look_y) >= YARN_LOOK_STEP) {
        s_look_x = lx;
        s_look_y = ly;
        cat_set_look(s_cat, lx, ly);
    }
}

/* ---- rounds ---- */

static void start_round(void)
{
    s_phase = Y_PLAY;
    s_x = s_y = SCREEN_CENTER;
    yarn_basket(s_round, &s_bx, &s_by);
    lv_obj_set_pos(s_basket, s_bx - YARN_BASKET_W / 2, s_by - YARN_BASKET_H / 2);
    place_yarn();
    chase(0, true);
    ui_set_hidden(s_basket, false);
    ui_set_hidden(s_cat, false);
    ui_set_hidden(s_yarn, false);
}

static void confetti_step(void *obj, int32_t v) /* v: 0..1000 along the eased fall */
{
    lv_obj_set_style_translate_y(obj, YARN_CONFETTI_FALL * v / 1000, 0);
#if YARN_CONFETTI_SPIN
    lv_obj_set_style_transform_rotation(obj, YARN_CONFETTI_TURN * v / 1000, 0);
#endif
}

static void confetti_start(void)
{
    for (uint32_t i = 0; i < YARN_CONFETTI; i++) {
        lv_obj_t *k = ui_plain(s_win);
        lv_obj_set_size(k, YARN_CONFETTI_W, YARN_CONFETTI_H);
        lv_obj_set_pos(k, YARN_CONFETTI_X0 + (int32_t)(i * YARN_CONFETTI_X_STEP % YARN_CONFETTI_X_SPAN),
                       YARN_CONFETTI_TOP);
        lv_obj_set_style_radius(k, YARN_CONFETTI_R, 0);
        lv_obj_set_style_bg_color(k, lv_color_hex(CONFETTI_COLORS[i % CONFETTI_COLOR_COUNT]), 0);
        lv_obj_set_style_bg_opa(k, LV_OPA_COVER, 0);
        lv_obj_set_style_transform_pivot_x(k, YARN_CONFETTI_W / 2, 0);
        lv_obj_set_style_transform_pivot_y(k, YARN_CONFETTI_H / 2, 0);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, k);
        lv_anim_set_exec_cb(&a, confetti_step);
        lv_anim_set_values(&a, 0, 1000);
        lv_anim_set_duration(&a, YARN_CONFETTI_MS);
        lv_anim_set_delay(&a, i * YARN_CONFETTI_DELAY % YARN_CONFETTI_DELAY_MAX);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
        s_confetti[i] = k;
    }
}

static void confetti_stop(void)
{
    for (size_t i = 0; i < YARN_CONFETTI; i++) {
        if (s_confetti[i]) {
            lv_anim_delete(s_confetti[i], confetti_step);
            s_confetti[i] = NULL;
        }
    }
}

static void next_round(lv_timer_t *t)
{
    (void)t;
    s_round_timer = NULL;
    if (!s_yarn) {
        return;
    }
    confetti_stop();
    lv_obj_delete(s_win); /* the confetti, the proud cat (and its timer) and the full basket */
    s_win = NULL;
    s_round++;
    start_round();
}

static void win_show(lv_timer_t *t)
{
    (void)t;
    s_win_timer = NULL;
    if (!s_yarn) {
        return;
    }
    s_phase = Y_WIN;
    hint_off();
    ui_set_hidden(s_yarn, true);
    ui_set_hidden(s_basket, true);
    ui_set_hidden(s_cat, true);
    s_win = ui_plain(s_screen);
    lv_obj_set_size(s_win, SCREEN_SIZE, SCREEN_SIZE);
    confetti_start(); /* first, so it falls behind the cat and the basket (design order) */
    lv_obj_t *cat = cat_create(s_win, CAT_SCALE_M);
    cat_set_origin(cat, YARN_WIN_CAT_X, YARN_WIN_CAT_Y);
    cat_set_state(cat, CAT_PROUD);
    ui_image(s_win, "ui/yarn_basket_full", YARN_FULL_BASKET_X, YARN_FULL_BASKET_Y);
    audio_play(SND_YARN_WIN);
    s_round_timer = one_shot(next_round, YARN_NEXT_ROUND_MS);
}

static void hit(void)
{
    s_phase = Y_HIT; /* the yarn stays put until the win scene (spec §3.11) */
    rumble(false);
    s_win_timer = one_shot(win_show, YARN_WIN_DELAY_MS);
}

/* ---- sensors: app_manager calls these on the LVGL task while the screen is awake ---- */

static void yarn_tilt(float x_deg, float y_deg)
{
    if (!s_yarn) {
        return;
    }
    const uint32_t dt = lv_tick_elaps(s_last_tilt_ms);
    s_last_tilt_ms = lv_tick_get();
    if (s_phase != Y_PLAY) {
        return;
    }
    const float ox = s_x, oy = s_y;
    const float moved = roll_step(&s_x, &s_y, x_deg, y_deg, dt, SCREEN_CENTER, SCREEN_CENTER, YARN_RANGE);
    if (moved > 0.0f) {
        place_yarn();
    }
    if (moved >= YARN_ROLL_MIN_PX) {
        lv_display_trigger_activity(NULL); /* rolling is play; resting against the rim is not */
        s_last_roll_ms = s_last_tilt_ms;
        roll_frames(moved, (s_x - ox) + (s_y - oy) >= 0.0f);
        rumble(true);
    } else if (lv_tick_elaps(s_last_roll_ms) >= YARN_RUMBLE_STOP_MS) {
        rumble(false);
    }
    chase(dt, false);
    if (yarn_in_basket(s_x, s_y, s_bx, s_by)) {
        hit();
    }
}

static void yarn_shake(void)
{
    if (!s_yarn || s_phase != Y_PLAY) {
        return; /* not during the win (spec §3.11) */
    }
    s_x = s_y = SCREEN_CENTER;
    place_yarn(); /* the cat eases after it on the next tilt sample */
}

/* ---- screen ---- */

static void build(lv_obj_t *scr)
{
    /* design order: basket, cat, yarn, hint */
    s_basket = ui_image(scr, "ui/yarn_basket", 0, 0);
    s_cat = cat_create(scr, CAT_SCALE_S);
    cat_set_state(s_cat, CAT_SURPRISED);
    s_yarn = ui_image(scr, FRAME[0], 0, 0);
    s_hint = ui_image(scr, "ui/tilt_hint", YARN_HINT_X, YARN_HINT_Y);
    lv_image_set_pivot(s_hint, YARN_HINT_PIVOT_X, YARN_HINT_PIVOT_Y);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_hint);
    lv_anim_set_exec_cb(&a, wobble_step);
    lv_anim_set_values(&a, -YARN_HINT_WOBBLE, YARN_HINT_WOBBLE);
    lv_anim_set_duration(&a, YARN_HINT_WOBBLE_MS / 2);
    lv_anim_set_reverse_duration(&a, YARN_HINT_WOBBLE_MS / 2);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void yarn_create(lv_obj_t *screen)
{
    s_screen = screen;
    s_round = 0;
    s_frame = 0;
    s_frame_px = 0.0f;
    s_rumbling = false;
    s_win = NULL;
    s_last_tilt_ms = s_last_roll_ms = lv_tick_get();
    build(screen);
    start_round();
    s_hint_timer = one_shot(hint_done, YARN_HINT_MS);
}

static void yarn_destroy(void)
{
    cancel_timers();
    lv_anim_delete(s_hint, wobble_step);
    confetti_stop();
    s_rumbling = false; /* the rumble loop: app_manager calls audio_stop_all() after this */
    s_yarn = NULL;
    s_screen = s_basket = s_cat = s_hint = s_win = NULL;
}

const app_t app_yarn = {
    .id = "yarn",
    .icon = "apps/yarn",
    .icon_small = "apps/yarn_96",
    .color = COLOR_YARN,
    .name_sound = SND_NAME_YARN,
    .swipe_sound = SND_SWIPE_YARN,
    .react = CAT_LISTENING,
    .create = yarn_create,
    .destroy = yarn_destroy,
    .on_shake = yarn_shake,
    .on_tilt = yarn_tilt,
};
```

- [ ] **Step 3: Remove the placeholder line**

In `components/ui/apps/placeholder.c`, delete:

```c
PLACEHOLDER(app_yarn, "yarn", COLOR_YARN, SND_NAME_YARN, SND_SWIPE_YARN, CAT_LISTENING)
```

and make the comment read `(voices: Task 17, pet: M4, draw and yarn: M5)`.

- [ ] **Step 4: Build**

Run:
```bash
idf.py reconfigure build
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" reconfigure build
```
Expected: both `Project build complete`, no warnings in `components/ui/apps/yarn.c`.

- [ ] **Step 5: Commit**

```bash
git add components/ui/apps/yarn.c components/ui/include/tokens.h components/ui/apps/placeholder.c
git commit -m "feat: Yarn roll: tilt the yarn into the basket, the cat chases it, confetti win"
```

---

### Task 7: Meow piano app

**Files:**
- Create: `components/ui/apps/piano.c`
- Modify: `components/ui/include/tokens.h` (append)
- Modify: `components/ui/apps/placeholder.c` (remove the `app_piano` line)

**Interfaces:**
- Consumes:
  - Task 3's `cat_create_colored()` with the `piano_*` palettes at `CAT_SCALE_S`;
  - Task 4's `ui/piano_note` and `SND_PIANO_C5` … `SND_PIANO_A5`.
- Produces: `const app_t app_piano` (no sensors).

- [ ] **Step 1: Tokens**

Append to `components/ui/include/tokens.h`:

```c
/* Meow piano (Screen.dc.html piano view; spec §3.11) */
#define PIANO_KEYS         5
#define PIANO_RING_R       140
#define PIANO_PAD_SIZE     145 /* r 70 with its 5 px stroke centered on it */
#define PIANO_PAD_BORDER   5
#define PIANO_PAD_LINE_OPA 179 /* 70 % */
#define PIANO_PAD_FILL_OPA 64  /* 25 % while it sings */
#define PIANO_TAP_SIZE     150
#define PIANO_CAT_DX       65  /* scale S origin from the key's point on the ring */
#define PIANO_CAT_DY       70
#define PIANO_NOTE_X       183
#define PIANO_NOTE_Y       183
#define PIANO_SING_MS      700
```

- [ ] **Step 2: The app**

Create `components/ui/apps/piano.c`:

```c
#include <math.h>
#include <stdint.h>
#include "app.h"
#include "audio.h"
#include "cat.h"
#include "tokens.h"
#include "ui_util.h"

/* Meow piano (design: Screen.dc.html piano view, Main.dc.html act('piano'); spec §3.11). Five cats on a
 * ring, one per note of the C major pentatonic scale, so any order sounds nice. */

#define RAD_PER_DEG 0.017453293f

static const struct {
    uint32_t color;
    sound_id_t sound;
    const char *palette; /* exported at scale S (spec §3.11) */
} KEYS[PIANO_KEYS] = {
    {0xFF5A5A, SND_PIANO_C5, "piano_c"}, {0xFF9F3A, SND_PIANO_D5, "piano_d"}, {0xFFD84A, SND_PIANO_E5, "piano_e"},
    {0x5CD67A, SND_PIANO_G5, "piano_g"}, {0x5AA8FF, SND_PIANO_A5, "piano_a"},
};

/* The design's @keyframes bounce (400 ms): time in ms and lift in px, each leg easing out. */
static const int16_t BOUNCE_MS[] = {0, 140, 280, 340, 400};
static const int16_t BOUNCE_Y[] = {0, -22, 0, -6, 0};
#define BOUNCE_KEYS (sizeof(BOUNCE_MS) / sizeof(BOUNCE_MS[0]))

static lv_obj_t *s_note; /* NULL once destroy() has run: every callback checks it first (app.h) */
static lv_obj_t *s_pad[PIANO_KEYS], *s_cat[PIANO_KEYS];
static lv_timer_t *s_sing_timer;
static int s_singing = -1; /* the key singing now, or -1 */

static void bounce_step(void *obj, int32_t t_ms)
{
    int32_t y = 0;
    for (size_t k = 1; k < BOUNCE_KEYS; k++) {
        if (t_ms <= BOUNCE_MS[k]) {
            const int32_t p = (t_ms - BOUNCE_MS[k - 1]) * 1024 / (BOUNCE_MS[k] - BOUNCE_MS[k - 1]);
            const int32_t eased = 1024 - (1024 - p) * (1024 - p) / 1024;
            y = BOUNCE_Y[k - 1] + (BOUNCE_Y[k] - BOUNCE_Y[k - 1]) * eased / 1024;
            break;
        }
    }
    lv_obj_set_style_translate_y(obj, y, 0);
}

/* The singing cat (if any) goes back to happy, its pad empties and the note turns white again. */
static void quiet(void)
{
    if (s_sing_timer) {
        lv_timer_delete(s_sing_timer);
        s_sing_timer = NULL;
    }
    if (s_singing < 0) {
        return;
    }
    const int i = s_singing;
    s_singing = -1;
    lv_anim_delete(s_cat[i], bounce_step);
    lv_obj_set_style_translate_y(s_cat[i], 0, 0);
    cat_set_state(s_cat[i], CAT_HAPPY);
    lv_obj_set_style_bg_opa(s_pad[i], LV_OPA_TRANSP, 0);
    lv_obj_set_style_image_recolor_opa(s_note, LV_OPA_TRANSP, 0);
}

static void sing_done(lv_timer_t *t)
{
    (void)t;
    s_sing_timer = NULL; /* one-shot timers delete themselves */
    if (!s_note) {
        return; /* destroy() already ran; the screen can outlive it briefly, see app.h */
    }
    quiet();
}

static void key_tapped(lv_event_t *e)
{
    if (!s_note) {
        return;
    }
    const int i = (int)(intptr_t)lv_event_get_user_data(e);
    quiet(); /* a new tap replaces the last song (spec §3.11) */
    s_singing = i;
    cat_set_state(s_cat[i], CAT_SINGING);
    lv_obj_set_style_bg_opa(s_pad[i], PIANO_PAD_FILL_OPA, 0);
    lv_obj_set_style_image_recolor(s_note, lv_color_hex(KEYS[i].color), 0);
    lv_obj_set_style_image_recolor_opa(s_note, LV_OPA_COVER, 0);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, s_cat[i]);
    lv_anim_set_exec_cb(&a, bounce_step);
    lv_anim_set_values(&a, 0, BOUNCE_MS[BOUNCE_KEYS - 1]);
    lv_anim_set_duration(&a, (uint32_t)BOUNCE_MS[BOUNCE_KEYS - 1]);
    lv_anim_start(&a);
    audio_play(KEYS[i].sound); /* one voice: this cuts off a meow still playing */
    s_sing_timer = lv_timer_create(sing_done, PIANO_SING_MS, NULL);
    lv_timer_set_repeat_count(s_sing_timer, 1);
}

static void piano_create(lv_obj_t *screen)
{
    s_singing = -1;
    s_note = ui_image(screen, "ui/piano_note", PIANO_NOTE_X, PIANO_NOTE_Y);
    for (int i = 0; i < PIANO_KEYS; i++) {
        const float a = (-90.0f + 72.0f * (float)i) * RAD_PER_DEG; /* C at the top, then clockwise */
        const int32_t cx = SCREEN_CENTER + (int32_t)lroundf(PIANO_RING_R * cosf(a));
        const int32_t cy = SCREEN_CENTER + (int32_t)lroundf(PIANO_RING_R * sinf(a));
        lv_obj_t *pad = ui_circle(screen, cx, cy, PIANO_PAD_SIZE, KEYS[i].color);
        lv_obj_set_style_bg_opa(pad, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(pad, PIANO_PAD_BORDER, 0);
        lv_obj_set_style_border_color(pad, lv_color_hex(KEYS[i].color), 0);
        lv_obj_set_style_border_opa(pad, PIANO_PAD_LINE_OPA, 0);
        lv_obj_add_flag(pad, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_ext_click_area(pad, (PIANO_TAP_SIZE - PIANO_PAD_SIZE + 1) / 2); /* the 150 px target */
        lv_obj_add_event_cb(pad, key_tapped, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        s_pad[i] = pad;
        s_cat[i] = cat_create_colored(screen, CAT_SCALE_S, KEYS[i].palette); /* not clickable: taps reach the pad */
        cat_set_origin(s_cat[i], cx - PIANO_CAT_DX, cy - PIANO_CAT_DY);
    }
}

static void piano_destroy(void)
{
    if (s_sing_timer) {
        lv_timer_delete(s_sing_timer);
        s_sing_timer = NULL;
    }
    for (int i = 0; i < PIANO_KEYS; i++) {
        lv_anim_delete(s_cat[i], bounce_step);
        s_cat[i] = NULL;
        s_pad[i] = NULL;
    }
    s_singing = -1;
    s_note = NULL;
    /* the meow: app_manager calls audio_stop_all() after this */
}

const app_t app_piano = {
    .id = "piano",
    .icon = "apps/piano",
    .icon_small = "apps/piano_96",
    .color = COLOR_PIANO,
    .name_sound = SND_NAME_PIANO,
    .swipe_sound = SND_SWIPE_PIANO,
    .react = CAT_LAUGHING,
    .create = piano_create,
    .destroy = piano_destroy,
};
```

- [ ] **Step 3: Remove the placeholder line**

In `components/ui/apps/placeholder.c`, delete:

```c
PLACEHOLDER(app_piano, "piano", COLOR_PIANO, SND_NAME_PIANO, SND_SWIPE_PIANO, CAT_LAUGHING)
```

and make the comment read `(voices: Task 17, pet: M4, draw, yarn and piano: M5)`. Only the `app_clock` line remains.

- [ ] **Step 4: Build every variant**

Run:
```bash
sh test/run.sh
idf.py reconfigure build
idf.py -B build-diag -D SDKCONFIG=build-diag/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.diag" reconfigure build
idf.py -B build-gallery -D SDKCONFIG=build-gallery/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.gallery;sdkconfig.perf" reconfigure build
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" reconfigure build
idf.py -B build-perf -D SDKCONFIG=build-perf/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.perf" reconfigure build
```
Expected: the host tests pass and all five builds print `Project build complete`, with no warnings in `components/ui/apps/`.

- [ ] **Step 5: Commit**

```bash
git add components/ui/apps/piano.c components/ui/include/tokens.h components/ui/apps/placeholder.c
git commit -m "feat: Meow piano: five note-colored cats that sing C D E G A"
```

---

### Task 8: Device session

Every step here needs the board. Run each flash in the background with `run_in_background`, wait for the log to show the app is up, then ask. Keep the logs under `build-logs/` (git-ignored by `build-*/`). Before every flash, stop the previous logger (TaskStop on its task id): only one listener can hold the port.

**Files:**
- Modify: `docs/device-checks.md` (append)
- Possibly modify: files named in Step 7 (fallbacks), each with its own `fix:` commit

- [ ] **Step 1: Flash the normal build and log 20 minutes**

Run in the background:
```bash
tools/build_assets.sh && idf.py build && tools/flash_and_log.sh 1200 > build-logs/m5.log 2>&1
```
Wait until the log shows the app manager or menu lines. Because of the 3 s wait, early boot lines may be missing, which is fine.

- [ ] **Step 2: Human step, Magic paws**

Ask (AskUserQuestion, header "Magic paws"). The question text:

> "M5 check 1 of 4. The new build is on the board. On the menu, swipe to the purple 'Magic paws' icon (first look at the small cat at the top: its surprised face with big round eyes and black pupils should look just like before) and tap it. Please check:
> (a) Black screen, a small row of twinkling rainbow dots near the top, and a round black button with a thin white ring and a rainbow at the bottom.
> (b) Draw with one finger. A rainbow trail of dots with a soft glow follows your finger and changes color every few dots, with a sparkle tick now and then. Scribble very fast: the trail stays unbroken. The twinkling dots disappear with your first dot.
> (c) Tap the round button: a blip, and the button now shows a pink paw. Draw: colored paw prints, left and right in turn, toes pointing the way you move, a soft pat with each.
> (d) Draw right up to the edge of the screen: the outermost ring (about 2 mm wide) stays black. Draw across the button: nothing appears on the button.
> (e) Press BOOT, then open Magic paws again: your drawing is still there, and the button shows the brush you had.
> (f) Shake the toy. The drawing dims, a cat with a broom sweeps side to side with a swish and grey dust, it says 'Čisto!', and then the screen is clean with the twinkling dots back. Drawing during the sweep does nothing.
> (g) Draw something, shake, and press BOOT while the cat is still sweeping. Open Magic paws again: the screen is clean.
> Which of these passed?"

multiSelect, options: "(a)-(b) layout and rainbow", "(c)-(d) paws, edge and button", "(e)-(g) kept drawing and sweep", "Something looked or sounded wrong (I'll describe)".

- [ ] **Step 3: Human step, Yarn roll and Meow piano**

Ask (AskUserQuestion, header "Yarn, piano"):

> "M5 check 2 of 4. Press BOOT, swipe to the green 'Yarn roll' icon and tap it, holding the toy the way she would (that grip counts as level). Please check:
> (a) A basket at the bottom right, a pink yarn ball in the middle with a small surprised cat just above it looking at it, and a little wobbling 'tilt' hint at the bottom that disappears after about 4 s.
> (b) Tilt the toy. The yarn rolls downhill with a soft rumble, its stripes turning as it rolls, and it never leaves the screen. The cat follows smoothly behind it, always on the middle side, and its eyes look at the yarn. When the yarn stops, the rumble stops too.
> (c) Roll the yarn into the basket. A moment later: the basket full, a proud cat, spinning confetti, a fanfare and 'Bravo!'. About 3.5 s later a new round starts with the yarn in the middle and the basket in the bottom-left corner. Win two more rounds: the basket moves to the top right, then the top left. No round ends by itself without the yarn reaching the basket.
> (d) Shake the toy while playing: the yarn jumps back to the middle.
> (e) Lean the toy so the yarn rests against the edge and leave it untouched: the rumble stops, and after about 60 s it goes back to the menu.
> Then press BOOT, swipe to the orange 'Meow piano' icon and tap it:
> (f) Five cats in a ring, each a different color (red at the top, then orange, yellow, green and blue clockwise), each inside a circle outline in its color, and a white note in the middle.
> (g) Tap each cat in turn. Each meows a higher note, sings with its mouth moving, bounces up and down once, its circle fills lightly, and the middle note takes its color. After about 0.7 s it is happy again and the note is white.
> (h) Tap quickly across several cats. Each tap cuts off the previous meow, only one cat sings at a time, and none stays stuck singing or colored.
> Which of these passed?"

multiSelect, options: "(a)-(b) yarn layout, rolling, chasing cat", "(c)-(e) win, rounds, shake, resting yarn", "(f)-(h) piano", "Something was wrong (I'll describe)".

- [ ] **Step 4: Human step, Home, timeouts and the dark screen**

Ask (AskUserQuestion, header "M5 system"):

> "M5 check 3 of 4.
> (a) Press BOOT in each of these moments. Each time the menu must be back at once and every sound must stop immediately: while the Magic paws cat sweeps; while the yarn rolls (rumble); right after the yarn drops into the basket (before the confetti); during the confetti; while a piano cat sings.
> (b) In Yarn roll, play ONLY by tilting (no touching) for 90 s: the app stays open the whole time.
> (c) In Magic paws and in Meow piano, leave the toy untouched: after about 60 s each goes back to the menu by itself.
> (d) In Yarn roll, press PWR (screen goes dark), then tilt and shake it: it stays dark and silent. Wake it by tapping the screen.
> Which of these passed?"

multiSelect, options: "(a) Home from every state", "(b)-(c) timeouts", "(d) dark screen ignores motion", "Something was wrong (I'll describe)".

Then check the log:
```bash
grep -c -E "missing image|missing sound|Guru Meditation|abort\(\)|assert" build-logs/m5.log
grep "rst:" build-logs/m5.log
```
Expected: the count is 0, and `rst:` lines appear only for the planned flashes.

- [ ] **Step 5: FPS on the perf build**

Stop Step 1's logger first. Run in the background:
```bash
idf.py -B build-perf -D SDKCONFIG=build-perf/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.perf" build && tools/flash_and_log.sh 300 -B build-perf -D SDKCONFIG=build-perf/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.perf" > build-logs/m5-perf.log 2>&1
```

Ask (AskUserQuestion, header "M5 FPS"):

> "M5 check 4 of 4. The FPS build is on the board (it logs frame rates). For about 10 s each, with a short pause between them:
> (1) Magic paws: scribble fast with the rainbow;
> (2) switch to paws and scribble;
> (3) shake (the sweep);
> (4) Yarn roll: roll the yarn around;
> (5) roll it into the basket and watch the confetti;
> (6) Meow piano: tap the cats quickly.
> Say Done when finished, and whether anything looked choppy."

Options: "Done, all smooth" / "Done, something looked choppy (I'll say which)".

Then read the sysmon lines (`grep -i fps build-logs/m5-perf.log`). Each activity's samples must be ≥ 24 FPS; the app-open zoom is the accepted §3.4 exception.

- [ ] **Step 6: Soak**

Stop the perf logger. Run in the background (about 6 minutes):
```bash
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" build && tools/flash_and_log.sh 420 -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" > build-logs/m5-soak.log 2>&1
```
The soak's warm-up round opens Magic paws once, so the kept 434 KB picture is allocated before the baseline. Each step shakes the open app right after opening it: Magic paws starts a sweep, and Yarn roll recenters. Then it tilts (Yarn roll rolls, rumble on) and presses Home.

Expected:
- `grep SOAK build-logs/m5-soak.log` ends with `SOAK: PASS: internal delta=… psram delta=… over 50 rounds`;
- a single boot, with no `Guru Meditation`, `abort()` or `assert`.

- [ ] **Step 7: Fallbacks, only if a check failed**

- **Confetti below 24 FPS:** set `YARN_CONFETTI_SPIN` to `0` in `components/ui/include/tokens.h` (spec §3.11 fallback). Re-measure Step 5 (5), then commit as `fix: confetti falls without spinning (spec §3.11 fallback)`.
- **Drawing below 24 FPS:** use superpowers:systematic-debugging. First measure how long `paint_stamps` takes per touch sample (`esp_timer_get_time()` around it, logged once a second). Only then change anything.
- **Piano below 24 FPS with five cats:** the spec has no fallback. Record the numbers and ask the user how to proceed.
- **The menu's surprised face or the yarn cat's eyes look wrong:** check the manifest rows for `eyes_big`, `pupil_big` and `highlight_big` against Task 3 Step 3, fix, re-flash, and repeat Step 2 (menu) or Step 3 (b).
- **Any other failure:** use superpowers:systematic-debugging, fix it, re-flash, and repeat the failed step only.

- [ ] **Step 8: Re-flash the normal build**

Stop the soak logger. Run: `tools/flash_and_log.sh 10`. That leaves the board on the normal firmware.

- [ ] **Step 9: Human step, kid test (spec §7: after each playable milestone)**

Ask (AskUserQuestion, header "Kid test"):

> "M5 is playable. Spec §7 asks for a kid test after each playable milestone: 15 minutes of play with no adult help, noting every time she asks for help or gets stuck (the M3 and M4 kid tests are still open too). Do you want to run it now (I'll wait and then record your notes in docs/kid-tests.md) or later (the PR goes up now and the kid test stays open)?"

Options: "Later, open the PR now" / "Now, I'll report back". If now, record the notes in `docs/kid-tests.md` in the same form as the M3 session, and turn each stuck moment into a finding for M8.

- [ ] **Step 10: Record and commit**

Append to `docs/device-checks.md`, filled in from Steps 2–7:

```markdown
## Magic paws, Yarn roll and Meow piano (M5) — 2026-09-24

| Check | Result |
| --- | --- |
| Menu: the surprised face (Magic paws icon) looks as before | … |
| Magic paws: layout, twinkling hint, rainbow trail with glow and sparkles, fast scribbles unbroken | … |
| Magic paws: paw brush (alternating, turned, pat), edge ring and button stay clean | … |
| Magic paws: drawing kept after Home; shake sweeps it (dim, broom, swish, "Čisto!", clean); Home mid-sweep leaves it clean | … |
| Yarn roll: layout and 4 s hint; rolls downhill with rumble and turning frames; the cat follows and looks at the yarn | … |
| Yarn roll: win scene (full basket, proud cat, confetti, fanfare + "Bravo!"), three rounds through the basket spots | … |
| Yarn roll: shake recenters; resting against the rim stops the rumble and times out after ~60 s | … |
| Meow piano: five colored cats and pads; each tap sings, bounces, fills its pad and colors the note; fast taps never stick | … |
| BOOT mid-sweep, mid-roll, after the hit, during the confetti and mid-song: menu at once, silence | … |
| Tilt-only play in Yarn roll for 90 s stays open; Magic paws and Meow piano time out after ~60 s | … |
| PWR off in Yarn roll: tilt and shake stay dark and silent | … |
| FPS (perf build): rainbow / paws / sweep / rolling / confetti / piano | … |
| Soak | … |
| Log | … |
```

```bash
git add docs/device-checks.md
git commit -m "docs: Magic paws, Yarn roll and Meow piano device results"
```

---

### Task 9: Whole-branch review and pull request

This branch was cut from `davor/m4-pet-the-cat` (PR #5) before that PR merged, so it is stacked on M4.

- [ ] **Step 1: Full verification**

Run:
```bash
sh test/run.sh
tools/build_assets.sh
git status --short components/assets/
idf.py build
idf.py -B build-diag -D SDKCONFIG=build-diag/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.diag" build
idf.py -B build-gallery -D SDKCONFIG=build-gallery/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.gallery;sdkconfig.perf" build
idf.py -B build-soak -D SDKCONFIG=build-soak/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.soak" build
idf.py -B build-perf -D SDKCONFIG=build-perf/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.perf" build
```
Expected:
- all host tests pass;
- `git status` shows no change to the committed manifest after the rebuild;
- five `Project build complete`.

- [ ] **Step 2: Whole-branch review**

Use superpowers:requesting-code-review on the M5 commits (`davor/m4-pet-the-cat..HEAD`, or `main..HEAD` if PR #5 has merged), against this plan and spec §3.11. Fix Critical and Important findings with superpowers:receiving-code-review. Re-run Step 1 after any code change, and re-flash any variant whose device result a fix could change.

- [ ] **Step 3: Bring in main if it moved**

Run `gh pr view 5 --json state -q .state` and `git fetch origin && git log --oneline HEAD..origin/main`.
- If PR #5 has merged, or `main` gained other commits (for example PR #3, which also appends to `docs/device-checks.md`), run `git merge origin/main`.
- Resolve an end-of-file conflict in `docs/device-checks.md` by keeping both sections in date order.
- Then re-run `sh test/run.sh && idf.py build`.

- [ ] **Step 4: Push and open the pull request**

Use superpowers:finishing-a-development-branch. The user merges PRs; open one against `main`:

```bash
git push -u origin davor/m5-paws-yarn-piano
gh pr create --base main --title "M5: Magic paws, Yarn roll and Meow piano" --body-file <file with the summary>
```

The PR body covers:
- what she can do in each app;
- the spec §3.11 decisions (the kept drawing, painted strokes, the rolling and win rules, the recolored piano cats);
- the device results table;
- anything left open;
- if PR #5 is still open: "Stacked on #5 (M4). Until it merges, this PR also shows M4's commits; the M5 commits start after `6eb5b22`."

No attribution lines.
