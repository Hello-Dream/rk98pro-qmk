import { beforeEach, describe, expect, it, vi } from 'vitest';
import { invoke } from '@tauri-apps/api/core';
import { api } from './api';

vi.mock('@tauri-apps/api/core', () => ({
  invoke: vi.fn(async () => undefined),
}));

describe('Tauri API 包装', () => {
  beforeEach(() => {
    vi.mocked(invoke).mockClear();
  });

  it('只暴露只读状态和本机操作', () => {
    expect(Object.keys(api).sort()).toEqual([
      'getHistory',
      'getPreferences',
      'getStatus',
      'getWpmStats',
      'refresh',
      'resetWpmStats',
      'setPreferences',
      'testNotification',
    ]);
  });

  it('读取状态快照', async () => {
    await api.getStatus();
    expect(invoke).toHaveBeenCalledWith('get_status');
  });

  it('读取 WPM 统计', async () => {
    await api.getWpmStats();
    expect(invoke).toHaveBeenCalledWith('get_wpm_stats');
  });

  it('重置 WPM 统计只调用本机命令', async () => {
    await api.resetWpmStats();
    expect(invoke).toHaveBeenCalledWith('reset_wpm_stats');
  });

  it('用 camelCase 参数保存通知偏好', async () => {
    const preferences = {
      notifyLowBattery: false,
      notifyFull: true,
      notifyDisconnect: true,
    };
    await api.setPreferences(preferences);
    expect(invoke).toHaveBeenCalledWith('set_preferences', { preferences });
  });
});
