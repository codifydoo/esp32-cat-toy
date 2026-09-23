#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "sounds.h"

#define AUDIO_RATE_HZ 24000

typedef struct {
    /* Recording finished (auto-stop, max length, or audio_record_stop). The callee owns pcm and must
     * heap_caps_free() it. pcm is NULL and n is 0 if memory ran out. */
    void (*on_recorded)(uint32_t session, int16_t *pcm, size_t n, bool heard);
    /* Peak of each 20 ms chunk while this session's clip plays. */
    void (*on_level)(uint32_t session, uint16_t peak);
    /* This session's clip finished, or another sound replaced it. */
    void (*on_done)(uint32_t session);
} audio_listener_t;

esp_err_t audio_init(uint8_t volume_pct);
bool audio_available(void);
void audio_set_volume(uint8_t pct);
void audio_play(sound_id_t id);
void audio_loop_start(sound_id_t id);
void audio_loop_stop(void);
void audio_stop_all(void);
uint32_t audio_session_begin(const audio_listener_t *listener);
void audio_session_end(uint32_t session);
void audio_record_start(uint32_t session, size_t max_samples);
void audio_record_stop(uint32_t session);
void audio_play_pcm(uint32_t session, int16_t *pcm, size_t n);
