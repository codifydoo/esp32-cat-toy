#pragma once
/* Design constants from design/claude-design (Spec.dc.html, Screen.dc.html). One place to tune. */

#define SCREEN_SIZE   466
#define SCREEN_CENTER 233

/* App colors (Spec: App colors) */
#define COLOR_VOICES 0xFF5FA2
#define COLOR_PET    0xFFB547
#define COLOR_DRAW   0x8A7BFF
#define COLOR_YARN   0x2FC4A0
#define COLOR_CLOCK  0x3D7BFF
#define COLOR_PIANO  0xFF7A45

/* Menu (Screen.dc.html, menu view). The cat is the M scale (200 px); its design origin sits here. */
#define MENU_CAT_ORIGIN_X   133
#define MENU_CAT_ORIGIN_Y   9
#define MENU_CAT_CX         233
#define MENU_CAT_CY         97
#define MENU_CAT_R          75
#define MENU_ICON_X         113
#define MENU_ICON_Y         142
#define MENU_ICON_SIZE      240
#define MENU_ICON_CX        233
#define MENU_ICON_CY        262
#define MENU_ICON_R         120
#define MENU_PAW_A_CX       205
#define MENU_PAW_B_CX       261
#define MENU_PAW_CY         144
#define MENU_ZONE_LEFT_X    14
#define MENU_ZONE_RIGHT_X   362
#define MENU_ZONE_Y         172
#define MENU_ZONE_W         90
#define MENU_ZONE_H         180
#define MENU_DOT_RADIUS     204
#define MENU_DOT_STEP_DEG   7
#define MENU_DOT_ACTIVE     18
#define MENU_DOT_IDLE       12
#define MENU_DOT_EDGE       8
#define MENU_DOT_IDLE_OPA   89  /* 35 % */
#define STATIC_OPA          77  /* 30 %: chevrons and the home hint */
#define HOME_HINT_ANGLE_DEG (-30) /* 2 o'clock; set from docs/device-checks.md (BOOT position) */
#define HOME_HINT_RADIUS    206

/* Motion (Spec: Motion) */
#define ANIM_SWIPE_MS          280
#define ANIM_SETTLE_MS         320
#define ANIM_OPEN_ZOOM_MS      220
#define ANIM_OPEN_FADE_MS      180
#define ANIM_HOME_MS           200
#define ANIM_DIM_MS            600
#define ICON_SCALE_OPEN        307 /* spec 3.4 fallback 1.2 x 256: 1.9 x (486) measured 11 FPS on the device */
#define ICON_SCALE_SETTLE_FROM 230 /* 0.9 x 256 */
#define TALK_AFTER_NAME_MS     1200

/* Idle and burn-in (Spec: Burn-in) */
#define IDLE_DIM_MS      20000
#define IDLE_SLEEP_MS    60000
#define IDLE_OFF_MS      120000
#define IDLE_APP_HOME_MS 60000
#define IDLE_DIM_PCT     35
#define BRIGHTNESS_FULL  100
#define BURN_SHIFT_MS    60000
#define BURN_SHIFT_PX    4

/* Boot (Screens: Boot / wake) */
#define BOOT_MS         2600
#define BOOT_RING_COLOR 0xFFB547

/* Placeholder apps: an L-scale cat centered on the screen */
#define PLACEHOLDER_CAT_X 103
#define PLACEHOLDER_CAT_Y 70

/* Silly voices (Screen.dc.html, parrot view) */
#define VOICES_CAT_ORIGIN_X   133
#define VOICES_CAT_ORIGIN_Y   17
#define VOICES_CAT_HIT_X      148
#define VOICES_CAT_HIT_Y      38
#define VOICES_CAT_HIT_SIZE   170
#define VOICES_MIC_X          123
#define VOICES_MIC_Y          232
#define VOICES_MIC_SIZE       220
#define VOICES_RING_CX        233
#define VOICES_RING_CY        124
#define VOICES_RING_R         102
#define VOICES_RING_DOTS      29
#define VOICES_RING_DOT       7
#define VOICES_WAVE_CY        120
#define VOICES_RECORD_MAX_S   5
#define VOICES_BLIP_LEAD_MS   150
#define COLOR_MIC_IDLE        0xFF5FA2
#define COLOR_MIC_LISTEN      0xFF3B5C
#define COLOR_MIC_PLAY        0x5A2340
#define COLOR_MIC_OFF         0x5F5E5A
#define COLOR_MIC_RING        0x9E1F5C
#define COLOR_MIC_RING_LISTEN 0x8A0F26
#define COLOR_COSTUME_MOUSE   0xFF8FC8
#define COLOR_COSTUME_LION    0xFFB020
#define COLOR_COSTUME_ROBOT   0x4FD8FF
#define COLOR_COSTUME_CAVE    0x9C7BFF

/* Parent area (design: "Parent area" colors; "Parent · max volume") */
#define COLOR_P_SURFACE 0x1E1D2A
#define COLOR_P_BORDER  0x3A3850
#define COLOR_P_TEXT    0xF2EFFA
#define COLOR_P_MUTED   0xA9A5BF
#define COLOR_P_TRACK   0x2A2838
#define COLOR_P_ARC     0xFFB547
#define PARENT_ARC_CX   233
#define PARENT_ARC_CY   254
#define PARENT_ARC_R    110
#define PARENT_ARC_W    22
#define PARENT_BTN_SIZE 84
#define PARENT_BTN_Y    332
#define PARENT_MINUS_X  106
#define PARENT_PLUS_X   276

/* Parent entry ring (design: "Menu · parent hold") */
#define HOLD_RING_CX  233
#define HOLD_RING_CY  112
#define HOLD_RING_R   92
#define HOLD_RING_W   6
#define HOLD_RING_OPA 140 /* 55 % */
#define HOLD_POLL_MS  50

/* Pet the cat (Screen.dc.html pet view; spec §3.10). The design's 250 px cat (1.25x) is drawn at
 * scale L (1.3x), centered on the design's head center (233, 159). */
#define PET_CAT_ORIGIN_X  103
#define PET_CAT_ORIGIN_Y  19
#define PET_EYES_X        233 /* between the eyes: design (100, 104) at 1.3x */
#define PET_EYES_Y        154
#define PET_HIT_CX        233 /* the whole head is the touch area */
#define PET_HIT_CY        155
#define PET_HIT_SIZE      230
#define PET_FISH_CX       233
#define PET_FISH_CY       373
#define PET_FISH_SIZE     150 /* touch target */
#define PET_FISH_RING     146 /* the drawn circle: r 68 with its 10 px stroke centered on it */
#define PET_FISH_BORDER   10
#define PET_FISH_EAT_OPA  128 /* 50 % while eating */
#define COLOR_FISH        0x3FB6FF
#define COLOR_FISH_RING   0x0F5A85
#define PET_BALL_SIZE     48
#define PET_BALL_START_X  330
#define PET_BALL_START_Y  110
#define PET_BALL_RANGE    190  /* the ball's center stays this close to the screen center */
#define PET_BALL_HIDE_MS  3000 /* back to the plain face 3 s after the last tilt */
#define PET_ROLL_MIN_PX   0.5f /* less than this per step is not rolling (not activity, no tick) */
#define PET_TICK_PX       60   /* one bell tick per 60 px rolled */
#define PET_HEART_RISE    44
#define PET_HEART_MS      1200
#define PET_PURR_SOUND_MS 1500
#define PET_PURR_MS       1800
#define PET_GIGGLE_MS     1800
#define PET_DIZZY_MS      2000
#define PET_EAT_MS        2400

/* Rainbow brush (Spec.dc.html): Magic paws dots and prints, Yarn roll confetti. An array initializer. */
#define RAINBOW_COLORS {0xFF5A5A, 0xFF9F3A, 0xFFD84A, 0x5CD67A, 0x4FD8FF, 0x5A8CFF, 0xB07CFF}

/* Magic paws (Screen.dc.html draw and erase views; spec §3.11) */
#define DRAW_DOT_R           8.0f  /* 16 px dots */
#define DRAW_HALO_R          15.0f /* 30 px halo */
#define DRAW_HALO_OPA        71    /* 28 % */
#define DRAW_BUTTON_X        158   /* the 150 px brush button */
#define DRAW_BUTTON_Y        300
#define DRAW_HINT_X          153
#define DRAW_HINT_Y          120
#define DRAW_DUST_X          100
#define DRAW_DUST_Y          360
#define DRAW_TWINKLE_MS      3000  /* 30 % -> 100 % -> 30 % */
#define DRAW_TWINKLE_LOW_OPA 77    /* 30 % */
#define DRAW_SPARKLE_EVERY   8     /* dots per sparkle tick */
#define DRAW_SOUND_GAP_MS    125   /* sparkles and pats: at most 8 per second */
#define DRAW_SWEEP_OPA       77    /* the drawing at 30 % while the cat sweeps */
#define DRAW_SWEEP_CAT_X     133   /* scale M origin */
#define DRAW_SWEEP_CAT_Y     196
#define DRAW_SWEEP_PX        60
#define DRAW_SWING_MS        550   /* one swing there and back */
#define DRAW_CISTO_MS        900
#define DRAW_CLEAN_MS        1700
