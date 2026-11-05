import { describe, expect, it } from 'vitest';
import type { HistorySample } from '../../lib/types';
import { batteryStats, buildBatterySegments, smoothBatterySeries } from './battery';

function sample(t: number, battery: number | null, online = true): HistorySample {
  return {
    t,
    battery,
    charging: 0,
    link: online ? 2 : 0xff,
    mode: online ? 6 : 0xff,
    online,
  };
}

describe('电量曲线处理', () => {
  it('用中值 + 移动平均压掉单点尖刺', () => {
    const samples = [sample(0, 80), sample(60, 81), sample(120, 60), sample(180, 81), sample(240, 82)];
    const smoothed = smoothBatterySeries(samples);
    const values = smoothed.map((item) => item.displayBattery);

    expect(values.some((value) => value === 60)).toBe(false);
    expect(values.every((value) => value != null && value >= 80 && value <= 82)).toBe(true);
  });

  it('统计使用原始样本，不受显示平滑影响', () => {
    const samples = [sample(0, 80), sample(60, 81), sample(120, 60), sample(180, 81), sample(240, 82)];
    const stats = batteryStats(samples);

    expect(stats.current).toBe(82);
    expect(stats.minimum).toBe(60);
    expect(stats.average).toBe(77);
    expect(stats.count).toBe(5);
  });

  it('离线样本把折线断成两段', () => {
    const samples = [sample(0, 80), sample(60, null, false), sample(120, 79)];
    const segments = buildBatterySegments(smoothBatterySeries(samples), 0, 120);

    expect(segments).toHaveLength(2);
    expect(segments[0].points).toHaveLength(1);
    expect(segments[1].points).toHaveLength(1);
  });

  it('超过 10 分钟的时间空档也断开折线', () => {
    const samples = [sample(0, 80), sample(60, 81), sample(1201, 82)];
    const segments = buildBatterySegments(smoothBatterySeries(samples), 0, 1201);

    expect(segments).toHaveLength(2);
  });
});
