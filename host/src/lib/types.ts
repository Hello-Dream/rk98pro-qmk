export const CHARGING_DISCHARGING = 0;
export const CHARGING_CHARGING = 1;
export const CHARGING_FULL = 2;
export const CHARGING_UNKNOWN = 0xff;

export const LINK_NONE = 0;
export const LINK_PAIRING = 1;
export const LINK_CONNECTED = 2;
export const LINK_DISCONNECTED = 3;
export const LINK_REJECT = 4;
export const LINK_UNKNOWN = 0xff;

export const MODE_USB = 0;
export const MODE_BT1 = 1;
export const MODE_BT2 = 2;
export const MODE_BT3 = 3;
export const MODE_2G4 = 6;
export const MODE_UNKNOWN = 0xff;

export const POWER_ACTIVE = 0;
export const POWER_CHARGING = 1;
export const POWER_FULL = 2;
export const POWER_LOW_BATTERY = 3;
export const POWER_DIM = 4;
export const POWER_OFF = 5;
export const POWER_UNKNOWN = 0xff;

export const FLAG_ONLINE = 1 << 0;
export const FLAG_LOW_BATTERY = 1 << 1;
export const FLAG_USB = 1 << 2;
export const FLAG_RGBREC = 1 << 3;
export const FLAG_RGB_ON = 1 << 4;
export const FLAG_ANTI_BOUNCE = 1 << 5;
export const FLAG_MAC = 1 << 6;

export const LOCK_CAPS = 1 << 0;
export const LOCK_NUM = 1 << 1;
export const LOCK_SCROLL = 1 << 2;
export const LOCK_WIN = 1 << 3;
export const LOCK_CAPS_WORD = 1 << 4;

export const FULL_BATTERY_MIN = 98;

export interface Status {
  seq: number;
  version: number;
  firmwareVersion: string;
  settingsVersion: number;
  battery: number | null;
  charging: number;
  link: number;
  mode: number;
  powerState: number;
  flags: number;
  locks: number;
  layer: number;
  defaultLayer: number;
  wpm: number;
  idleSeconds: number;
  rgbrecChannel: number | null;
  rgbMode: number;
  rgbBrightness: number;
  wirelessVersion: number;
  online: boolean;
  updatedAt: number;
}

export interface HistorySample {
  t: number;
  battery: number | null;
  charging: number;
  link: number;
  mode: number;
  online: boolean;
}

export interface HistoryEvent {
  t: number;
  kind: string;
  title: string;
  message: string;
}

export interface HistorySnapshot {
  samples: HistorySample[];
  events: HistoryEvent[];
}

export interface Preferences {
  notifyLowBattery: boolean;
  notifyFull: boolean;
  notifyDisconnect: boolean;
}

export interface WpmSample {
  t: number;
  wpm: number;
}

export interface WpmStats {
  current: number;
  peak: number;
  average5m: number;
  samples: WpmSample[];
}

export function hasFlag(status: Status | null, flag: number): boolean {
  return Boolean(status && (status.flags & flag));
}

export function hasLock(status: Status | null, lock: number): boolean {
  return Boolean(status && (status.locks & lock));
}

export function isOnline(status: Status | null): boolean {
  return Boolean(status?.online);
}

/**
 * 充满状态归一化：固件误报 FULL 且电量不足时按充电中处理。
 * 这是上位机的第二道防线，正常情况由固件保证。
 */
export function effectiveCharging(status: Status | null): number {
  if (!status) {
    return CHARGING_UNKNOWN;
  }
  if (
    status.charging === CHARGING_FULL &&
    (status.battery == null || status.battery < FULL_BATTERY_MIN)
  ) {
    return CHARGING_CHARGING;
  }
  return status.charging;
}

export function modeLabel(mode: number): string {
  switch (mode) {
    case MODE_USB:
      return 'USB';
    case MODE_BT1:
      return 'BT1';
    case MODE_BT2:
      return 'BT2';
    case MODE_BT3:
      return 'BT3';
    case MODE_2G4:
      return '2.4G';
    default:
      return '未知';
  }
}

export function linkLabel(link: number): string {
  switch (link) {
    case LINK_NONE:
      return '未连接';
    case LINK_PAIRING:
      return '配对中';
    case LINK_CONNECTED:
      return '已连接';
    case LINK_DISCONNECTED:
      return '已断开';
    case LINK_REJECT:
      return '被拒绝';
    default:
      return '未知';
  }
}

export function chargingLabel(charging: number): string {
  switch (charging) {
    case CHARGING_DISCHARGING:
      return '放电中';
    case CHARGING_CHARGING:
      return '充电中';
    case CHARGING_FULL:
      return '已充满';
    default:
      return '未知';
  }
}

export function powerLabel(powerState: number): string {
  switch (powerState) {
    case POWER_ACTIVE:
      return '活跃';
    case POWER_CHARGING:
      return '充电中';
    case POWER_FULL:
      return '已充满';
    case POWER_LOW_BATTERY:
      return '低电';
    case POWER_DIM:
      return '调光中';
    case POWER_OFF:
      return '已关灯';
    default:
      return '未知';
  }
}

export function batteryLabel(battery: number | null): string {
  return battery == null ? '--' : `${battery}%`;
}

export function lockLabels(status: Status | null): Array<{ label: string; active: boolean }> {
  return [
    { label: 'Caps', active: hasLock(status, LOCK_CAPS) || hasLock(status, LOCK_CAPS_WORD) },
    { label: 'Num', active: hasLock(status, LOCK_NUM) },
    { label: 'Scroll', active: hasLock(status, LOCK_SCROLL) },
    { label: 'Win', active: hasLock(status, LOCK_WIN) },
    { label: 'Mac', active: hasFlag(status, FLAG_MAC) },
  ];
}

export function statusSummary(status: Status | null): string {
  if (!status || !isOnline(status)) {
    return '等待键盘连接';
  }

  const parts = [`${modeLabel(status.mode)} ${linkLabel(status.link)}`];
  if (status.battery != null) {
    parts.push(`电量 ${status.battery}%`);
  }
  const charging = effectiveCharging(status);
  if (charging === CHARGING_CHARGING) {
    parts.push('充电中');
  } else if (charging === CHARGING_FULL) {
    parts.push('已充满');
  }
  return parts.join(' · ');
}
