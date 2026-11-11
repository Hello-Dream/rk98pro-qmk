import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { api } from './api';
import type { Preferences, Status } from './types';

export const queryKeys = {
  status: ['status'] as const,
  monitorError: ['monitor-error'] as const,
  history: ['history'] as const,
  wpm: ['wpm'] as const,
  preferences: ['preferences'] as const,
};

export function useStatus() {
  return useQuery({
    queryKey: queryKeys.status,
    queryFn: api.getStatus,
    // 主路径是 status-updated 事件；30 秒只是事件丢失/重连后的兜底。
    refetchInterval: 30000,
  });
}

export function useMonitorError() {
  return useQuery({
    queryKey: queryKeys.monitorError,
    queryFn: api.getMonitorError,
    staleTime: Infinity,
  });
}

export function useHistory() {
  return useQuery({
    queryKey: queryKeys.history,
    queryFn: api.getHistory,
    refetchInterval: 60000,
  });
}

export function useWpmStats() {
  return useQuery({
    queryKey: queryKeys.wpm,
    queryFn: api.getWpmStats,
    refetchInterval: 2000,
  });
}

export function usePreferences() {
  return useQuery({
    queryKey: queryKeys.preferences,
    queryFn: api.getPreferences,
  });
}

export function useSavePreferences() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: (preferences: Preferences) => api.setPreferences(preferences),
    onSuccess: (preferences) => {
      queryClient.setQueryData(queryKeys.preferences, preferences);
    },
  });
}

export function useResetWpmStats() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: api.resetWpmStats,
    onSuccess: (stats) => {
      queryClient.setQueryData(queryKeys.wpm, stats);
    },
  });
}

export function useRefresh() {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: api.refresh,
    onSuccess: () => {
      void queryClient.invalidateQueries();
    },
  });
}

export function useStatusSnapshot(): Status | null {
  const { data } = useStatus();
  return data ?? null;
}
