import { AlertTriangle } from 'lucide-react';
import { useMonitorError } from '../lib/queries';

const TITLES: Record<string, string> = {
  unsupported: '固件不支持只读状态通道',
  access_denied: '无法访问键盘 HID 接口',
  not_found: '未找到键盘',
  hid: 'HID 访问异常',
};

const HINTS: Record<string, string> = {
  unsupported: '请更新固件到与上位机匹配的版本后再连接。',
  access_denied: '请安装或检查 udev 规则，重新加载规则并重新插拔设备。',
  not_found: '请使用 USB 有线或 2.4G 接收器；蓝牙模式不提供 raw HID。',
  hid: '请重新插拔键盘或接收器，确认设备未被其他程序占用。',
};

export default function MonitorErrorBanner() {
  const error = useMonitorError().data;
  if (!error) {
    return null;
  }

  return (
    <div
      role="alert"
      className="mb-4 flex gap-3 rounded-xl border border-amber-500/30 bg-amber-500/10 p-4 text-sm text-amber-100"
    >
      <AlertTriangle className="mt-0.5 h-5 w-5 shrink-0 text-amber-300" />
      <div className="min-w-0">
        <strong className="block">{TITLES[error.kind] ?? '键盘连接异常'}</strong>
        <p className="mt-1 text-amber-200/90">{HINTS[error.kind] ?? error.message}</p>
        <p className="mt-1 break-words text-xs text-amber-200/60">{error.message}</p>
      </div>
    </div>
  );
}
