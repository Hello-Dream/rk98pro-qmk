import { describe, expect, it } from 'vitest';
import {
  CHARGING_CHARGING,
  CHARGING_DISCHARGING,
  CHARGING_FULL,
  FLAG_MAC,
  LOCK_CAPS,
  LOCK_NUM,
  MODE_2G4,
  MODE_USB,
  Status,
  effectiveCharging,
  linkLabel,
  lockLabels,
  modeLabel,
  powerLabel,
  statusSummary,
} from './types';

function status(overrides: Partial<Status> = {}): Status {
  return {
    seq: 1,
    version: 1,
    firmwareVersion: '1.0.1',
    settingsVersion: 1,
    battery: 80,
    charging: CHARGING_DISCHARGING,
    link: 2,
    mode: MODE_2G4,
    powerState: 0,
    flags: 0,
    locks: 0,
    layer: 0,
    defaultLayer: 0,
    wpm: 0,
    idleSeconds: 0,
    rgbrecChannel: null,
    rgbMode: 0,
    rgbBrightness: 90,
    wirelessVersion: 1,
    online: true,
    updatedAt: 1_700_000_000,
    ...overrides,
  };
}

describe('状态字段与标签', () => {
  it('93% 的 FULL 归一化为充电中', () => {
    const value = status({ charging: CHARGING_FULL, battery: 93 });
    expect(effectiveCharging(value)).toBe(CHARGING_CHARGING);
    expect(statusSummary(value)).toContain('充电中');
    expect(statusSummary(value)).not.toContain('已充满');
  });

  it('98% 的 FULL 保持已充满', () => {
    expect(effectiveCharging(status({ charging: CHARGING_FULL, battery: 98 }))).toBe(
      CHARGING_FULL,
    );
  });

  it('未知电量不触发充满', () => {
    expect(effectiveCharging(status({ charging: CHARGING_FULL, battery: null }))).toBe(
      CHARGING_CHARGING,
    );
  });

  it('格式化模式和连接标签', () => {
    expect(modeLabel(MODE_USB)).toBe('USB');
    expect(modeLabel(MODE_2G4)).toBe('2.4G');
    expect(linkLabel(2)).toBe('已连接');
    expect(powerLabel(4)).toBe('调光中');
  });

  it('组合锁定键和 macOS 标志', () => {
    const value = status({ locks: LOCK_CAPS | LOCK_NUM, flags: FLAG_MAC });
    const labels = lockLabels(value);
    expect(labels.find((item) => item.label === 'Caps')?.active).toBe(true);
    expect(labels.find((item) => item.label === 'Num')?.active).toBe(true);
    expect(labels.find((item) => item.label === 'Scroll')?.active).toBe(false);
    expect(labels.find((item) => item.label === 'Mac')?.active).toBe(true);
  });

  it('离线时显示等待连接', () => {
    expect(statusSummary(null)).toBe('等待键盘连接');
    expect(statusSummary(status({ online: false }))).toBe('等待键盘连接');
  });
});
