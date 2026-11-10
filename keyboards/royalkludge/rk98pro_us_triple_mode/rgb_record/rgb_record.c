// Copyright 2024 yangzheng20003 (@yangzheng20003)
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rgb_record.h"
#include "rgb_matrix.h"
#include "eeprom.h"
#include "eeconfig.h"
#include "nvm_eeconfig.h"
#include "hs/store.h"
#include "hs/battery.h"
/* 上游把 EECONFIG_USER_DATABLOCK / EECONFIG_USER 移入内部头文件；
 * 直接引用其定义，自动跟随核心区大小，避免硬编码偏移 */
#include "nvm_eeprom_eeconfig_internal.h"

#define RGBREC_STATE_ON  1
#define RGBREC_STATE_OFF 0
#define RGBREC_COLOR_NUM (sizeof(rgbrec_hs_lists) / sizeof(rgbrec_hs_lists[0]))

typedef struct {
    uint8_t state;
    uint8_t channel;
    uint16_t value;
} rgbrec_info_t;

typedef uint16_t (*rgbrec_effects_t)[MATRIX_COLS];

rgbrec_effects_t p_rgbrec_effects = NULL;

static uint16_t rgbrec_hs_lists[] = RGB_RECORD_HS_LISTS;
static uint8_t  rgbrec_buffer[MATRIX_ROWS * MATRIX_COLS * 2];
/* LED 索引 → 打包后的矩阵位置 (row << 8) | col；0xFFFF 表示没有对应位置。 */
static uint16_t rgbrec_led_map[RGB_MATRIX_LED_COUNT];
static bool     rgbrec_led_map_ready = false;

//clang-format off
/* Fn+PrtSc 循环的常用内置灯效。必须使用当前 keyboard.json 已启用的效果宏，
 * 否则 rgb_matrix_mode() 会拒绝无效编号，导致循环卡住或退出录制预览后回不到原灯效。 */
static const uint8_t rgbmatrix_buff[] = {
    RGB_MATRIX_CYCLE_LEFT_RIGHT,
    RGB_MATRIX_RAINBOW_MOVING_CHEVRON,
    RGB_MATRIX_CYCLE_OUT_IN,
    RGB_MATRIX_PIXEL_FLOW,
    RGB_MATRIX_PIXEL_FRACTAL,
    RGB_MATRIX_HUE_WAVE,
    RGB_MATRIX_TYPING_HEATMAP,
    RGB_MATRIX_SOLID_REACTIVE_NEXUS,
    RGB_MATRIX_DIGITAL_RAIN,
    RGB_MATRIX_SPLASH,
    RGB_MATRIX_CYCLE_SPIRAL,
    RGB_MATRIX_RAINBOW_PINWHEELS,
    RGB_MATRIX_ALPHAS_MODS,
    RGB_MATRIX_BREATHING,
    RGB_MATRIX_CYCLE_UP_DOWN,
    RGB_MATRIX_CYCLE_PINWHEEL,
};
/* Fn+Del 切色时按“6 档”处理的效果 */
static const uint8_t sixth_gear_buff[] = {
    RGB_MATRIX_CYCLE_UP_DOWN,
    RGB_MATRIX_CYCLE_LEFT_RIGHT,
    RGB_MATRIX_RAINBOW_MOVING_CHEVRON,
    RGB_MATRIX_CYCLE_OUT_IN,
    RGB_MATRIX_PIXEL_FRACTAL,
    RGB_MATRIX_HUE_WAVE,
    RGB_MATRIX_SPLASH,
};
static uint8_t rgb_hsvs[RGB_HSV_MAX][2] = {
    {0,   255},
    {85,  255},
    {170, 255},
    {43,  255},
    {191, 255},
    {128, 255},
    {0,    0 }
};

_Static_assert(ARRAY_SIZE(rgbmatrix_buff) <= 16, "rgbmatrix_buff exceeds saved HSV slots");

//clang-format on
static rgbrec_info_t rgbrec_info = {
    .state   = RGBREC_STATE_OFF,
    .channel = 0,
    .value   = 0xFF,
};

extern const uint16_t PROGMEM rgbrec_default_effects[RGBREC_CHANNEL_NUM][MATRIX_ROWS][MATRIX_COLS];

/* 从 g_led_config 反向建立 LED 索引到矩阵行列的查找表。 */
static void rgbrec_build_led_map(void) {
    memset(rgbrec_led_map, 0xFF, sizeof(rgbrec_led_map));

    for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
        for (uint8_t col = 0; col < MATRIX_COLS; col++) {
            uint8_t index = g_led_config.matrix_co[row][col];
            if (index != NO_LED && index < RGB_MATRIX_LED_COUNT) {
                rgbrec_led_map[index] = ((uint16_t)row << 8) | col;
            }
        }
    }

    rgbrec_led_map_ready = true;
}

static inline RGB hs_to_rgb(uint8_t h, uint8_t s) {

    if ((h == 0) && (s == 0)) {
        return hsv_to_rgb((HSV){0, 0, 0});
    } else if ((h == 1) && (s == 1)) {
        return hsv_to_rgb((HSV){0, 0, rgbrec_info.value});
    } else {
        return hsv_to_rgb((HSV){h, s, rgbrec_info.value});
    }
}

void rgbrec_init(uint8_t channel) {

    if (channel >= RGBREC_CHANNEL_NUM) {
        channel = 0;
    }

    p_rgbrec_effects    = (rgbrec_effects_t)rgbrec_buffer;
    rgbrec_info.state   = RGBREC_STATE_OFF;
    rgbrec_info.channel = channel;
    rgbrec_info.value   = rgb_matrix_get_val();

    rgbrec_build_led_map();
    rgbrec_read_current_channel(rgbrec_info.channel);
}

bool rgbrec_show(uint8_t channel) {

    if (channel >= RGBREC_CHANNEL_NUM) {
        return false;
    }

    rgbrec_info.channel = channel;
    rgbrec_read_current_channel(rgbrec_info.channel);

    if (rgb_matrix_get_mode() != RGB_MATRIX_CUSTOM_RGBR_PLAY) {
        rgb_matrix_mode(RGB_MATRIX_CUSTOM_RGBR_PLAY);
    }

    return true;
}

bool rgbrec_start(uint8_t channel) {

    if (channel >= RGBREC_CHANNEL_NUM) {

        return false;
    }

    if (rgbrec_info.state == RGBREC_STATE_OFF) {
        rgbrec_info.state   = RGBREC_STATE_ON;
        rgbrec_info.channel = channel;
        rgbrec_read_current_channel(rgbrec_info.channel);
        if (rgb_matrix_get_mode() != RGB_MATRIX_CUSTOM_RGBR_PLAY) {
            rgb_matrix_mode(RGB_MATRIX_CUSTOM_RGBR_PLAY);
        }

        return true;
    }

    return false;
}

#define RGBREC_SAVE_CHUNK 4

void rgbrec_update_current_channel(uint8_t channel) {
    if (channel >= RGBREC_CHANNEL_NUM) {
        return;
    }

    uint16_t offset = (uint16_t)(channel * sizeof(rgbrec_buffer));
    hs_store_rgb_write(offset, sizeof(rgbrec_buffer), rgbrec_buffer);
    hs_store_rgb_commit();
}

// 清空指定通道（写入黑色），并同步当前预览缓存
void rgbrec_clear_channel(uint8_t channel) {
    if (channel >= RGBREC_CHANNEL_NUM) {
        return;
    }

    uint8_t zero[sizeof(rgbrec_buffer)] = {0};
    hs_store_rgb_write((uint16_t)(channel * sizeof(rgbrec_buffer)), sizeof(zero), zero);
    hs_store_rgb_commit();

    if (rgbrec_info.channel == channel) {
        memset(rgbrec_buffer, 0, sizeof(rgbrec_buffer));
    }
}

// 复制通道数据；目标通道若正在预览，则重新载入缓存
void rgbrec_copy_channel(uint8_t from, uint8_t to) {
    if (from >= RGBREC_CHANNEL_NUM || to >= RGBREC_CHANNEL_NUM || from == to) {
        return;
    }

    uint8_t buf[sizeof(rgbrec_buffer)];
    hs_store_rgb_read((uint16_t)(from * sizeof(rgbrec_buffer)), sizeof(buf), buf);
    hs_store_rgb_write((uint16_t)(to * sizeof(rgbrec_buffer)), sizeof(buf), buf);
    hs_store_rgb_commit();

    if (rgbrec_info.channel == to) {
        rgbrec_read_current_channel(to);
    }
}

bool rgbrec_end(uint8_t channel, bool save_choose) {

    if (channel >= RGBREC_CHANNEL_NUM) {

        return false;
    }

    if ((rgbrec_info.state == RGBREC_STATE_ON) && (channel == rgbrec_info.channel)) {
        rgbrec_info.state = RGBREC_STATE_OFF;
        if (save_choose)
            rgbrec_update_current_channel(rgbrec_info.channel);

        return true;
    }

    return false;
}

bool rgbrec_end_current(bool save_choose) {

    if (rgbrec_info.state != RGBREC_STATE_ON) {

        return false;
    }

    return rgbrec_end(rgbrec_info.channel, save_choose);
}

static inline void rgb_matrix_set_hs(int index, uint16_t hs) {

    RGB rgb = hs_to_rgb(HS_GET_H(hs), HS_GET_S(hs));
    rgb_matrix_set_color(index, rgb.r, rgb.g, rgb.b);
}

void rgbrec_play(uint8_t led_min, uint8_t led_max) {
    if (p_rgbrec_effects == NULL) {
        return;
    }
    if (!rgbrec_led_map_ready) {
        rgbrec_build_led_map();
    }

    rgbrec_info.value = rgb_matrix_get_val();

    for (uint8_t i = led_min; i < led_max; i++) {
        uint16_t position = rgbrec_led_map[i];
        if (position == 0xFFFF) {
            continue;
        }

        uint8_t  row      = (uint8_t)(position >> 8);
        uint8_t  col      = (uint8_t)(position & 0xFF);
        uint16_t hs_color = p_rgbrec_effects[row][col];
        rgb_matrix_set_hs(i, hs_color);
    }
}

void rgbrec_set_close_all(uint8_t h, uint8_t s, uint8_t v) {

    if (!h && !s && !v) {
        memset(rgbrec_buffer, 0, sizeof(rgbrec_buffer));
    } else {
        // 缓冲区存的是 HS（高字节 H、低字节 S），V 由当前亮度单独提供
        uint16_t hs = ((uint16_t)h << 8) | s;
        for (uint8_t row = 0; row < MATRIX_ROWS; row++) {
            for (uint8_t col = 0; col < MATRIX_COLS; col++) {
                uint16_t index           = (uint16_t)(row * MATRIX_COLS + col) * 2;
                rgbrec_buffer[index]     = (uint8_t)(hs & 0xFF);
                rgbrec_buffer[index + 1] = (uint8_t)(hs >> 8);
            }
        }
    }
}

void rgbrec_read_current_channel(uint8_t channel) {
    if (channel >= RGBREC_CHANNEL_NUM) {
        return;
    }

    hs_store_rgb_read((uint16_t)(channel * sizeof(rgbrec_buffer)), sizeof(rgbrec_buffer), rgbrec_buffer);
}

bool rgbrec_is_started(void) {

    return (rgbrec_info.state == RGBREC_STATE_ON);
}

static inline void cycle_rgb_next_color(uint8_t row, uint8_t col) {

    if (p_rgbrec_effects == NULL) {

        return;
    }

    for (uint8_t index = 0; index < RGBREC_COLOR_NUM; index++) {
        if (rgbrec_hs_lists[index] == p_rgbrec_effects[row][col]) {
            index                      = ((index + 1) % RGBREC_COLOR_NUM);
            p_rgbrec_effects[row][col] = rgbrec_hs_lists[index];

            return;
        }
    }

    p_rgbrec_effects[row][col] = rgbrec_hs_lists[0];
}

bool rgbrec_register_record(uint16_t keycode, keyrecord_t *record) {
    (void)keycode;

    if (rgbrec_info.state == RGBREC_STATE_ON) {
        cycle_rgb_next_color(record->event.key.row, record->event.key.col);

        return true;
    }

    return false;
}

bool hs_user_datablock_reset = false;

void eeconfig_init_user_datablock(void) {
    // 先写入 EECONFIG_USER 版本标记（并清零数据区）。否则 eeconfig_prepare_datablocks()
    // 会在每次启动时都判定数据区无效，导致设置、键位和录制数据反复复位。
    nvm_eeconfig_init_user_datablock();
    hs_store_reset();
    hs_user_datablock_reset = true;
}

uint8_t find_index(void) {

    for (uint8_t index = 0; index < (sizeof(rgbmatrix_buff) / sizeof(rgbmatrix_buff[0])); index++) {
        if (rgb_matrix_get_mode() == rgbmatrix_buff[index]) {

            return index;
        }
    }

    return 0;
}

void record_rgbmatrix_increase(uint8_t *last_mode) {
    uint8_t index;

    index = find_index();
    if (rgbrec_info.state != RGBREC_STATE_ON) {
        index = (index + 1) % (sizeof(rgbmatrix_buff) / sizeof(rgbmatrix_buff[0]));
    }
    *last_mode = rgbmatrix_buff[index];

    // 灯效与颜色先改 RAM，最后统一保存一次，避免同一份配置写两条日志
    rgb_matrix_mode_noeeprom(rgbmatrix_buff[index]);
    //record_color_hsv(false);

    uint8_t rgb_hsv_index = record_color_read_data();
    rgb_matrix_sethsv_noeeprom(rgb_hsvs[rgb_hsv_index][0], rgb_hsvs[rgb_hsv_index][1], rgb_matrix_get_val());
    eeconfig_update_rgb_matrix(&rgb_matrix_config);
}

uint8_t record_color_read_data(void) {
    uint8_t hs_c = hs_store_hsv_get(find_index());

    if (hs_c >= RGB_HSV_MAX) {
        return 0;
    } else {
        return hs_c;
    }
}

uint8_t hs_record_color_read_data(void) {
    uint8_t hs_c = hs_store_hsv_get(0);

    if (hs_c >= RGB_HSV_MAX) {
        return 0;
    } else {
        return hs_c;
    }
}


uint8_t record_color_hsv(bool status) {
    uint8_t temp = RGB_HSV_MAX;
    uint8_t rgb_hsv_index = record_color_read_data();

    for (uint8_t i = 0; i < (sizeof(sixth_gear_buff) / sizeof(sixth_gear_buff[0])); i++) {
        if (rgb_matrix_get_mode() == sixth_gear_buff[i]) {
            temp = RGB_HSV_MAX - 1;
            break;
        } 
    }

    if (rgb_hsv_index != temp)
        rgb_hsv_index = (rgb_hsv_index + 1) % temp;
    else
        return 0xFF;
  
    rgb_matrix_sethsv(rgb_hsvs[rgb_hsv_index][0], rgb_hsvs[rgb_hsv_index][1], rgb_matrix_get_val());

    hs_store_hsv_set(find_index(), rgb_hsv_index);
    return rgb_hsv_index;
}

bool rk_bat_req_flag;

void query(void) {
    if (rk_bat_req_flag) {
        uint8_t battery = hs_battery_percent();
#ifdef RGBLIGHT_ENABLE
        for (uint8_t i = 0; i < (RGB_MATRIX_LED_COUNT - RGBLED_NUM); i++) {
            rgb_matrix_set_color(i, 0, 0, 0);
        }
#else
        rgb_matrix_set_color_all(0x00, 0x00, 0x00);
#endif
        const uint8_t mi_index[10] = RGB_MATRIX_BAT_INDEX_MAP;
        for (uint8_t i = 0; i < 10; i++) {
            if (battery <= 100 && ((i < (battery / 10)) || (i < 1))) {
                if (battery >= (IM_BAT_REQ_LEVEL1_VAL)) {
                    rgb_matrix_set_color(mi_index[i], IM_BAT_REQ_LEVEL1_COLOR);
                } else if (battery >= (IM_BAT_REQ_LEVEL2_VAL)) {
                    rgb_matrix_set_color(mi_index[i], IM_BAT_REQ_LEVEL2_COLOR);
                } else if (battery >= (IM_BAT_REQ_LEVEL3_VAL)) {
                    rgb_matrix_set_color(mi_index[i], IM_BAT_REQ_LEVEL3_COLOR);
                } else {
                    rgb_matrix_set_color(mi_index[i], IM_BAT_REQ_LEVEL4_COLOR);
                }
            } else {
                rgb_matrix_set_color(mi_index[i], 0x00, 0x00, 0x00);
            }
        }
    }
}
