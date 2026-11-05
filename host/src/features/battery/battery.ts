import type { HistorySample } from '../../lib/types';

const MAX_GAP_SECONDS = 10 * 60;

export interface SmoothedSample extends HistorySample {
  displayBattery: number | null;
}

export interface ChartPoint {
  x: number;
  y: number;
}

export interface ChartSegment {
  points: ChartPoint[];
}

function median(values: number[]): number {
  const sorted = [...values].sort((left, right) => left - right);
  return sorted[Math.floor(sorted.length / 2)];
}

function smoothRun(values: number[]): number[] {
  if (values.length === 0) {
    return [];
  }

  const medianed = values.map((_, index) =>
    median(values.slice(Math.max(0, index - 1), Math.min(values.length, index + 2))),
  );

  return medianed.map((_, index) => {
    const window = medianed.slice(Math.max(0, index - 1), Math.min(medianed.length, index + 2));
    return Math.round(window.reduce((sum, value) => sum + value, 0) / window.length);
  });
}

/** 对连续在线样本做 3 点中值 + 3 点移动平均；离线样本保持 null。 */
export function smoothBatterySeries(samples: HistorySample[]): SmoothedSample[] {
  const result: SmoothedSample[] = samples.map((sample) => ({ ...sample, displayBattery: null }));
  let runStart = -1;

  for (let index = 0; index <= samples.length; index++) {
    const sample = samples[index];
    const valid = Boolean(sample?.online && sample.battery != null);

    if (valid) {
      if (runStart < 0) {
        runStart = index;
      }
    } else if (runStart >= 0) {
      const values = samples.slice(runStart, index).map((item) => item.battery as number);
      const smoothed = smoothRun(values);
      smoothed.forEach((value, offset) => {
        result[runStart + offset].displayBattery = value;
      });
      runStart = -1;
    }
  }

  return result;
}

/** 统计使用原始样本，不被显示平滑影响。 */
export function batteryStats(samples: HistorySample[]): {
  current: number | null;
  average: number | null;
  minimum: number | null;
  count: number;
} {
  const values = samples
    .filter((sample) => sample.online && sample.battery != null)
    .map((sample) => sample.battery as number);

  if (values.length === 0) {
    return { current: null, average: null, minimum: null, count: 0 };
  }

  return {
    current: values[values.length - 1],
    average: Math.round(values.reduce((sum, value) => sum + value, 0) / values.length),
    minimum: Math.min(...values),
    count: values.length,
  };
}

/** 按离线样本或超过 10 分钟的时间空档断开折线。 */
export function buildBatterySegments(
  samples: SmoothedSample[],
  start: number,
  end: number,
): ChartSegment[] {
  const span = Math.max(1, end - start);
  const segments: ChartSegment[] = [];
  let points: ChartPoint[] = [];
  let previousT: number | null = null;

  for (const sample of samples) {
    if (sample.t < start || sample.t > end) {
      continue;
    }

    const valid = sample.online && sample.displayBattery != null;
    if (!valid) {
      if (points.length) {
        segments.push({ points });
        points = [];
      }
      previousT = sample.t;
      continue;
    }

    const hasGap = previousT != null && sample.t - previousT > MAX_GAP_SECONDS;
    if (hasGap && points.length) {
      segments.push({ points });
      points = [];
    }

    points.push({
      x: ((sample.t - start) / span) * 100,
      y: 100 - (sample.displayBattery as number),
    });
    previousT = sample.t;
  }

  if (points.length) {
    segments.push({ points });
  }

  return segments;
}
