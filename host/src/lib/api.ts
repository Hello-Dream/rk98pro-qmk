import { invoke } from '@tauri-apps/api/core';
import type { HistorySnapshot, Preferences, Status, WpmStats } from './types';

/**
 * 唯一 Tauri invoke 入口。
 * 这里只允许读取键盘状态和本机偏好；不存在任何写键盘命令。
 */
export const api = {
  getStatus: () => invoke<Status | null>('get_status'),
  refresh: () => invoke<void>('refresh'),
  getHistory: () => invoke<HistorySnapshot>('get_history'),
  getWpmStats: () => invoke<WpmStats>('get_wpm_stats'),
  resetWpmStats: () => invoke<WpmStats>('reset_wpm_stats'),
  getPreferences: () => invoke<Preferences>('get_preferences'),
  setPreferences: (preferences: Preferences) =>
    invoke<Preferences>('set_preferences', { preferences }),
  testNotification: () => invoke<void>('test_notification'),
};
