// Copyright 2024 SDK (@sdk66)
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define USB_POWER_EN_PIN                    B1 // USB ENABLE pin
#define LED_POWER_EN_PIN                    A5 // LED ENABLE pin
#define HS_BAT_CABLE_PIN                    A7 // USB insertion detection pin
#define HS_LED_BOOSTING_PIN                 B7 // LED BOOSTING,Power boost pin
#define LED_ENABLE_ON 1

#define BAT_FULL_PIN                        A15
#define BAT_FULL_STATE                      1

#define MM_BT_DEF_PIN                       C14
#define MM_2G4_DEF_PIN                      C15
#define HS_SUCCEED_TIME                     2000 // 2s

/* Device Name Definition */
#define MD_BT1_NAME "RK98PRO"
#define MD_BT2_NAME "RK98PRO"
#define MD_BT3_NAME "RK98PRO"
#define MD_DONGLE_MANUFACTURER "RK98PRO"                     

/* Device Connection RGB Indicator Light Index And Color */
#define HS_RGB_BLINK_INDEX_BT1 61
#define HS_RGB_BLINK_INDEX_BT2 60 
#define HS_RGB_BLINK_INDEX_BT3 59 
#define HS_RGB_BLINK_INDEX_2G4 52 
// #define HS_RGB_BLINK_INDEX_USB 46 

#define HS_LBACK_COLOR_BT1 RGB_BLUE
#define HS_LBACK_COLOR_BT2 RGB_BLUE
#define HS_LBACK_COLOR_BT3 RGB_BLUE
#define HS_LBACK_COLOR_2G4 RGB_GREEN  
#define HS_LBACK_COLOR_USB RGB_WHITE

#define HS_PAIR_COLOR_BT1  RGB_BLUE
#define HS_PAIR_COLOR_BT2  RGB_BLUE
#define HS_PAIR_COLOR_BT3  RGB_BLUE
#define HS_PAIR_COLOR_2G4  RGB_GREEN   
#define HS_PAIR_COLOR_USB  RGB_WHITE 

#define WIITE_B 0X3e

/* Battery */
#define BATTERY_CAPACITY_LOW       15 
#define BATTERY_CAPACITY_STOP       0 
#define RGB_MATRIX_BAT_INDEX_MAP            {64, 65, 66, 67, 68, 69, 70, 71, 72, 73}
// #define RGB_MATRIX_INDEX_BAT_START 117          
// #define RGB_MATRIX_INDEX_BAT_END   126

/* LED */
#define LED_PIN_ON_STATE 0
#define LED_CAPS_PIN   A9
#define LED_SCROLL_PIN D2
#define LED_WIN_PIN    B8 
#define LED_NUM_PIN    B9
#define LED_MAC_PIN    B7
#define LED_POV_PIN    B6 

/* UART */
#define SERIAL_DRIVER SD3
#define SD1_TX_PIN C10
#define SD1_RX_PIN C11

/* SPI */
#define SPI_DRIVER                          SPIDQ
#define SPI_SCK_PIN                         B3
#define SPI_MOSI_PIN                        B5
#define SPI_MISO_PIN                        B4

/* Flash */
#define EXTERNAL_FLASH_SPI_SLAVE_SELECT_PIN C12
#define WEAR_LEVELING_LOGICAL_SIZE          (WEAR_LEVELING_BACKING_SIZE / 2)

/* RGB Matrix */
#define HS_RGB
#define RGB_MATRIX_FRAMEBUFFER_EFFECTS
#define RGB_MATRIX_KEYPRESSES

/* 内置动效已在 keyboard.json 中全部关闭，只保留自定义效果；默认动效指向自定义效果 */
#define RGB_MATRIX_DEFAULT_MODE RGB_MATRIX_CUSTOM_HS_PLASMA

/* RGB Light */
#define RGBLED_NUM 8
#define RGBLIGHT_CUSTOM_DRIVER
#define RGBLIGHT_DISABLE_KEYCODES
#define BREATHING_VAL           40
#define RAINBOW_MOOD            40
#define LED_TYPE                RGB
#define RGBLIGHT_LED_MAP {0, 1, 2, 3, 4, 5, 6, 7}

/* Status Indicator Lamp */
#define RGB_MATRIX_BLINK_INDEX_ALL          0xFF 
#define RGB_MATRIX_BLINK_INDEX_BAT          250
#define RGB_MATRIX_BLINK_INDEX_IND          251
#define RGB_MATRIX_BLINK_COUNT              6
#define IM_MM_RGB_BLINK_INDEX_DEB           27

#define HS_RGB_BLINK_INDEX_WIN              30
#define HS_RGB_BLINK_INDEX_MAC              31
#define HS_RGB_BLINK_INDEX_BAT              2
// #define HS_RGB_INDEX_INDEX_CAP              0
// #define HS_RGB_INDEX_INDEX_NUM              0
// #define HS_RGB_INDEX_SCR                    1
// #define HS_RGB_INDEX_WIN_LOCK               1
// #define HS_RGB_BLINK_INDEX_REC              32
#define HS_RGB_BLINK_INDEX_VAI              83
#define HS_RGB_BLINK_INDEX_VAD              84
#define HS_RGB_BLINK_INDEX_SPI              85
#define HS_RGB_BLINK_INDEX_SPD              86

/* WS2812 */
#define WS2812_SPI_DRIVER       SPIDM2
#define WS2812_SPI_DIVISOR      32

/* rgb_record */
#define ENABLE_RGB_MATRIX_RGBR_PLAY

/* 自定义灯效（HS_ 前缀） */
#define ENABLE_RGB_MATRIX_HS_PLASMA
#define ENABLE_RGB_MATRIX_HS_SPARKLE
#define ENABLE_RGB_MATRIX_HS_RIPPLE
#define ENABLE_RGB_MATRIX_HS_BREATH_GRADIENT
#define ENABLE_RGB_MATRIX_HS_FIRE
#define ENABLE_RGB_MATRIX_HS_CYBER_SCAN
#define ENABLE_RGB_MATRIX_HS_RAINDROPS

#define RGBREC_CHANNEL_NUM         4
#define EECONFIG_CONFINFO_USE_SIZE (4 + 16)
#define EECONFIG_RGBREC_USE_SIZE   (RGBREC_CHANNEL_NUM * MATRIX_ROWS * MATRIX_COLS * 2)
#define EECONFIG_USER_DATA_SIZE    (EECONFIG_RGBREC_USE_SIZE + EECONFIG_CONFINFO_USE_SIZE)
#define RGBREC_EECONFIG_ADDR       (uint8_t *)(EECONFIG_USER_DATABLOCK)
#define CONFINFO_EECONFIG_ADDR     (uint32_t *)((uint32_t)RGBREC_EECONFIG_ADDR + (uint32_t)EECONFIG_RGBREC_USE_SIZE)
