#include "app_manager.h"
#include <stdbool.h>
#include "audio.h"
#include "launcher.h"
#include "tokens.h"
#include "ui_util.h"

static const app_t *s_current;
static lv_obj_t *s_screen;  /* the current app's screen; NULL when none is open */
static bool s_pending_home; /* Home arrived before the open fade made s_screen the active screen */

void app_manager_init(void) { launcher_create(); }
void app_manager_start(void) { launcher_show(); }

static void close_current_app(void)
{
    const app_t *app = s_current;
    lv_obj_t *screen = s_screen;
    s_current = NULL;
    s_screen = NULL;

    /* Cut pointer input to the app screen before destroy(): a press already in flight on it can
     * otherwise still reach the app's event callbacks (e.g. a tap handler) after destroy() has
     * freed the app's own state. This is the same wait_release + reset pair LVGL itself runs on
     * every pointer indev before deleting an object it is currently pressing -- see
     * obj_indev_reset() in managed_components/lvgl__lvgl/src/core/lv_obj_tree.c. wait_release
     * makes the indev ignore the rest of the current touch; reset(indev, screen) drops this
     * screen as the indev's pressed/last/scrolled object so the next touch starts fresh once
     * the finger lifts (see indev_proc_press()/indev_proc_release() in lv_indev.c). */
    lv_indev_t *indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            if (lv_indev_get_state(indev) != LV_INDEV_STATE_RELEASED) {
                lv_indev_wait_release(indev);
            }
            lv_indev_reset(indev, screen);
        }
        indev = lv_indev_get_next(indev);
    }

    app->destroy();
    audio_stop_all();
    audio_play(SND_HOME);
    launcher_show(); /* fades the menu in and deletes the app screen */
}

static void deferred_home(void *user_data)
{
    (void)user_data;
    close_current_app();
}

/* LV_EVENT_SCREEN_LOADED fires once the app screen actually becomes the active screen: for a
 * screen-load animation that happens from inside scr_anim_completed(), itself called from the
 * anim-refresher timer on the NEXT tick after lv_screen_load_anim() returns (its start_cb,
 * scr_load_anim_start(), is what flips lv_display_t.act_scr, and that too only runs on that next
 * tick) -- see managed_components/lvgl__lvgl/src/display/lv_display.c. The LVGL lock is released
 * between worker-task ticks (see lvgl_worker() in esp_lv_adapter.c), so Home can arrive in that
 * gap while lv_screen_active() still reports the menu screen. If it did, finish the open first
 * and only then close, on a fresh lv_async_call() rather than re-entering screen loading from
 * inside this screen-load event. */
static void on_screen_loaded(lv_event_t *e)
{
    (void)e;
    if (s_pending_home) {
        s_pending_home = false;
        lv_async_call(deferred_home, NULL);
    }
}

void app_manager_open(const app_t *app)
{
    if (s_current || !app) {
        return;
    }
    s_current = app;
    s_screen = ui_screen();
    lv_obj_add_event_cb(s_screen, on_screen_loaded, LV_EVENT_SCREEN_LOADED, NULL);
    app->create(s_screen);
    lv_screen_load_anim(s_screen, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_OPEN_FADE_MS, 0, false);
}

void app_manager_home(void)
{
    if (!s_current) {
        return;
    }
    if (lv_screen_active() != s_screen) {
        /* The open fade hasn't switched the active screen yet; closing now would leave the app
         * screen loaded but orphaned once it does. Finish opening first (see on_screen_loaded()). */
        s_pending_home = true;
        return;
    }
    close_current_app();
}

const app_t *app_manager_current(void) { return s_current; }

void app_manager_on_button(button_id_t id)
{
    if (id != BUTTON_HOME) {
        return; /* Power is wired up in Task 16 */
    }
    if (s_current) {
        app_manager_home();
    } else {
        launcher_home_pressed();
    }
}
