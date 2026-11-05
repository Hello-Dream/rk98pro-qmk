import { useMemo, useState } from 'react';
import { format } from 'date-fns';
import { Card, CardDescription, CardHeader, CardTitle } from '../../components/ui/card';
import { useHistory, useStatus } from '../../lib/queries';
import { cn } from '../../components/ui/utils';
import {
  MODE_UNKNOWN,
  batteryLabel,
  chargingLabel,
  effectiveCharging,
  modeLabel,
} from '../../lib/types';
import { batteryStats, buildBatterySegments, smoothBatterySeries } from './battery';

const RANGES = [
  { label: '6 小时', seconds: 6 * 3600 },
  { label: '24 小时', seconds: 24 * 3600 },
  { label: '7 天', seconds: 7 * 86400 },
];

export default function BatteryPage() {
  const history = useHistory().data;
  const status = useStatus().data ?? null;
  const [range, setRange] = useState(24 * 3600);

  const rawSamples = useMemo(() => history?.samples ?? [], [history]);
  const latest = rawSamples.at(-1)?.t ?? 0;
  const start = latest - range;

  const rangeSamples = useMemo(
    () => rawSamples.filter((sample) => sample.t >= start && sample.t <= latest),
    [rawSamples, start, latest],
  );

  const smoothed = useMemo(() => smoothBatterySeries(rangeSamples), [rangeSamples]);
  const segments = useMemo(
    () => buildBatterySegments(smoothed, start, latest),
    [smoothed, start, latest],
  );
  const stats = useMemo(() => batteryStats(rangeSamples), [rangeSamples]);
  const hasData = segments.some((segment) => segment.points.length > 0);

  return (
    <div className="space-y-5">
      <Card>
        <CardHeader>
          <div>
            <CardTitle>电量历史</CardTitle>
            <CardDescription>每 5 分钟或状态变化时采样，最多保留 7 天</CardDescription>
          </div>
          <div className="flex rounded-xl border border-slate-800 bg-slate-950/50 p-1">
            {RANGES.map((item) => (
              <button
                key={item.seconds}
                type="button"
                onClick={() => setRange(item.seconds)}
                className={cn(
                  'rounded-lg px-3 py-1.5 text-xs transition-colors',
                  range === item.seconds
                    ? 'bg-cyan-400/15 text-cyan-200'
                    : 'text-slate-400 hover:text-slate-200',
                )}
              >
                {item.label}
              </button>
            ))}
          </div>
        </CardHeader>
        <div className="relative h-72 overflow-hidden rounded-xl border border-slate-800 bg-slate-950/40">
          {hasData ? (
            <svg viewBox="0 0 100 100" preserveAspectRatio="none" className="h-full w-full">
              {[25, 50, 75].map((line) => (
                <line key={line} x1="0" y1={line} x2="100" y2={line} stroke="#1e293b" strokeWidth="0.4" />
              ))}
              {segments.map((segment, index) =>
                segment.points.length === 1 ? (
                  <circle
                    key={index}
                    cx={segment.points[0].x}
                    cy={segment.points[0].y}
                    r="0.8"
                    fill="#22d3ee"
                  />
                ) : (
                  <polyline
                    key={index}
                    points={segment.points.map((point) => `${point.x.toFixed(2)},${point.y.toFixed(2)}`).join(' ')}
                    fill="none"
                    stroke="#22d3ee"
                    strokeWidth="1.2"
                    vectorEffect="non-scaling-stroke"
                  />
                ),
              )}
            </svg>
          ) : (
            <div className="grid h-full place-items-center text-sm text-slate-500">还没有足够的采样数据</div>
          )}
          <div className="absolute inset-y-0 left-2 flex flex-col justify-between py-2 text-[10px] text-slate-600">
            <span>100%</span>
            <span>50%</span>
            <span>0%</span>
          </div>
        </div>
        <div className="mt-4 grid grid-cols-2 gap-3 text-sm sm:grid-cols-4">
          {[
            ['当前', stats.current == null ? '--' : `${stats.current}%`],
            ['区间平均', stats.average == null ? '--' : `${stats.average}%`],
            ['区间最低', stats.minimum == null ? '--' : `${stats.minimum}%`],
            ['采样点', String(stats.count)],
          ].map(([label, value]) => (
            <div key={label} className="rounded-xl bg-slate-950/40 p-3">
              <p className="text-xs text-slate-500">{label}</p>
              <strong className="mt-1 block text-lg">{value}</strong>
            </div>
          ))}
        </div>
      </Card>

      <div className="grid gap-4 lg:grid-cols-2">
        <Card>
          <CardHeader>
            <div>
              <CardTitle>当前电池</CardTitle>
              <CardDescription>来自键盘的最新读数</CardDescription>
            </div>
          </CardHeader>
          <dl className="grid gap-3 text-sm sm:grid-cols-2">
            {[
              ['电量', batteryLabel(status?.battery ?? stats.current)],
              ['充电', chargingLabel(effectiveCharging(status))],
              ['模式', modeLabel(status?.mode ?? MODE_UNKNOWN)],
              [
                '最后更新',
                status?.updatedAt
                  ? format(status.updatedAt * 1000, 'yyyy-MM-dd HH:mm:ss')
                  : '--',
              ],
            ].map(([label, value]) => (
              <div key={label} className="rounded-xl bg-slate-950/40 p-3">
                <dt className="text-xs text-slate-500">{label}</dt>
                <dd className="mt-1 font-medium text-slate-200">{value}</dd>
              </div>
            ))}
          </dl>
        </Card>
        <Card>
          <CardHeader>
            <div>
              <CardTitle>记录说明</CardTitle>
              <CardDescription>数据仅保存在本机</CardDescription>
            </div>
          </CardHeader>
          <p className="text-sm leading-relaxed text-slate-400">
            上位机启动后开始记录；键盘休眠或断连时会留下空档，不会把缺失数据补成直线。
            曲线只做显示平滑，统计仍使用原始采样值。历史文件位于应用数据目录，卸载程序不会自动删除。
          </p>
        </Card>
      </div>
    </div>
  );
}
