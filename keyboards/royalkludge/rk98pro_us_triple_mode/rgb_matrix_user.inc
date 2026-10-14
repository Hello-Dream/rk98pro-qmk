// Copyright 2024 yangzheng20003 (@yangzheng20003)
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef ENABLE_RGB_MATRIX_RGBR_PLAY
RGB_MATRIX_EFFECT(RGBR_PLAY)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

bool RGBR_PLAY(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    extern void rgbrec_play(uint8_t led_min, uint8_t led_max);
    rgbrec_play(led_min, led_max);

    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif

/* ===================== 新增自定义灯效（HS_ 前缀） ===================== */

#ifdef ENABLE_RGB_MATRIX_HS_PLASMA
RGB_MATRIX_EFFECT(HS_PLASMA)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 流动等离子：三层正弦叠成 2D 场，同时调制色相与亮度
static bool HS_PLASMA(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t time = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 2, 1));
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t x = g_led_config.point[i].x >> 2;
        uint8_t y = g_led_config.point[i].y >> 2;
        uint8_t v = sin8(x * 8 + time);
        v         = qadd8(v, sin8(y * 8 + (time >> 1)));
        v         = qadd8(v, sin8(((x + y) * 4) + (time / 3)));
        hsv_t hsv = rgb_matrix_config.hsv;
        hsv.h += v;
        hsv.v = scale8(v, hsv.v);
        rgb_t rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif     // ENABLE_RGB_MATRIX_HS_PLASMA

#ifdef ENABLE_RGB_MATRIX_HS_SPARKLE
RGB_MATRIX_EFFECT(HS_SPARKLE)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 彩色星光：每颗灯按自身相位闪，色相沿灯序铺开
static bool HS_SPARKLE(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t time = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 1, 1));
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t phase = (uint8_t)(i * 97 + (uint16_t)i * i * 13);
        uint8_t v     = sin8(time + phase);
        v             = scale8(v, v); // 收窄成尖峰才像星星
        hsv_t hsv     = rgb_matrix_config.hsv;
        hsv.h += (uint8_t)(i * 37);
        hsv.v = scale8(v, hsv.v);
        rgb_t rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif     // ENABLE_RGB_MATRIX_HS_SPARKLE

#ifdef RGB_MATRIX_KEYREACTIVE_ENABLED
#    ifdef ENABLE_RGB_MATRIX_HS_RIPPLE
RGB_MATRIX_EFFECT(HS_RIPPLE)
#        ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 按键涟漪：每次按键从一个点扩散出一圈光环，600ms 后消散
static bool HS_RIPPLE(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint16_t acc = 0;
        for (uint8_t j = 0; j < g_last_hit_tracker.count; j++) {
            uint16_t age = g_last_hit_tracker.tick[j];
            if (age > 600) continue;
            int16_t  dx   = (int16_t)g_led_config.point[i].x - (int16_t)g_last_hit_tracker.x[j];
            int16_t  dy   = (int16_t)g_led_config.point[i].y - (int16_t)g_last_hit_tracker.y[j];
            uint16_t dist = (uint16_t)((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
            uint16_t radius = scale16by8(age, qadd8(rgb_matrix_config.speed >> 1, 8));
            uint16_t ring   = (dist > radius) ? (dist - radius) : (radius - dist);
            if (ring < 24) acc += (24 - ring) * 8;
        }
        hsv_t hsv = rgb_matrix_config.hsv;
        hsv.v     = scale8((uint8_t)(acc > 255 ? 255 : acc), hsv.v);
        rgb_t rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#        endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#    endif     // ENABLE_RGB_MATRIX_HS_RIPPLE
#endif         // RGB_MATRIX_KEYREACTIVE_ENABLED

#ifdef ENABLE_RGB_MATRIX_HS_BREATH_GRADIENT
RGB_MATRIX_EFFECT(HS_BREATH_GRADIENT)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 彩虹呼吸：左右铺满彩虹，整体一起明暗呼吸
static bool HS_BREATH_GRADIENT(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t time    = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 2, 1));
    uint8_t breathe = sin8(time);
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        hsv_t hsv = rgb_matrix_config.hsv;
        hsv.h += g_led_config.point[i].x;
        hsv.v = scale8(breathe, hsv.v);
        rgb_t rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif     // ENABLE_RGB_MATRIX_HS_BREATH_GRADIENT

#ifdef ENABLE_RGB_MATRIX_HS_FIRE
RGB_MATRIX_EFFECT(HS_FIRE)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 火焰：底部最热，向上变暗并不断跳动
static bool HS_FIRE(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t time = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 2, 1));
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t x     = g_led_config.point[i].x;
        uint8_t y     = g_led_config.point[i].y; // 0=顶部 .. 64=底部
        uint8_t base  = qadd8(63, y * 3);        // 越靠下越热
        uint8_t flick = sin8(x * 5 + time);
        flick         = qadd8(flick, sin8(y * 7 + (time >> 1)));
        flick         = scale8(flick, flick);
        uint8_t heat  = qsub8(base, flick >> 1);
        hsv_t hsv     = rgb_matrix_config.hsv;
        hsv.h = scale8(heat, 40); // 红 -> 橙黄
        hsv.s = 255;
        hsv.v = scale8(heat, hsv.v);
        rgb_t rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif     // ENABLE_RGB_MATRIX_HS_FIRE

#ifdef ENABLE_RGB_MATRIX_HS_CYBER_SCAN
RGB_MATRIX_EFFECT(HS_CYBER_SCAN)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 赛博扫描：一条亮线左右来回扫，带拖尾
static bool HS_CYBER_SCAN(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t time = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 2, 1));
    uint8_t tri  = (time < 128) ? (uint8_t)(time << 1) : (uint8_t)((255 - time) << 1);
    uint8_t head = scale8(tri, 230);
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t x = g_led_config.point[i].x;
        uint8_t d = (x > head) ? (x - head) : (head - x);
        uint8_t v = 0;
        if (d < 6) {
            v = 255; // 扫描线本体
        } else if (d < 46) {
            v = (uint8_t)(255 - (uint16_t)(d - 6) * 255 / 40); // 拖尾渐隐
        }
        hsv_t hsv = rgb_matrix_config.hsv;
        hsv.h += 128; // 青蓝系
        hsv.s = 255;
        hsv.v = scale8(v, hsv.v);
        rgb_t rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif     // ENABLE_RGB_MATRIX_HS_CYBER_SCAN

#ifdef ENABLE_RGB_MATRIX_HS_RAINDROPS
RGB_MATRIX_EFFECT(HS_RAINDROPS)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 雨滴：各列错开的下落光点（带一点拖尾）
static bool HS_RAINDROPS(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t time = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 2, 1));
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t x     = g_led_config.point[i].x;
        uint8_t y     = g_led_config.point[i].y;
        uint8_t phase = (uint8_t)(x * 53 + (x >> 3) * 29); // 每列不同相位
        uint8_t fall  = (uint8_t)((time + phase) << 1);    // 下落进度 0..255
        uint8_t dy    = fall >> 2;                          // 0..63
        uint8_t v     = 0;
        if (fall < 244) {
            uint8_t d = (y > dy) ? (y - dy) : (dy - y);
            if (d < 4) {
                v = 255;
            } else if (d < 20) {
                v = (uint8_t)(255 - (uint16_t)(d - 4) * 255 / 16);
            }
        }
        hsv_t hsv = rgb_matrix_config.hsv;
        hsv.v = scale8(v, hsv.v);
        rgb_t rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif     // ENABLE_RGB_MATRIX_HS_RAINDROPS
