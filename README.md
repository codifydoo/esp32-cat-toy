# Cat toy

Firmware for a Waveshare ESP32-S3-Touch-AMOLED-1.75 cat toy. Spec: `docs/superpowers/specs/2026-09-22-cat-toy-design.md`.

## Build

```bash
. ~/esp/esp-idf/export.sh
tools/build_assets.sh          # images + sounds -> assets/
idf.py build
tools/flash_and_log.sh 15      # flash and show 15 s of logs
```

## Developer builds

| Build | Command |
| --- | --- |
| Hardware diagnostics | `tools/flash_and_log.sh 30 -B build-diag -D SDKCONFIG=build-diag/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.diag"` |
| Cat gallery | same with `gallery` |
| FPS overlay | same with `perf` |
| Soak test | same with `soak` |

## Host tests

`test/run.sh` (add `-R name` to run one test)

## Adding an app

1. Create `components/ui/apps/<name>.c` that defines `const app_t app_<name>` (see `components/ui/include/app.h`
   and `apps/voices.c` for a full example).
2. Add `X(app_<name>)` to `APP_LIST` in `components/ui/apps/registry.c`.
3. If it replaces a placeholder, delete that line from `apps/placeholder.c`.

## Replacing placeholder sounds

Record the phrases in `docs/recording-list.md` into `assets-src/sounds/phrases/`, then run
`tools/build_assets.sh && idf.py build && tools/flash_and_log.sh 5`.
