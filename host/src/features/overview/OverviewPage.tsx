import { BatteryCharging, Link2, Radio } from 'lucide-react';
import { Link } from 'react-router-dom';
import { formatDistanceToNow } from 'date-fns';
import { zhCN } from 'date-fns/locale';
import { Card, CardDescription, CardHeader, CardTitle } from '../../components/ui/card';
import { useHistory, useStatus } from '../../lib/queries';
import { cn } from '../../components/ui/utils';
import WpmCard from '../wpm/WpmCard';
import {
  CHARGING_CHARGING,
  CHARGING_FULL,
  FLAG_ANTI_BOUNCE,
  FLAG_MAC,
  FLAG_RGBREC,
  FLAG_RGB_ON,
  LINK_UNKNOWN,
  MODE_UNKNOWN,
  POWER_UNKNOWN,
  batteryLabel,
  effectiveCharging,
  hasFlag,
  isOnline,
  linkLabel,
  lockLabels,
  modeLabel,
  powerLabel,
  statusSummary,
} from '../../lib/types';

export default function OverviewPage() {
  const status = useStatus().data ?? null;
  const history = useHistory().data;
  const online = isOnline(status);
  const battery = status?.battery ?? null;
  const charging = effectiveCharging(status);
  const events = history?.events.slice(-5).reverse() ?? [];
  const locks = lockLabels(status);

  return (
    <div className="space-y-5">
      {!online ? (
        <div className="rounded-xl border border-amber-500/30 bg-amber-500/10 p-4 text-sm text-amber-200">
          键盘当前未连接。请使用 USB 有线或 2.4G 接收器；蓝牙模式不提供 raw HID 状态通道。
        </div>
      ) : null}

      <Card className="relative overflow-hidden">
        <div className="absolute -right-24 -top-24 h-64 w-64 rounded-full bg-cyan-400/10 blur-3xl" />
        <div className="relative grid gap-6 md:grid-cols-[220px_1fr]">
          <div className="flex items-center justify-center">
            <div className="relative grid h-48 w-48 place-items-center">
              <svg viewBox="0 0 200 200" className="absolute inset-0 -rotate-90">
                <circle cx="100" cy="100" r="86" fill="none" stroke="#1e293b" strokeWidth="10" />
                <circle
                  cx="100"
                  cy="100"
                  r="86"
                  fill="none"
                  stroke="url(#battery-gradient)"
                  strokeWidth="10"
                  strokeLinecap="round"
                  pathLength="100"
                  strokeDasharray={`${battery ?? 0} 100`}
                />
                <defs>
                  <linearGradient id="battery-gradient" x1="0" y1="0" x2="1" y2="1">
                    <stop offset="0%" stopColor="#22d3ee" />
                    <stop offset="100%" stopColor="#a78bfa" />
                  </linearGradient>
                </defs>
              </svg>
              <div className="text-center">
                <div className="text-5xl font-semibold tracking-tight">
                  {batteryLabel(battery).replace('%', '')}
                  <span className="text-lg text-slate-500">%</span>
                </div>
                <div className="mt-1 text-xs text-slate-400">
                  {charging === CHARGING_FULL
                    ? '已充满'
                    : charging === CHARGING_CHARGING
                      ? '充电中'
                      : '放电中'}
                </div>
              </div>
            </div>
          </div>

          <div className="flex min-w-0 flex-col justify-center">
            <div className="flex items-center gap-2">
              <span className={cn('h-2.5 w-2.5 rounded-full', online ? 'bg-emerald-400' : 'bg-slate-600')} />
              <strong className="text-lg">{statusSummary(status)}</strong>
            </div>
            <p className="mt-2 text-sm text-slate-400">
              {online
                ? `${modeLabel(status?.mode ?? MODE_UNKNOWN)} · ${linkLabel(status?.link ?? LINK_UNKNOWN)}`
                : '等待键盘唤醒或插入接收器'}
            </p>
            <div className="mt-4 flex flex-wrap items-center gap-2">
              <Link
                to="/battery"
                className="inline-flex h-10 items-center gap-2 rounded-xl border border-slate-700 bg-slate-800 px-4 text-sm text-slate-100 hover:bg-slate-700"
              >
                <BatteryCharging className="h-4 w-4" />
                查看电量历史
              </Link>
              <span className="text-xs text-slate-500">
                固件 {status?.firmwareVersion ?? '--'} · 设置版本 {status?.settingsVersion ?? '--'}
              </span>
            </div>
            <p className="mt-3 text-xs text-slate-500">
              更新于{' '}
              {status?.updatedAt
                ? formatDistanceToNow(status.updatedAt * 1000, { addSuffix: true, locale: zhCN })
                : '--'}
            </p>
          </div>
        </div>
      </Card>

      <div className="grid gap-4 sm:grid-cols-2 xl:grid-cols-4">
        {[
          ['当前模式', modeLabel(status?.mode ?? MODE_UNKNOWN), 'USB / 2.4G / 蓝牙'],
          ['链路状态', linkLabel(status?.link ?? LINK_UNKNOWN), '接收器或蓝牙连接'],
          ['电源状态', powerLabel(status?.powerState ?? POWER_UNKNOWN), '空闲调光 / 休眠阶段'],
          [
            '当前层 / 默认层',
            `${status?.layer ?? '--'} / ${status?.defaultLayer ?? '--'}`,
            '键盘当前活动层',
          ],
        ].map(([label, value, hint]) => (
          <Card key={label}>
            <p className="text-xs text-slate-500">{label}</p>
            <strong className="mt-2 block text-2xl font-semibold text-slate-100">{value}</strong>
            <p className="mt-1 text-xs text-slate-500">{hint}</p>
          </Card>
        ))}
      </div>

      <div className="grid gap-4 lg:grid-cols-2">
        <Card>
          <CardHeader>
            <div>
              <CardTitle>锁定键与布局</CardTitle>
              <CardDescription>来自键盘状态快照</CardDescription>
            </div>
            <Link2 className="h-5 w-5 text-cyan-300" />
          </CardHeader>
          <div className="flex flex-wrap gap-2">
            {locks.map((lock) => (
              <span
                key={lock.label}
                className={cn(
                  'rounded-full border px-3 py-1.5 text-sm',
                  lock.active
                    ? 'border-cyan-400/40 bg-cyan-400/10 text-cyan-200'
                    : 'border-slate-800 bg-slate-950/40 text-slate-500',
                )}
              >
                {lock.label}
                <span className="ml-2 text-xs">{lock.active ? '开' : '关'}</span>
              </span>
            ))}
            <span
              className={cn(
                'rounded-full border px-3 py-1.5 text-sm',
                hasFlag(status, FLAG_MAC)
                  ? 'border-violet-400/40 bg-violet-400/10 text-violet-200'
                  : 'border-slate-800 bg-slate-950/40 text-slate-500',
              )}
            >
              {hasFlag(status, FLAG_MAC) ? 'macOS 布局' : 'Windows 布局'}
            </span>
          </div>
        </Card>

        <Card>
          <CardHeader>
            <div>
              <CardTitle>灯光与功能</CardTitle>
              <CardDescription>背光、录制与防连击状态</CardDescription>
            </div>
            <Radio className="h-5 w-5 text-cyan-300" />
          </CardHeader>
          <dl className="grid gap-3 text-sm sm:grid-cols-2">
            {[
              ['背光', hasFlag(status, FLAG_RGB_ON) ? '开启' : '关闭'],
              ['亮度', status?.rgbBrightness == null ? '--' : String(status.rgbBrightness)],
              [
                'RGB 录制',
                hasFlag(status, FLAG_RGBREC)
                  ? `通道 ${status?.rgbrecChannel == null ? '?' : status.rgbrecChannel + 1}`
                  : '未录制',
              ],
              ['防连击', hasFlag(status, FLAG_ANTI_BOUNCE) ? '开启' : '关闭'],
              ['无线模块', status?.wirelessVersion ? String(status.wirelessVersion) : '--'],
              ['空闲时间', status == null ? '--' : `${status.idleSeconds}s`],
            ].map(([label, value]) => (
              <div key={label} className="rounded-xl bg-slate-950/40 p-3">
                <dt className="text-xs text-slate-500">{label}</dt>
                <dd className="mt-1 font-medium text-slate-200">{value}</dd>
              </div>
            ))}
          </dl>
        </Card>
      </div>

      <WpmCard />

      <Card>
        <CardHeader>
          <div>
            <CardTitle>最近事件</CardTitle>
            <CardDescription>低电、充满与连接变化</CardDescription>
          </div>
        </CardHeader>
        <div className="space-y-2">
          {events.length ? (
            events.map((event) => (
              <div key={`${event.t}-${event.title}`} className="rounded-xl border border-slate-800 bg-slate-950/40 p-3">
                <div className="flex items-center justify-between gap-2">
                  <strong className="text-sm text-slate-200">{event.title}</strong>
                  <span className="text-[11px] text-slate-500">
                    {formatDistanceToNow(event.t * 1000, { addSuffix: true, locale: zhCN })}
                  </span>
                </div>
                <p className="mt-1 text-xs text-slate-400">{event.message}</p>
              </div>
            ))
          ) : (
            <p className="text-sm text-slate-500">暂无事件</p>
          )}
        </div>
      </Card>
    </div>
  );
}
