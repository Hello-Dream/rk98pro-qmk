import { useUiStore } from '../../store/ui';
import { cn } from './utils';

export function ToastViewport() {
  const toasts = useUiStore((state) => state.toasts);
  const removeToast = useUiStore((state) => state.removeToast);

  return (
    <div className="pointer-events-none fixed bottom-5 right-5 z-[60] flex w-[min(92vw,360px)] flex-col gap-2">
      {toasts.map((toast) => (
        <button
          key={toast.id}
          type="button"
          onClick={() => removeToast(toast.id)}
          className={cn(
            'pointer-events-auto rounded-xl border px-4 py-3 text-left text-sm shadow-xl',
            toast.tone === 'error'
              ? 'border-rose-500/40 bg-rose-950/95 text-rose-100'
              : toast.tone === 'success'
                ? 'border-emerald-500/40 bg-emerald-950/95 text-emerald-100'
                : 'border-slate-700 bg-slate-900/95 text-slate-200',
          )}
        >
          {toast.message}
        </button>
      ))}
    </div>
  );
}
