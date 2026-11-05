import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';
import BatteryPage from './BatteryPage';

const mocks = vi.hoisted(() => ({
  history: {
    samples: [
      { t: 0, battery: 80, charging: 0, link: 2, mode: 6, online: true },
      { t: 60, battery: null, charging: 0, link: 255, mode: 255, online: false },
      { t: 120, battery: 79, charging: 0, link: 2, mode: 6, online: true },
    ],
    events: [],
  },
  status: {
    seq: 1,
    version: 3,
    firmwareVersion: '3.0.0',
    settingsVersion: 5,
    battery: 79,
    charging: 0,
    link: 2,
    mode: 6,
    powerState: 0,
    flags: 1,
    locks: 0,
    layer: 0,
    defaultLayer: 0,
    wpm: 0,
    idleSeconds: 0,
    rgbrecChannel: null,
    rgbMode: 0,
    rgbBrightness: 90,
    wirelessVersion: 1,
    online: true,
    updatedAt: 1_700_000_000,
  },
}));

vi.mock('../../lib/queries', () => ({
  useHistory: () => ({ data: mocks.history }),
  useStatus: () => ({ data: mocks.status }),
}));

describe('电量历史页', () => {
  it('显示范围切换、统计和离线断线', () => {
    render(<BatteryPage />);

    expect(screen.getByText('电量历史')).toBeInTheDocument();
    expect(screen.getAllByText('79%').length).toBeGreaterThan(0);
    expect(screen.getAllByText('2').length).toBeGreaterThan(0);
    expect(document.querySelectorAll('circle').length).toBe(2);

    fireEvent.click(screen.getByRole('button', { name: '6 小时' }));
    fireEvent.click(screen.getByRole('button', { name: '7 天' }));
    expect(screen.getByRole('button', { name: '7 天' })).toHaveClass('bg-cyan-400/15');
  });
});
