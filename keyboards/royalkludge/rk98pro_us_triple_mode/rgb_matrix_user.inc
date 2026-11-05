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

/* ===================== 自定义灯效（HS_ 前缀）v2 =====================
 * 目标：更亮、更平滑、颜色更干净；亮度尽量对齐全局 val（不过度衰减）
 */

#ifdef ENABLE_RGB_MATRIX_HS_PLASMA
RGB_MATRIX_EFFECT(HS_PLASMA)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 等离子：两组正弦叠加成平缓的色相流动，亮度保持高位
static bool HS_PLASMA(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t t = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 2, 1));
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t x = g_led_config.point[i].x;
        uint8_t y = g_led_config.point[i].y;
        uint8_t h = qadd8(sin8((x >> 1) + t), sin8((y << 1) - (t >> 1)));
        uint8_t v = qadd8(200, (uint8_t)(sin8((x + y) + (t << 1)) >> 2)); // 200~255
        HSV     hsv = rgb_matrix_config.hsv;
        hsv.h += h;
        hsv.v = scale8(v, hsv.v);
        RGB rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif     // ENABLE_RGB_MATRIX_HS_PLASMA

#ifdef ENABLE_RGB_MATRIX_HS_SPARKLE
RGB_MATRIX_EFFECT(HS_SPARKLE)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 星光：底光常亮 + 每颗灯按自身相位闪烁，色相沿灯序铺开
static bool HS_SPARKLE(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t t = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 1, 1));
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t ph = (uint8_t)(i * 97 + (uint16_t)i * i * 13);
        uint8_t s  = sin8(t + ph);
        uint8_t v  = qadd8(110, scale8(s, 145)); // 110~255
        HSV     hsv = rgb_matrix_config.hsv;
        hsv.h += (uint8_t)(i * 37);
        hsv.v = scale8(v, hsv.v);
        RGB rgb = hsv_to_rgb(hsv);
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

// 涟漪：按键扩散出较粗较亮的圆环，带一点底光
static bool HS_RIPPLE(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint16_t acc = 0;
        for (uint8_t j = 0; j < g_last_hit_tracker.count; j++) {
            uint16_t age = g_last_hit_tracker.tick[j];
            if (age > 700) continue;
            int16_t  dx   = (int16_t)g_led_config.point[i].x - (int16_t)g_last_hit_tracker.x[j];
            int16_t  dy   = (int16_t)g_led_config.point[i].y - (int16_t)g_last_hit_tracker.y[j];
            uint16_t dist = (uint16_t)((dx < 0 ? -dx : dx) + (dy < 0 ? -dy : dy));
            uint16_t radius = scale16by8(age, qadd8(rgb_matrix_config.speed >> 1, 8));
            uint16_t ring   = (dist > radius) ? (dist - radius) : (radius - dist);
            if (ring < 40) acc += (40 - ring) * 6;
        }
        if (acc > 200) acc = 200;
        uint8_t v = qadd8(55, (uint8_t)acc); // 底光 55 + 环
        HSV     hsv = rgb_matrix_config.hsv;
        hsv.v = scale8(v, hsv.v);
        RGB rgb = hsv_to_rgb(hsv);
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

// 彩虹呼吸：左右铺满彩虹，整体亮度在 50%~100% 之间起伏
static bool HS_BREATH_GRADIENT(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t t  = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 2, 1));
    uint8_t br = qadd8(128, (uint8_t)(sin8(t) >> 1)); // 128~255
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        HSV hsv = rgb_matrix_config.hsv;
        hsv.h += g_led_config.point[i].x;
        hsv.v = scale8(br, hsv.v);
        RGB rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif     // ENABLE_RGB_MATRIX_HS_BREATH_GRADIENT

#ifdef ENABLE_RGB_MATRIX_HS_FIRE
RGB_MATRIX_EFFECT(HS_FIRE)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 火焰：底部更热、颜色红->橙黄，轻微跳动
static bool HS_FIRE(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t t = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 2, 1));
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t x     = g_led_config.point[i].x >> 1;
        uint8_t y     = g_led_config.point[i].y; // 0 顶部 .. 64 底部
        uint8_t base  = qadd8(120, qadd8(y, y)); // 底部更热
        uint8_t flick = sin8(x + t);
        flick         = qadd8(flick, sin8((y << 1) + (t >> 1)));
        flick         = scale8(flick, flick);
        uint8_t heat  = qsub8(base, flick >> 2);
        HSV     hsv   = rgb_matrix_config.hsv;
        hsv.h = scale8(heat, 36); // 红 -> 橙黄
        hsv.s = 255;
        hsv.v = scale8(qadd8(heat, 40), hsv.v);
        RGB rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif     // ENABLE_RGB_MATRIX_HS_FIRE

#ifdef ENABLE_RGB_MATRIX_HS_CYBER_SCAN
RGB_MATRIX_EFFECT(HS_CYBER_SCAN)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 赛博扫描：青蓝亮线来回扫，带渐隐拖尾 + 微弱底光
static bool HS_CYBER_SCAN(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t t    = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 2, 1));
    uint8_t tri  = (t < 128) ? (uint8_t)(t << 1) : (uint8_t)((255 - t) << 1);
    uint8_t head = scale8(tri, 235);
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t x = g_led_config.point[i].x;
        uint8_t d = (x > head) ? (x - head) : (head - x);
        uint8_t v = 45; // 底光
        if (d < 10) {
            v = 255;
        } else if (d < 60) {
            v = 255 - (uint8_t)((uint16_t)(d - 10) * 255 / 50);
        }
        HSV hsv = rgb_matrix_config.hsv;
        hsv.h += 128;
        hsv.s = 255;
        hsv.v = scale8(v, hsv.v);
        RGB rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif     // ENABLE_RGB_MATRIX_HS_CYBER_SCAN

#ifdef ENABLE_RGB_MATRIX_HS_RAINDROPS
RGB_MATRIX_EFFECT(HS_RAINDROPS)
#    ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

// 雨滴：每列错开下落的光点 + 微弱底光
static bool HS_RAINDROPS(effect_params_t *params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t t = scale16by8(g_rgb_timer, qadd8(rgb_matrix_config.speed >> 2, 1));
    for (uint8_t i = led_min; i < led_max; i++) {
        RGB_MATRIX_TEST_LED_FLAGS();
        uint8_t x    = g_led_config.point[i].x;
        uint8_t y    = g_led_config.point[i].y;
        uint8_t ph   = (uint8_t)(x * 53 + (x >> 3) * 29);
        uint8_t fall = (uint8_t)((t + ph) << 1);
        uint8_t dy   = fall >> 2;
        uint8_t v    = 40; // 底光
        if (fall < 250) {
            uint8_t d = (y > dy) ? (y - dy) : (dy - y);
            if (d < 4) {
                v = 255;
            } else if (d < 22) {
                v = 255 - (uint8_t)((uint16_t)(d - 4) * 255 / 18);
            }
        }
        HSV hsv = rgb_matrix_config.hsv;
        hsv.v = scale8(v, hsv.v);
        RGB rgb = hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#    endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
#endif     // ENABLE_RGB_MATRIX_HS_RAINDROPS
