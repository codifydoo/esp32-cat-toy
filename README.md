# Cat toy

Firmware for a Waveshare ESP32-S3-Touch-AMOLED-1.75C cat toy (the 1.75C, not the 1.75: different audio clock pin, no I/O expander). Spec: `docs/superpowers/specs/2026-09-22-cat-toy-design.md`.

## Build

```bash
. ~/esp/esp-idf/export.sh
tools/build_assets.sh          # images + sounds -> assets/
idf.py build
tools/flash_and_log.sh 15      # flash and show 15 s of logs
```

`tools/flash_and_log.sh` waits 3 s after flashing and then listens without resetting the board.
Opening the port the usual way resets it, and a second reset within about a second of the flasher's
can hang early boot (`docs/device-checks.md`). So the first ~3 s of the boot log are not shown. Only
one listener can hold the port: let a running one finish (or stop it) before flashing again.

## Developer builds

| Build | Command |
| --- | --- |
| Hardware diagnostics | `tools/flash_and_log.sh 30 -B build-diag -D SDKCONFIG=build-diag/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.diag"` |
| Cat gallery | same with `gallery` |
| FPS overlay | same with `perf` |
| Soak test | same with `soak` |

## Wi-Fi for the clock (optional)

The toy sets its clock over NTP at boot and 10 minutes before the wake-up minute when it knows a
Wi-Fi network (spec §3.2). Without one, set the time in the parent area (Sleep → Now). Put the
network in `sdkconfig.secrets` next to `sdkconfig.defaults`. The file is git-ignored; never commit it:

    CONFIG_CAT_WIFI_SSID="network name"
    CONFIG_CAT_WIFI_PASSWORD="password"

A build directory reads it only when it creates its `sdkconfig`, so delete `sdkconfig` after creating or
changing the file, then build. Developer builds that pass `SDKCONFIG_DEFAULTS` read it only if you add
it to their list. Never run `idf.py save-defconfig` on a build with Wi-Fi configured: it writes
`CONFIG_CAT_WIFI_PASSWORD` into the committed `sdkconfig.defaults`.

## Host tests

`test/run.sh` (add `-R name` to run one test)

## Adding an app

1. Create `components/ui/apps/<name>.c` that defines `const app_t app_<name>` (see `components/ui/include/app.h`
   and `apps/voices.c` for a full example).
2. Add `X(app_<name>)` to `APP_LIST` in `components/ui/apps/registry.c`.

## Replacing placeholder sounds

Record the phrases in `docs/recording-list.md` into `assets-src/sounds/phrases/`, then run
`tools/build_assets.sh && idf.py build && tools/flash_and_log.sh 5`.
