#include "app_manager.h"
#include "audio.h"
#include "launcher.h"
#include "tokens.h"
#include "ui_util.h"

static const app_t *s_current;

void app_manager_init(void) { launcher_create(); }
void app_manager_start(void) { launcher_show(); }

void app_manager_open(const app_t *app)
{
    if (s_current || !app) {
        return;
    }
    s_current = app;
    lv_obj_t *screen = ui_screen();
    app->create(screen);
    lv_screen_load_anim(screen, LV_SCR_LOAD_ANIM_FADE_IN, ANIM_OPEN_FADE_MS, 0, false);
}

void app_manager_home(void)
{
    if (!s_current) {
        return;
    }
    const app_t *app = s_current;
    s_current = NULL;
    app->destroy();
    audio_stop_all();
    audio_play(SND_HOME);
    launcher_show(); /* fades the menu in and deletes the app screen */
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
