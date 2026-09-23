#include "audio.h"
#include <stdio.h>
#include <string.h>
#include "bsp/esp-bsp.h"
#include "envelope.h"
#include "esp_check.h"
#include "esp_codec_dev.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "settings.h"
#include "wav.h"

#define CHUNK_SAMPLES      480 /* 20 ms at 24 kHz */
#define MIC_GAIN_DB        30.0f
#define AUDIO_MIC_CHANNELS 1   /* set to 2 if recordings are silent (see Task 13, Step 6) */

static const char *TAG = "audio";

typedef enum {
    CMD_PLAY,
    CMD_PLAY_PCM,
    CMD_LOOP_START,
    CMD_LOOP_STOP,
    CMD_STOP_ALL,
    CMD_REC_START,
    CMD_REC_STOP,
    CMD_SESSION_END,
    CMD_VOLUME,
} cmd_kind_t;

typedef struct {
    cmd_kind_t kind;
    uint32_t session;
    uint32_t arg;
    int16_t *pcm;
    size_t n;
} cmd_t;

typedef struct {
    int16_t *pcm;
    size_t n;
    bool tried;
} clip_t;

static esp_codec_dev_handle_t s_spk, s_mic;
static QueueHandle_t s_queue;
static bool s_ok;
static portMUX_TYPE s_mux = portMUX_INITIALIZER_UNLOCKED;
static uint32_t s_session;
static uint32_t s_next_session = 1;
static audio_listener_t s_listener;
static clip_t s_clips[SND_COUNT];

/* Audio-task-only state. */
static const int16_t *s_fx;
static int16_t *s_fx_owned;
static size_t s_fx_n, s_fx_pos;
static uint32_t s_fx_session;
static const int16_t *s_loop;
static size_t s_loop_n, s_loop_pos;
static int16_t *s_rec;
static size_t s_rec_n, s_rec_max;
static uint32_t s_rec_session;
static rec_stop_t s_rec_stop;

static int16_t clamp16(int32_t v) { return (int16_t)(v > INT16_MAX ? INT16_MAX : v < INT16_MIN ? INT16_MIN : v); }

static uint32_t current_session(void)
{
    portENTER_CRITICAL(&s_mux);
    uint32_t s = s_session;
    portEXIT_CRITICAL(&s_mux);
    return s;
}

static bool listener_for(uint32_t session, audio_listener_t *out)
{
    portENTER_CRITICAL(&s_mux);
    bool ok = session != 0 && session == s_session;
    if (ok) {
        *out = s_listener;
    }
    portEXIT_CRITICAL(&s_mux);
    return ok;
}

static const clip_t *clip_get(uint32_t id)
{
    if (id >= SND_COUNT) {
        return NULL;
    }
    clip_t *c = &s_clips[id];
    if (c->pcm || c->tried) {
        return c->pcm ? c : NULL;
    }
    c->tried = true; /* a missing or bad file is reported once, then stays silent */
    char path[64];
    if (!sounds_path((sound_id_t)id, path, sizeof(path))) {
        return NULL;
    }
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "missing sound %s", path);
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = len > 44 ? heap_caps_malloc((size_t)len, MALLOC_CAP_SPIRAM) : NULL;
    bool ok = buf && fread(buf, 1, (size_t)len, f) == (size_t)len;
    fclose(f);
    wav_info_t w;
    if (!ok || !wav_parse(buf, (size_t)len, &w) || w.channels != 1 || w.bits != 16 || w.rate != AUDIO_RATE_HZ) {
        ESP_LOGE(TAG, "bad sound %s (need 24 kHz 16-bit mono PCM)", path);
        heap_caps_free(buf);
        return NULL;
    }
    memmove(buf, buf + w.data_offset, w.data_bytes);
    c->pcm = (int16_t *)buf;
    c->n = w.data_bytes / 2;
    return c;
}

static void fx_finish(bool notify)
{
    uint32_t session = s_fx_session;
    heap_caps_free(s_fx_owned);
    s_fx = NULL;
    s_fx_owned = NULL;
    s_fx_n = s_fx_pos = 0;
    s_fx_session = 0;
    audio_listener_t l;
    if (notify && session && listener_for(session, &l) && l.on_done) {
        l.on_done(session);
    }
}

static void rec_finish(void)
{
    int16_t *buf = s_rec;
    size_t n = s_rec_n;
    uint32_t session = s_rec_session;
    bool heard = s_rec_stop.heard;
    s_rec = NULL;
    s_rec_n = 0;
    s_rec_session = 0;
    audio_listener_t l;
    if (listener_for(session, &l) && l.on_recorded) {
        l.on_recorded(session, buf, n, heard);
    } else {
        heap_caps_free(buf);
    }
}

static void handle(const cmd_t *c)
{
    switch (c->kind) {
    case CMD_PLAY: {
        const clip_t *clip = clip_get(c->arg);
        if (clip && !s_rec) {
            fx_finish(true);
            s_fx = clip->pcm;
            s_fx_n = clip->n;
        }
        break;
    }
    case CMD_PLAY_PCM:
        if (c->session != current_session() || s_rec) {
            heap_caps_free(c->pcm);
            break;
        }
        fx_finish(true);
        s_fx = s_fx_owned = c->pcm;
        s_fx_n = c->n;
        s_fx_session = c->session;
        break;
    case CMD_LOOP_START: {
        const clip_t *clip = clip_get(c->arg);
        if (clip) {
            s_loop = clip->pcm;
            s_loop_n = clip->n;
            s_loop_pos = 0;
        }
        break;
    }
    case CMD_LOOP_STOP:
        s_loop = NULL;
        break;
    case CMD_STOP_ALL:
        fx_finish(true);
        s_loop = NULL;
        break;
    case CMD_REC_START:
        if (c->session != current_session() || s_rec) {
            break;
        }
        fx_finish(true);
        s_loop = NULL;
        s_rec = heap_caps_malloc(c->n * sizeof(int16_t), MALLOC_CAP_SPIRAM);
        s_rec_max = c->n;
        s_rec_n = 0;
        s_rec_session = c->session;
        rec_stop_init(&s_rec_stop);
        if (!s_rec) {
            ESP_LOGE(TAG, "no memory for a %u-sample recording", (unsigned)c->n);
            rec_finish(); /* delivers pcm = NULL, n = 0 */
        }
        break;
    case CMD_REC_STOP:
        if (s_rec && c->session == s_rec_session) {
            rec_finish();
        }
        break;
    case CMD_SESSION_END:
        if (s_rec && s_rec_session == c->session) {
            heap_caps_free(s_rec);
            s_rec = NULL;
            s_rec_n = 0;
            s_rec_session = 0;
        }
        if (s_fx_session == c->session) {
            fx_finish(false);
        }
        break;
    case CMD_VOLUME:
        esp_codec_dev_set_out_vol(s_spk, (int)c->arg);
        break;
    }
}

static void record_chunk(void)
{
    size_t want = s_rec_max - s_rec_n;
    if (want > CHUNK_SAMPLES) {
        want = CHUNK_SAMPLES;
    }
    int16_t *dst = s_rec + s_rec_n;
#if AUDIO_MIC_CHANNELS == 2
    static int16_t stereo[CHUNK_SAMPLES * 2];
    if (esp_codec_dev_read(s_mic, stereo, (int)(want * 4)) != ESP_CODEC_DEV_OK) {
        rec_finish();
        return;
    }
    for (size_t i = 0; i < want; i++) {
        dst[i] = stereo[i * 2];
    }
#else
    if (esp_codec_dev_read(s_mic, dst, (int)(want * 2)) != ESP_CODEC_DEV_OK) {
        rec_finish();
        return;
    }
#endif
    uint16_t peak = envelope_peak(dst, want);
    s_rec_n += want;
    if (rec_stop_update(&s_rec_stop, peak, (uint32_t)want, AUDIO_RATE_HZ) || s_rec_n >= s_rec_max) {
        rec_finish();
    }
}

static void play_chunk(void)
{
    static int16_t mix[CHUNK_SAMPLES];
    for (size_t i = 0; i < CHUNK_SAMPLES; i++) {
        int32_t v = 0;
        if (s_fx && s_fx_pos < s_fx_n) {
            v += s_fx[s_fx_pos++];
        }
        if (s_loop) {
            v += s_loop[s_loop_pos++];
            if (s_loop_pos >= s_loop_n) {
                s_loop_pos = 0;
            }
        }
        mix[i] = clamp16(v);
    }
    esp_codec_dev_write(s_spk, mix, sizeof(mix));
    if (s_fx) {
        audio_listener_t l;
        if (s_fx_session && listener_for(s_fx_session, &l) && l.on_level) {
            l.on_level(s_fx_session, envelope_peak(mix, CHUNK_SAMPLES));
        }
        if (s_fx_pos >= s_fx_n) {
            fx_finish(true);
        }
    }
}

static void audio_task(void *arg)
{
    (void)arg;
    cmd_t c;
    for (;;) {
        bool busy = s_fx || s_loop || s_rec;
        if (xQueueReceive(s_queue, &c, busy ? 0 : portMAX_DELAY) == pdTRUE) {
            handle(&c);
            continue; /* drain every queued command before the next chunk */
        }
        if (s_rec) {
            record_chunk();
        } else if (s_fx || s_loop) {
            play_chunk();
        }
    }
}

static void send(cmd_t c)
{
    if (!s_ok || xQueueSend(s_queue, &c, 0) != pdTRUE) {
        if (s_ok) {
            ESP_LOGW(TAG, "command queue full; dropped command %d", c.kind);
        }
        if (c.kind == CMD_PLAY_PCM) {
            heap_caps_free(c.pcm);
        }
    }
}

static uint8_t clamp_volume(uint8_t pct) { return pct > SETTINGS_VOLUME_MAX ? SETTINGS_VOLUME_MAX : pct; }

esp_err_t audio_init(uint8_t volume_pct)
{
    s_spk = bsp_audio_codec_speaker_init();
    s_mic = bsp_audio_codec_microphone_init();
    ESP_RETURN_ON_FALSE(s_spk && s_mic, ESP_FAIL, TAG, "codec init failed");
    esp_codec_dev_sample_info_t fs = {
        .bits_per_sample = 16,
        .channel = 1,
        .sample_rate = AUDIO_RATE_HZ,
    };
    ESP_RETURN_ON_FALSE(esp_codec_dev_open(s_spk, &fs) == ESP_CODEC_DEV_OK, ESP_FAIL, TAG, "speaker open failed");
    fs.channel = AUDIO_MIC_CHANNELS;
    ESP_RETURN_ON_FALSE(esp_codec_dev_open(s_mic, &fs) == ESP_CODEC_DEV_OK, ESP_FAIL, TAG, "mic open failed");
    esp_codec_dev_set_in_gain(s_mic, MIC_GAIN_DB);
    esp_codec_dev_set_out_vol(s_spk, clamp_volume(volume_pct));
    s_queue = xQueueCreate(16, sizeof(cmd_t));
    ESP_RETURN_ON_FALSE(s_queue, ESP_ERR_NO_MEM, TAG, "queue");
    ESP_RETURN_ON_FALSE(xTaskCreatePinnedToCore(audio_task, "audio", 6144, NULL, 5, NULL, 0) == pdPASS, ESP_ERR_NO_MEM,
                        TAG, "task");
    s_ok = true;
    ESP_LOGI(TAG, "ready: %d Hz mono, volume %u%%", AUDIO_RATE_HZ, clamp_volume(volume_pct));
    return ESP_OK;
}

bool audio_available(void) { return s_ok; }
void audio_set_volume(uint8_t pct) { send((cmd_t){.kind = CMD_VOLUME, .arg = clamp_volume(pct)}); }
void audio_play(sound_id_t id) { send((cmd_t){.kind = CMD_PLAY, .arg = (uint32_t)id}); }
void audio_loop_start(sound_id_t id) { send((cmd_t){.kind = CMD_LOOP_START, .arg = (uint32_t)id}); }
void audio_loop_stop(void) { send((cmd_t){.kind = CMD_LOOP_STOP}); }
void audio_stop_all(void) { send((cmd_t){.kind = CMD_STOP_ALL}); }

uint32_t audio_session_begin(const audio_listener_t *listener)
{
    portENTER_CRITICAL(&s_mux);
    uint32_t s = s_next_session++;
    if (s_next_session == 0) {
        s_next_session = 1;
    }
    s_session = s;
    s_listener = *listener;
    portEXIT_CRITICAL(&s_mux);
    return s;
}

void audio_session_end(uint32_t session)
{
    if (session == 0) {
        return;
    }
    portENTER_CRITICAL(&s_mux);
    if (s_session == session) {
        s_session = 0;
    }
    portEXIT_CRITICAL(&s_mux);
    send((cmd_t){.kind = CMD_SESSION_END, .session = session});
}

void audio_record_start(uint32_t session, size_t max_samples)
{
    send((cmd_t){.kind = CMD_REC_START, .session = session, .n = max_samples});
}

void audio_record_stop(uint32_t session) { send((cmd_t){.kind = CMD_REC_STOP, .session = session}); }

void audio_play_pcm(uint32_t session, int16_t *pcm, size_t n)
{
    send((cmd_t){.kind = CMD_PLAY_PCM, .session = session, .pcm = pcm, .n = n});
}
