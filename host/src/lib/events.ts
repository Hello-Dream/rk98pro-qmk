import { useEffect } from 'react';
import { listen } from '@tauri-apps/api/event';
import { useQueryClient } from '@tanstack/react-query';
import { queryKeys } from './queries';
import type { MonitorError, Status } from './types';

/** 只监听只读状态/诊断事件；事件缺失时由 React Query 的定时读取兜底。 */
export function useDeviceEvents() {
  const queryClient = useQueryClient();

  useEffect(() => {
    const statusListener = listen<Status | null>('status-updated', (event) => {
      queryClient.setQueryData(queryKeys.status, event.payload);
    });
    const errorListener = listen<MonitorError | null>('monitor-error', (event) => {
      queryClient.setQueryData(queryKeys.monitorError, event.payload);
    });
    const historyListener = listen('history-updated', () => {
      void queryClient.invalidateQueries({ queryKey: queryKeys.history });
    });

    return () => {
      void statusListener.then((dispose) => dispose());
      void errorListener.then((dispose) => dispose());
      void historyListener.then((dispose) => dispose());
    };
  }, [queryClient]);
}
