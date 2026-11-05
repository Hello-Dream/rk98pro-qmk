import { fireEvent, render, screen } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import NotificationSettingsDialog from './NotificationSettingsDialog';
import { api } from '../../lib/api';

const mocks = vi.hoisted(() => ({
  preferences: {
    notifyLowBattery: true,
    notifyFull: true,
    notifyDisconnect: false,
  },
  saveMutate: vi.fn(),
}));

vi.mock('../../lib/queries', () => ({
  usePreferences: () => ({ data: mocks.preferences, isLoading: false }),
  useSavePreferences: () => ({ mutate: mocks.saveMutate, isPending: false }),
}));

vi.mock('../../lib/api', () => ({
  api: {
    testNotification: vi.fn(async () => undefined),
  },
}));

describe('通知设置对话框', () => {
  beforeEach(() => {
    mocks.saveMutate.mockClear();
    vi.mocked(api.testNotification).mockClear();
  });

  it('显示三个独立开关', () => {
    render(<NotificationSettingsDialog open onOpenChange={() => undefined} />);
    expect(screen.getByText('低电提醒')).toBeInTheDocument();
    expect(screen.getByText('充满提醒')).toBeInTheDocument();
    expect(screen.getByText('断连提醒')).toBeInTheDocument();
  });

  it('切换低电提醒只保存本机偏好', () => {
    render(<NotificationSettingsDialog open onOpenChange={() => undefined} />);
    const checkbox = screen.getByText('低电提醒').closest('label')?.querySelector('input');
    expect(checkbox).not.toBeNull();
    fireEvent.click(checkbox as HTMLInputElement);

    expect(mocks.saveMutate).toHaveBeenCalledWith(
      { ...mocks.preferences, notifyLowBattery: false },
      expect.any(Object),
    );
  });

  it('测试通知调用本机通知命令', () => {
    render(<NotificationSettingsDialog open onOpenChange={() => undefined} />);
    fireEvent.click(screen.getByRole('button', { name: /测试通知/ }));
    expect(api.testNotification).toHaveBeenCalledTimes(1);
  });
});
