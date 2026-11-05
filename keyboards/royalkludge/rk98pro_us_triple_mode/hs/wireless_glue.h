#include QMK_KEYBOARD_H

#include "quantum.h"
#include <stdbool.h>
#include "wireless.h"
#include "raw_hid.h"
#include "via.h"
#include "rgb_record/rgb_record.h"
#include "hs/store.h"
#include "hs/battery.h"

#ifdef MM_BT_DEF_PIN
    #define HS_BT_DEF_PIN                     MM_BT_DEF_PIN
#endif

#ifdef MM_2G4_DEF_PIN
    #define HS_2G4_DEF_PIN                    MM_2G4_DEF_PIN
#endif

#define HS_BT_PIN_STATE                   0, 1
#define HS_2G4_PIN_STATE                  0, 0
#define HS_USB_PIN_STATE                  1, x

#define HS_GET_MODE_PIN_(pin_bt, pin_2g4) ((((#pin_bt)[0] == 'x') || ((gpio_read_pin(HS_BT_DEF_PIN) + 0x30) == ((#pin_bt)[0]))) && (((#pin_2g4)[0] == 'x') || ((gpio_read_pin(HS_2G4_DEF_PIN) + 0x30) == ((#pin_2g4)[0]))))
#define HS_GET_MODE_PIN(state)            HS_GET_MODE_PIN_(state)
#define HS_MODEIO_DETECTION_TIME          50
#define HS_LBACK_TIMEOUT                  (30 * 1000)
#define HS_PAIR_TIMEOUT                   (30 * 1000) // bt
#define HS_PAIR_TIMEOUT1                  (30 * 1000) // 2.4g
#define HS_SLEEP_TIMEOUT                  (5 * 60000) // (5 * 60000)
#define HS_WIRESS_SLEEP_TIME              (30)  // Bluetooth deep sleep time：1~30min


enum modeio_mode {
    hs_none = 0,
    hs_usb,
    hs_bt,
    hs_2g4,
    hs_wireless
};

enum mm_sleep_timeout {
    hs_sleep_timeout_none = 0,
    hs_sleep_timeout_1min,
    hs_sleep_timeout_3min,
    hs_sleep_timeout_5min,
    hs_sleep_timeout_10min,
    hs_sleep_timeout_20min,
    hs_sleep_timeout_30min,
    hs_sleep_timeout_vendor, // Max. 7
};

/* 3.0 ABI：0x01 为已废弃的旧上位机 RPC（保留不复用），
 * 0x10–0x21 为 VIA 控件，0x22 为只读状态。
 * 已发布后只能追加，不能重排或复用。 */
enum via_custom_defined_id {
    id_sleep_timeout = 0x10,
    id_side_light_mode = 0x11,
    id_anti_bounce = 0x12,
    id_power_idle_dim = 0x13,
    id_power_idle_off = 0x14,
    id_power_usb_save = 0x15,
    id_power_low_bat_warn = 0x16,
    id_power_low_bat_brightness = 0x17,
    id_power_status_led = 0x18,
    id_battery_check = 0x19,
    id_rgbrec_channel = 0x1A,
    id_rgbrec_record = 0x1B,
    id_rgbrec_clear = 0x1C,
    id_rgbrec_copy = 0x1D,
    id_rgbrec_active_channel = 0x1E,
    id_battery_level = 0x1F,
    id_connection_status = 0x20,
    id_power_state = 0x21,
    id_host_status = 0x22,
};

enum hs_power_state_id {
    HS_POWER_STATE_ACTIVE = 0,
    HS_POWER_STATE_CHARGING,
    HS_POWER_STATE_FULL,
    HS_POWER_STATE_LOW_BATTERY,
    HS_POWER_STATE_DIM,
    HS_POWER_STATE_OFF,
    HS_POWER_STATE_UNKNOWN = 0xFF,
};

extern bool lower_sleep;
extern uint8_t hs_get_sleep_timeout(void);
extern void hs_set_sleep_timeout(uint8_t time);
extern void hs_settings_update(void);
extern void hs_reset_settings(void);

/* 键盘层提供给 VIA 自定义菜单的读写接口 */
uint8_t hs_get_side_light_mode(void);
void hs_set_side_light_mode(uint8_t mode);
bool hs_get_anti_bounce(void);
void hs_set_anti_bounce(bool enabled);
uint8_t hs_get_rgbrec_channel(void);
void hs_set_rgbrec_channel(uint8_t channel);
void hs_rgbrec_toggle_record(void);
void hs_rgbrec_clear_current(void);
void hs_rgbrec_copy_next(void);
void hs_bat_show(void);

bool mm_get_rgb_enable(void);
void mm_set_rgb_enable(bool state);
bool hs_keymap_is_mac(void);

/* 电源状态与只读状态快照共用同一组采集函数。 */
uint8_t  hs_power_state_code(void);
bool     hs_power_low_battery_active(void);
uint16_t hs_power_idle_seconds(void);

/* 电源/续航设置（VIA Power 菜单） */
uint8_t hs_power_get_config(uint8_t value_id);
void hs_power_set_config(uint8_t value_id, uint8_t value);
const char *hs_power_state_name(void);

/* 空闲省电：VIA 写操作也会调用，确保从调光/关灯状态恢复 */
void hs_power_activity(void);

/* 用户在 VIA 中主动打开灯效时，取消状态灯的临时点亮标记 */
void hs_power_rgb_user_enabled(void);

extern uint32_t post_init_timer;
extern bool linker_ok;
void wireless_post_task(void);
bool process_record_wls(uint16_t keycode, keyrecord_t *record);
void hs_set_sleep_state(void);
void rgb_matrix_wls_indicator_set(uint8_t index, RGB rgb, uint32_t interval, uint8_t times);
void wireless_devs_change_kb(uint8_t old_devs, uint8_t new_devs, bool reset);
void rgb_matrix_wls_indicator(void);
void hs_wls_connected_indicator(void);
void hs_wls_indicator_force_task(void);
void hs_rgb_blink_hook_user(void);
extern uint32_t wls_rgb_indicator_timer;

bool hs_rgb_blink_hook(void);
bool hs_mode_scan(bool update, uint8_t moude, uint8_t lsat_btdev);
bool hs_modeio_detection(bool update, uint8_t *mode, uint8_t lsat_btdev);
void hs_rgb_blink_set_timer(uint32_t time);