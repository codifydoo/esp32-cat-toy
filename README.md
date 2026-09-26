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
| Deep sleep on the USB cable (USB counts as battery; sleep 20 s after the panel goes off) | same with `power` |
| Battery screens (as `power`, plus a fake battery: 12 % at boot, one point less every 10 s, 1 % after a battery sleep; a reset starts over) | same with `drain` |
| Wake cycles without hands (as `power`, plus: the panel goes off 15 s after start, and each deep sleep ends by itself after 10 s as a touch would, with the boot animation and the boot NTP sync); log them with `tools/serial_log.py <port> 600 -t` | `tools/flash_and_log.sh 30 -B build-autowake -D SDKCONFIG=build-autowake/sdkconfig -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.secrets;sdkconfig.power;sdkconfig.autowake"` (without `sdkconfig.secrets`: no Wi-Fi) |

## Wi-Fi for the clock (optional)

The toy sets its clock over NTP at boot and 10 minutes before the wake-up minute when it knows a
Wi-Fi network, and every 5 minutes while it has no valid time (spec §3.2). Without one, set the time
in the parent area (Sleep → Now). Put the
network in `sdkconfig.secrets` next to `sdkconfig.defaults`. The file is git-ignored; never commit it:

    CONFIG_CAT_WIFI_SSID="network name"
    CONFIG_CAT_WIFI_PASSWORD="password"

A build directory reads it only when it creates its `sdkconfig`, so delete `sdkconfig` after creating or
changing the file, then build. Developer builds that pass `SDKCONFIG_DEFAULTS` read it only if you add
it to their list. Never run `idf.py save-defconfig` on a build with Wi-Fi configured: it writes
`CONFIG_CAT_WIFI_PASSWORD` into the committed `sdkconfig.defaults`.

## First run, battery and deep sleep

- **First run.** The first power-on plays the boot animation, then shows the cat picker. A tap chooses the cat
  and saves it; the parent area's Cat tile changes it later. Erasing the settings (the `nvs` partition in
  `partitions.csv`) brings the picker back, and also resets the volume, the sleep times and the hidden apps.
- **Battery screens.** The hungry cat shows once at 10 % and once at 5 %; plugging in USB shows the eating cat.
  Neither shows over the boot animation, the cat picker, the parent area, the lock or the sunrise.
- **Deep sleep.** On battery the toy deep-sleeps 10 minutes after the panel goes off (never on USB power), with
  a timer 10 minutes before wake time when the clock is valid. A touch or BOOT wakes it (PWR cannot). Every wake
  is a restart: by day the boot animation plays, at night the lock shows at once, and Magic paws' drawing is
  gone. At 2 % it sleeps until it is charged. The serial port disappears while it sleeps, so log it with
  `python tools/serial_log.py <port> <seconds> -t`, which waits for the port to come back.

## Host tests

`test/run.sh` (add `-R name` to run one test)

## Adding an app

1. Create `components/ui/apps/<name>.c` that defines `const app_t app_<name>` (see `components/ui/include/app.h`
   and `apps/voices.c` for a full example).
2. Add `X(app_<name>)` to `APP_LIST` in `components/ui/apps/registry.c`.

## Replacing placeholder sounds

Record the phrases in `docs/recording-list.md` into `assets-src/sounds/phrases/`, then run
`tools/build_assets.sh && idf.py build && tools/flash_and_log.sh 5`.
