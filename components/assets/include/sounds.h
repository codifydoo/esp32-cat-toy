#pragma once
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SND_BOOT,
    SND_OPEN,
    SND_HOME,
    SND_MEOW,
    SND_BLIP_UP,
    SND_BLIP_DOWN,
    SND_NAME_VOICES,
    SND_NAME_PET,
    SND_NAME_DRAW,
    SND_NAME_YARN,
    SND_NAME_CLOCK,
    SND_NAME_PIANO,
    SND_SWIPE_VOICES,
    SND_SWIPE_PET,
    SND_SWIPE_DRAW,
    SND_SWIPE_YARN,
    SND_SWIPE_CLOCK,
    SND_SWIPE_PIANO,
    SND_COSTUME_MOUSE,
    SND_COSTUME_LION,
    SND_COSTUME_ROBOT,
    SND_COSTUME_CAVE,
    SND_PURR, /* a loop: audio_loop_start() */
    SND_BELL_TICK,
    SND_PET_RUB,
    SND_PET_DIZZY,
    SND_PET_EAT,
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
    SND_WHISPER, /* "Pssst… spavaj": play it with audio_play_quiet() (spec §3.12) */
    SND_WAKE,    /* the sunrise: the rising chime, a happy meow and "Dobro jutro!" */
    SND_HUNGRY,   /* "Gladna sam…": the low-battery screen (spec §3.13) */
    SND_CHARGING, /* munching and "Njam njam!": the charging screen */
    SND_COUNT
} sound_id_t;

/* Writes "/assets/snd/<file>.wav" for the sound into out. False for an unknown id. */
bool sounds_path(sound_id_t id, char *out, size_t n);
