import { useMutation, useQuery, useQueryClient } from '@tanstack/react-query';
import { api } from './api';
import type { Preferences, Status } from './types';

export const queryKeys = {
  status: ['status'] as const,
  history: ['history'] as const,
  wpm: ['wpm'] as const,
  preferences: ['preferences'] as const,
};

export function useStatus() {
  return useQuery({
    queryKey: queryKeys.status,
    queryFn: api.getStatus,
    refetchInterval: 5000,
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
