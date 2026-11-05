import { Bell, Send } from 'lucide-react';
import { Dialog, DialogContent } from '../../components/ui/dialog';
import { Button } from '../../components/ui/button';
import { api } from '../../lib/api';
import { usePreferences, useSavePreferences } from '../../lib/queries';
import { useUiStore } from '../../store/ui';
import type { Preferences } from '../../lib/types';

const FIELDS: Array<{ key: keyof Preferences; label: string; hint: string }> = [
  { key: 'notifyLowBattery', label: '低电提醒', hint: '电量跨过 20% / 15% / 10% / 5% 时通知' },
  { key: 'notifyFull', label: '充满提醒', hint: '充电完成时通知一次' },
  { key: 'notifyDisconnect', label: '断连提醒', hint: '离线 60 秒后通知，恢复连接时也会提示' },
];

export default function NotificationSettingsDialog({
  open,
  onOpenChange,
}: {
  open: boolean;
  onOpenChange: (open: boolean) => void;
}) {
  const preferences = usePreferences();
  const save = useSavePreferences();
  const pushToast = useUiStore((state) => state.pushToast);
  const current = preferences.data;

  const toggle = (key: keyof Preferences) => {
    if (!current) {
      return;
    }
    save.mutate(
      { ...current, [key]: !current[key] },
      {
        onSuccess: () => pushToast('通知设置已保存', 'success'),
        onError: (error) => pushToast(String(error), 'error'),
      },
    );
  };

  const test = () => {
    void api
      .testNotification()
      .then(() => pushToast('测试通知已发送', 'success'))
      .catch((error) => pushToast(String(error), 'error'));
  };

  return (
    <Dialog open={open} onOpenChange={onOpenChange}>
      <DialogContent
        title="通知设置"
        description="只保存在本机，不会写入键盘。"
      >
        <div className="space-y-4">
          {preferences.isLoading || !current ? (
            <p className="text-sm text-slate-400">读取通知设置…</p>
          ) : (
            FIELDS.map((field) => (
              <label
                key={field.key}
                className="flex cursor-pointer items-center justify-between gap-4 rounded-xl border border-slate-800 bg-slate-950/40 p-3"
              >
                <span>
                  <strong className="block text-sm text-slate-200">{field.label}</strong>
                  <span className="text-xs text-slate-500">{field.hint}</span>
                </span>
                <input
                  type="checkbox"
                  className="h-5 w-5 accent-cyan-400"
                  checked={current[field.key]}
                  disabled={save.isPending}
                  onChange={() => toggle(field.key)}
                />
              </label>
            ))
          )}

          <div className="flex items-center justify-between gap-3 border-t border-slate-800 pt-4">
            <p className="text-xs text-slate-500">
              <Bell className="mr-1 inline h-3.5 w-3.5" />
              通知由本机发送，键盘离线时仍然有效。
            </p>
            <Button size="sm" onClick={test}>
              <Send className="h-3.5 w-3.5" />
              测试通知
            </Button>
          </div>
        </div>
      </DialogContent>
    </Dialog>
  );
}
