#include "app.h"

/* Adding an app: create components/ui/apps/<name>.c defining `const app_t app_<name>`, then add one
 * X(app_<name>) line below. The menu shows apps in this order. */
#define APP_LIST(X) \
    X(app_voices)   \
    X(app_pet)      \
    X(app_draw)     \
    X(app_yarn)     \
    X(app_clock)    \
    X(app_piano)

#define DECLARE(sym)   extern const app_t sym;
#define REFERENCE(sym) &sym,
APP_LIST(DECLARE)
const app_t *const APPS[] = {APP_LIST(REFERENCE)};
const size_t APP_COUNT = sizeof(APPS) / sizeof(APPS[0]);
