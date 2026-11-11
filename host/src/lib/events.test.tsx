import { act, renderHook, waitFor } from '@testing-library/react';
import { QueryClient, QueryClientProvider } from '@tanstack/react-query';
import type { ReactNode } from 'react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import { useDeviceEvents } from './events';
import { queryKeys } from './queries';
import type { Status } from './types';

const mocks = vi.hoisted(() => ({
  listeners: new Map<string, (event: { payload: unknown }) => void>(),
  unlistenStatus: vi.fn(),
  unlistenError: vi.fn(),
  unlistenHistory: vi.fn(),
}));

vi.mock('@tauri-apps/api/event', () => ({
  listen: vi.fn((name: string, handler: (event: { payload: unknown }) => void) => {
    mocks.listeners.set(name, handler);
    if (name === 'status-updated') {
      return Promise.resolve(mocks.unlistenStatus);
    }
    if (name === 'monitor-error') {
      return Promise.resolve(mocks.unlistenError);
    }
    return Promise.resolve(mocks.unlistenHistory);
  }),
}));

/** 构造一份完整的只读状态快照。 */
function status(): Status {
  return {
    seq: 1,
    version: 1,
    firmwareVersion: '1.0.1',
    settingsVersion: 1,
    battery: 80,
    charging: 0,
    link: 2,
    mode: 6,
    powerState: 0,
    flags: 1,
    locks: 0,
    layer: 0,
    defaultLayer: 0,
    wpm: 64,
    idleSeconds: 0,
    rgbrecChannel: null,
    rgbMode: 0,
    rgbBrightness: 90,
    wirelessVersion: 1,
    online: true,
    updatedAt: 1_700_000_000,
  };
}

/** 创建带 QueryClient 的 Hook 包装器。 */
function createWrapper() {
  const queryClient = new QueryClient({
    defaultOptions: { queries: { retry: false } },
  });
  const wrapper = ({ children }: { children: ReactNode }) => (
    <QueryClientProvider client={queryClient}>{children}</QueryClientProvider>
  );
  return { queryClient, wrapper };
}

/** 挂载事件 Hook，并等待三个监听器注册完成。 */
async function renderEvents() {
  const { queryClient, wrapper } = createWrapper();
  const hook = renderHook(() => useDeviceEvents(), { wrapper });
  await waitFor(() => expect(mocks.listeners.size).toBe(3));
  return { ...hook, queryClient };
}

describe('设备事件', () => {
  beforeEach(() => {
    mocks.listeners.clear();
    mocks.unlistenStatus.mockClear();
    mocks.unlistenError.mockClear();
    mocks.unlistenHistory.mockClear();
  });

  it('状态事件只更新状态缓存，不失效历史或 WPM', async () => {
    const { queryClient, unmount } = await renderEvents();
    const setQueryData = vi.spyOn(queryClient, 'setQueryData');
    const invalidateQueries = vi.spyOn(queryClient, 'invalidateQueries');
    const payload = status();

    act(() => {
      mocks.listeners.get('status-updated')!({ payload });
    });

    expect(setQueryData).toHaveBeenCalledWith(queryKeys.status, payload);
    expect(invalidateQueries).not.toHaveBeenCalled();
    unmount();
  });

  it('历史事件只失效历史查询，不更新状态', async () => {
    const { queryClient, unmount } = await renderEvents();
    const setQueryData = vi.spyOn(queryClient, 'setQueryData');
    const invalidateQueries = vi.spyOn(queryClient, 'invalidateQueries');

    act(() => {
      mocks.listeners.get('history-updated')!({ payload: null });
    });

    expect(invalidateQueries).toHaveBeenCalledWith({ queryKey: queryKeys.history });
    expect(setQueryData).not.toHaveBeenCalled();
    unmount();
  });

  it('诊断事件更新错误缓存', async () => {
    const { queryClient, unmount } = await renderEvents();
    const setQueryData = vi.spyOn(queryClient, 'setQueryData');
    const payload = { kind: 'access_denied', message: 'Permission denied' };

    act(() => {
      mocks.listeners.get('monitor-error')!({ payload });
    });

    expect(setQueryData).toHaveBeenCalledWith(queryKeys.monitorError, payload);
    unmount();
  });

  it('卸载时释放三个监听器', async () => {
    const { unmount } = await renderEvents();

    unmount();

    await waitFor(() => expect(mocks.unlistenStatus).toHaveBeenCalledTimes(1));
    await waitFor(() => expect(mocks.unlistenError).toHaveBeenCalledTimes(1));
    await waitFor(() => expect(mocks.unlistenHistory).toHaveBeenCalledTimes(1));
  });
});
