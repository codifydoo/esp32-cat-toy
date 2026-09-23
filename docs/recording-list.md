# Sounds to record

Recordings replace the placeholders. Record in a quiet room, with the phone about 20 cm away, one file per line below. Save them as WAV (any sample rate, mono or stereo) with the exact file name, then run `tools/build_assets.sh`. The pipeline trims silence, normalizes, converts to 24 kHz mono, and pitches phrases up 4 semitones into the cat voice.

## Phrases → `assets-src/sounds/phrases/`

| File | Say | Tone |
| --- | --- | --- |
| bok.wav | Bok! | cheerful hello |
| name_voices.wav | Smiješni glasovi | silly, bouncy |
| name_pet.wav | Maži macu | soft, warm |
| name_draw.wav | Čarobne šapice | magical |
| name_yarn.wav | Klupko | playful |
| name_clock.wav | Pospani sat | sleepy |
| name_piano.wav | Mijau klavir | sing-song |
| idemo.wav | Idemo! | excited |
| hihihi.wav | Hihihi! | giggle |
| njam.wav | Njam! | yummy |
| njam_njam.wav | Njam njam! | munching |
| cisto.wav | Čisto! | proud |
| bravo.wav | Bravo! | big cheer |
| pssst_spavaj.wav | Pssst… spavaj | whisper |
| dobro_jutro.wav | Dobro jutro! | bright morning |
| gladna_sam.wav | Gladna sam… | sleepy, a little sad |

## Animal and effect sounds → `assets-src/sounds/sfx/`

Record these yourself (meowing is encouraged) or use CC0 sounds. For CC0 files, add the source URL and licence to `assets-src/sounds/sfx/LICENSES.md`.

The files are: meow_1, meow_2 (higher), meow_3 (lower), mew_down (short falling mew), mrrrow (stretchy wake-up), purr (1.5 s loop), giggle, wheee, boing, crunch, swish (broom), rumble (1 s loop), whoosh, squeak (mouse), roar (small lion), yawn, munch (loop), snore.
