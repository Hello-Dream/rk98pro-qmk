import { Activity, RotateCcw } from 'lucide-react';
import { Button } from '../../components/ui/button';
import { Card, CardDescription, CardHeader, CardTitle } from '../../components/ui/card';
import { useResetWpmStats, useWpmStats } from '../../lib/queries';
import { useUiStore } from '../../store/ui';
import type { WpmSample } from '../../lib/types';

/** 把最近 WPM 采样映射到 0~100 的 SVG 坐标。 */
function buildSparkline(samples: WpmSample[]): string {
  if (samples.length < 2) {
    return '';
  }
  return samples
    .map((sample, index) => {
      const x = (index / (samples.length - 1)) * 100;
      const y = 100 - (Math.min(150, Math.max(0, sample.wpm)) / 150) * 100;
      return `${x.toFixed(2)},${y.toFixed(2)}`;
    })
    .join(' ');
}

export default function WpmCard() {
  const stats = useWpmStats().data;
  const reset = useResetWpmStats();
  const pushToast = useUiStore((state) => state.pushToast);
  const samples = stats?.samples ?? [];
  const points = buildSparkline(samples);

  const resetStats = () => {
    reset.mutate(undefined, {
      onSuccess: () => pushToast('WPM 统计已重置', 'success'),
      onError: (error) => pushToast(String(error), 'error'),
    });
  };

  return (
    <Card>
      <CardHeader>
        <div>
          <CardTitle>WPM 统计</CardTitle>
          <CardDescription>当前、本次峰值和最近 5 分钟平均</CardDescription>
        </div>
        <Activity className="h-5 w-5 text-cyan-300" />
      </CardHeader>

      <div className="grid grid-cols-3 gap-3">
        {[
          ['当前', stats?.current ?? 0],
          ['峰值', stats?.peak ?? 0],
          ['5 分钟平均', Math.round(stats?.average5m ?? 0)],
        ].map(([label, value]) => (
          <div key={label} className="rounded-xl bg-slate-950/40 p-3">
            <p className="text-xs text-slate-500">{label}</p>
            <strong className="mt-1 block text-2xl font-semibold text-slate-100">{value}</strong>
          </div>
        ))}
      </div>

      <div className="mt-4 h-20 overflow-hidden rounded-xl border border-slate-800 bg-slate-950/40">
        {points ? (
          <svg viewBox="0 0 100 100" preserveAspectRatio="none" className="h-full w-full">
            <line x1="0" y1="50" x2="100" y2="50" stroke="#1e293b" strokeWidth="0.5" />
            <polyline
              points={points}
              fill="none"
              stroke="#22d3ee"
              strokeWidth="1.4"
              vectorEffect="non-scaling-stroke"
            />
          </svg>
        ) : (
          <div className="grid h-full place-items-center text-xs text-slate-500">
            还没有打字数据
          </div>
        )}
      </div>

      <div className="mt-3 flex items-center justify-between gap-3">
        <p className="text-xs text-slate-500">
          窗口打开时按 1 秒采样；托盘时 30 秒，曲线会变稀疏。
        </p>
        <Button size="sm" disabled={reset.isPending} onClick={resetStats}>
          <RotateCcw className="h-3.5 w-3.5" />
          重置统计
        </Button>
      </div>
    </Card>
  );
}
