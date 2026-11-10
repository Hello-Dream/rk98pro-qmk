import { useEffect } from 'react';
import { listen } from '@tauri-apps/api/event';
import { useQueryClient } from '@tanstack/react-query';
import { queryKeys } from './queries';
import type { Status } from './types';

/** 只监听只读状态事件；历史变化由后端单独通知，事件缺失时由 React Query 的定时读取兜底。 */
export function useDeviceEvents() {
  const queryClient = useQueryClient();

  useEffect(() => {
    const statusListener = listen<Status | null>('status-updated', (event) => {
      queryClient.setQueryData(queryKeys.status, event.payload);
    });
    const historyListener = listen('history-updated', () => {
      void queryClient.invalidateQueries({ queryKey: queryKeys.history });
    });

    return () => {
      void statusListener.then((dispose) => dispose());
      void historyListener.then((dispose) => dispose());
    };
  }, [queryClient]);
}
