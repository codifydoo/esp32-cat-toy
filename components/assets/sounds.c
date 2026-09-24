#include "sounds.h"
#include <stdio.h>
#include "assets.h"

static const char *const FILES[SND_COUNT] = {
    [SND_BOOT] = "boot",
    [SND_OPEN] = "open",
    [SND_HOME] = "mew_down",
    [SND_MEOW] = "meow_1",
    [SND_BLIP_UP] = "blip_up",
    [SND_BLIP_DOWN] = "blip_down",
    [SND_NAME_VOICES] = "name_voices",
    [SND_NAME_PET] = "name_pet",
    [SND_NAME_DRAW] = "name_draw",
    [SND_NAME_YARN] = "name_yarn",
    [SND_NAME_CLOCK] = "name_clock",
    [SND_NAME_PIANO] = "name_piano",
    [SND_SWIPE_VOICES] = "swipe_voices",
    [SND_SWIPE_PET] = "swipe_pet",
    [SND_SWIPE_DRAW] = "swipe_draw",
    [SND_SWIPE_YARN] = "swipe_yarn",
    [SND_SWIPE_CLOCK] = "swipe_clock",
    [SND_SWIPE_PIANO] = "swipe_piano",
    [SND_COSTUME_MOUSE] = "costume_mouse",
    [SND_COSTUME_LION] = "costume_lion",
    [SND_COSTUME_ROBOT] = "costume_robot",
    [SND_COSTUME_CAVE] = "costume_cave",
    [SND_PURR] = "purr",
    [SND_BELL_TICK] = "bell_tick",
    [SND_PET_RUB] = "pet_rub",
    [SND_PET_DIZZY] = "pet_dizzy",
    [SND_PET_EAT] = "pet_eat",
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
};

bool sounds_path(sound_id_t id, char *out, size_t n)
{
    if ((unsigned)id >= SND_COUNT || FILES[id] == NULL) {
        return false;
    }
    snprintf(out, n, ASSETS_BASE_PATH "/snd/%s.wav", FILES[id]);
    return true;
}
