import { useEffect } from 'react';
import { listen } from '@tauri-apps/api/event';
import { useQueryClient } from '@tanstack/react-query';
import { queryKeys } from './queries';
import type { Status } from './types';

/** 只监听只读状态事件；事件缺失时由 React Query 的定时读取兜底。 */
export function useDeviceEvents() {
  const queryClient = useQueryClient();

  useEffect(() => {
    const unlisten = listen<Status | null>('status-updated', (event) => {
      queryClient.setQueryData(queryKeys.status, event.payload);
      void queryClient.invalidateQueries({ queryKey: queryKeys.history });
      void queryClient.invalidateQueries({ queryKey: queryKeys.wpm });
    });

    return () => {
      void unlisten.then((dispose) => dispose());
    };
  }, [queryClient]);
}
