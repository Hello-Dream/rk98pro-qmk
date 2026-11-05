import { create } from 'zustand';

export type ToastTone = 'info' | 'success' | 'error';

interface Toast {
  id: number;
  message: string;
  tone: ToastTone;
}

interface UiState {
  toasts: Toast[];
  pushToast: (message: string, tone?: ToastTone) => void;
  removeToast: (id: number) => void;
}

let nextToast = 1;

export const useUiStore = create<UiState>((set) => ({
  toasts: [],
  pushToast: (message, tone = 'info') => {
    const id = nextToast++;
    set((state) => ({ toasts: [...state.toasts, { id, message, tone }] }));
    window.setTimeout(() => {
      set((state) => ({ toasts: state.toasts.filter((toast) => toast.id !== id) }));
    }, 3600);
  },
  removeToast: (id) =>
    set((state) => ({ toasts: state.toasts.filter((toast) => toast.id !== id) })),
}));
