import { useState } from 'react';
import { NavLink, Outlet, useLocation } from 'react-router-dom';
import { BatteryCharging, Bell, Gauge, RefreshCw } from 'lucide-react';
import { useRefresh, useStatus } from '../lib/queries';
import { useDeviceEvents } from '../lib/events';
import { isOnline, statusSummary } from '../lib/types';
import { useUiStore } from '../store/ui';
import { Button } from './ui/button';
import { ToastViewport } from './ui/toast';
import { cn } from './ui/utils';
import NotificationSettingsDialog from '../features/notifications/NotificationSettingsDialog';
import MonitorErrorBanner from './MonitorErrorBanner';

const NAV = [
  { to: '/', label: '状态', icon: Gauge, title: '键盘状态' },
  { to: '/battery', label: '电量', icon: BatteryCharging, title: '电量历史' },
];

export default function AppShell() {
  useDeviceEvents();
  const location = useLocation();
  const status = useStatus().data ?? null;
  const refresh = useRefresh();
  const pushToast = useUiStore((state) => state.pushToast);
  const [notificationsOpen, setNotificationsOpen] = useState(false);
  const page = NAV.find((item) => item.to === location.pathname) ?? NAV[0];
  const online = isOnline(status);
  const summary = statusSummary(status);

  return (
    <div className="flex min-h-screen text-slate-200">
      <aside className="hidden w-64 shrink-0 flex-col border-r border-slate-800/80 bg-slate-950/70 p-4 lg:flex">
        <div className="mb-8 flex items-center gap-3 px-2">
          <div className="grid h-10 w-10 place-items-center rounded-xl bg-cyan-400 text-sm font-black text-slate-950">
            RK
          </div>
          <div>
            <strong className="block text-sm">RK98PRO</strong>
            <span className="text-xs text-slate-500">Monitor</span>
          </div>
        </div>
        <nav className="flex flex-1 flex-col gap-1" aria-label="主导航">
          {NAV.map(({ to, label, icon: Icon }) => (
            <NavLink
              key={to}
              to={to}
              end={to === '/'}
              className={({ isActive }) =>
                cn(
                  'flex items-center gap-3 rounded-xl px-3 py-2.5 text-sm transition-colors focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-cyan-400/70',
                  isActive
                    ? 'bg-cyan-400/10 text-cyan-200'
                    : 'text-slate-400 hover:bg-slate-900 hover:text-slate-100',
                )
              }
            >
              <Icon className="h-4 w-4" />
              {label}
            </NavLink>
          ))}
        </nav>
        <div className="mt-4 rounded-xl border border-slate-800 bg-slate-900/60 p-3 text-xs">
          <div className="flex items-center gap-2">
            <span className={cn('h-2 w-2 rounded-full', online ? 'bg-emerald-400' : 'bg-slate-600')} />
            <strong>{online ? '已连接' : '未连接'}</strong>
          </div>
          <p className="mt-1 text-slate-500">{summary}</p>
        </div>
      </aside>

      <main className="flex min-w-0 flex-1 flex-col">
        <header className="sticky top-0 z-30 flex items-center justify-between gap-3 border-b border-slate-800/70 bg-[#070b13]/85 px-5 py-4 backdrop-blur">
          <div>
            <p className="text-[11px] uppercase tracking-[0.2em] text-cyan-300/70">RK98PRO 三模键盘</p>
            <h1 className="mt-0.5 text-xl font-semibold text-slate-100">{page.title}</h1>
          </div>
          <div className="flex items-center gap-2">
            <span
              className={cn(
                'rounded-full border px-3 py-1 text-xs',
                online
                  ? 'border-emerald-500/30 bg-emerald-500/10 text-emerald-300'
                  : 'border-slate-700 bg-slate-900 text-slate-400',
              )}
            >
              {online ? '在线' : '离线'}
            </span>
            <Button
              size="icon"
              aria-label="通知设置"
              title="通知设置"
              onClick={() => setNotificationsOpen(true)}
            >
              <Bell className="h-4 w-4" />
            </Button>
            <Button
              size="icon"
              aria-label="立即刷新"
              title="立即刷新"
              disabled={refresh.isPending}
              onClick={() => {
                refresh.mutate(undefined, {
                  onSuccess: () => pushToast('已请求刷新', 'success'),
                  onError: (error) => pushToast(String(error), 'error'),
                });
              }}
            >
              <RefreshCw className={cn('h-4 w-4', refresh.isPending && 'animate-spin')} />
            </Button>
          </div>
        </header>

        <div className="mx-auto w-full max-w-6xl flex-1 p-5">
          <MonitorErrorBanner />
          <Outlet />
        </div>

        <nav className="sticky bottom-0 flex justify-around border-t border-slate-800 bg-slate-950/95 p-2 lg:hidden">
          {NAV.map(({ to, label, icon: Icon }) => (
            <NavLink
              key={to}
              to={to}
              end={to === '/'}
              className={({ isActive }) =>
                cn(
                  'flex flex-col items-center gap-1 rounded-lg px-3 py-1 text-[11px]',
                  isActive ? 'text-cyan-300' : 'text-slate-500',
                )
              }
            >
              <Icon className="h-4 w-4" />
              {label}
            </NavLink>
          ))}
        </nav>
      </main>
      <ToastViewport />
      <NotificationSettingsDialog open={notificationsOpen} onOpenChange={setNotificationsOpen} />
    </div>
  );
}
