#include "hold_ring.h"
#include "app_manager.h"
#include "gestures.h"
#include "overlays.h"
#include "tokens.h"
#include "touch_raw.h"
#include "ui_util.h"

typedef struct {
    lv_obj_t *screen, *ring;
    lv_timer_t *timer;
    hold_track_t track;
    hold_ring_cb_t cb;
} hold_ring_t;

static void poll(lv_timer_t *t)
{
    hold_ring_t *h = lv_timer_get_user_data(t);
    if (app_manager_current() || lv_screen_active() != h->screen || overlay_showing()) {
        hold_reset(&h->track); /* only while this screen shows, with no battery screen over it */
        ui_set_hidden(h->ring, true);
        return;
    }
    const touch_raw_t raw = touch_raw_get();
    uint16_t progress = 0;
    const hold_state_t st = hold_update(&h->track, raw.count, raw.x, raw.y, lv_tick_get(), &progress);
    if (hold_owns_touch(st)) {
        /* The hold owns this touch (the ring is showing): LVGL ignores the rest of it, so letting go is not also a
         * tap on whatever is under the fingers. */
        ui_wait_release();
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
