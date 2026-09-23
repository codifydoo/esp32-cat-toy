/* Host tool: apply a voice effect to raw s16le mono audio. Used by tools/build_sounds. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "voice_fx.h"

int main(int argc, char **argv)
{
    static const char *const names[VOICE_COUNT] = {"mouse", "lion", "robot", "cave"};
    if (argc != 5) {
        fprintf(stderr, "usage: fxcli <mouse|lion|robot|cave> <rate> <in.raw> <out.raw>\n");
        return 2;
    }
    int v = -1;
    for (int i = 0; i < VOICE_COUNT; i++) {
        if (strcmp(argv[1], names[i]) == 0) {
            v = i;
        }
    }
    if (v < 0) {
        fprintf(stderr, "unknown voice %s\n", argv[1]);
        return 2;
    }
    uint32_t rate = (uint32_t)strtoul(argv[2], NULL, 10);
    FILE *f = fopen(argv[3], "rb");
    if (!f) {
        perror(argv[3]);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long bytes = ftell(f);
    fseek(f, 0, SEEK_SET);
    size_t n = (size_t)bytes / 2;
    int16_t *in = malloc(n * sizeof(int16_t) + 2);
    if (fread(in, sizeof(int16_t), n, f) != n) {
        fprintf(stderr, "short read\n");
        return 1;
    }
    fclose(f);
    size_t out_n = voice_fx_output_len((voice_t)v, n, rate);
    int16_t *out = malloc(out_n * sizeof(int16_t) + 2);
    size_t w = voice_fx_apply((voice_t)v, in, n, out, out_n, rate);
    FILE *o = fopen(argv[4], "wb");
    if (!o) {
        perror(argv[4]);
        return 1;
    }
    fwrite(out, sizeof(int16_t), w, o);
    fclose(o);
    free(in);
    free(out);
    return 0;
}
