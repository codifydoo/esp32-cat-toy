#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "sounds.h"

#define AUDIO_RATE_HZ 24000

/* All callbacks below run on the audio task (core 0), never on the caller's task. A callback
 * that touches LVGL or other app state must take display_lock() and, under that lock, re-check
 * its own copy of the session it was given against whatever the app currently considers "the
 * active session" — see audio_session_end() for why the session number alone is not enough. */
typedef struct {
    /* Recording finished (auto-stop, max length, or audio_record_stop). The callee owns pcm and must
     * heap_caps_free() it whenever pcm is non-NULL -- n can be 0 with a non-NULL pcm (a stop or a
     * mic read failure within the first 100 ms, before any sample was kept) as well as with n > 0.
     * pcm is NULL (n is then 0 too) only if memory ran out. */
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
/* Starts a new session and ends whichever session was previously active (as audio_session_end()
 * would), so the caller only needs to end its own session when it is done, not when it starts. */
uint32_t audio_session_begin(const audio_listener_t *listener);
/* After this call returns, no NEW callback will start for this session. It does NOT wait for a
 * callback already in flight (e.g. an on_recorded already running a voice effect for hundreds of
 * ms): that call keeps running and may overlap or outlive audio_session_end(), including
 * touching app state after this function has returned. This is why a callback that touches LVGL
 * or app state must take display_lock() and re-check its own copy of the session under that
 * lock, rather than trusting that "session_end already happened" means "no callback is running".
 * audio_session_end() itself never blocks on the audio task, so it is safe to call from the LVGL
 * task while holding display_lock(). */
void audio_session_end(uint32_t session);
/* Discards the first 100 ms of captured audio (RX DMA backlog plus the mic's own pickup of a
 * preceding blip's release tail) before it counts toward max_samples or speech/silence detection. */
void audio_record_start(uint32_t session, size_t max_samples);
void audio_record_stop(uint32_t session);
/* Takes ownership of pcm unconditionally: it is freed on every path, whether the clip actually
 * plays, the session is already stale, a recording is in progress, or the command queue is
 * full/uninitialized and the request is dropped. The caller must not touch pcm again after this
 * call. */
void audio_play_pcm(uint32_t session, int16_t *pcm, size_t n);
