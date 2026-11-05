import { render, screen } from '@testing-library/react';
import { MemoryRouter } from 'react-router-dom';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import OverviewPage from './OverviewPage';

const mocks = vi.hoisted(() => {
  const historyData = {
    samples: [],
    events: [
      {
        t: 1_700_000_000,
        kind: 'low_battery',
        title: 'RK98PRO 电量低',
        message: '电量已降至 15%',
      },
    ],
  };
  const statusData = {
    seq: 1,
    version: 3,
    firmwareVersion: '3.0.0',
    settingsVersion: 5,
    battery: 82,
    charging: 0,
    link: 2,
    mode: 6,
    powerState: 0,
    flags: 1,
    locks: 1,
    layer: 2,
    defaultLayer: 0,
    wpm: 64,
    idleSeconds: 3,
    rgbrecChannel: null,
    rgbMode: 0,
    rgbBrightness: 90,
    wirelessVersion: 1,
    online: true,
    updatedAt: 1_700_000_000,
  };
  return { historyData, statusData };
});

vi.mock('../../lib/queries', () => ({
  useStatus: () => ({ data: mocks.statusData }),
  useHistory: () => ({ data: mocks.historyData }),
}));

vi.mock('../wpm/WpmCard', () => ({
  default: () => <div>WPM 统计卡片</div>,
}));

describe('键盘状态仪表盘', () => {
  beforeEach(() => {
    mocks.statusData = {
      seq: 1,
      version: 3,
      firmwareVersion: '3.0.0',
      settingsVersion: 5,
      battery: 82,
      charging: 0,
      link: 2,
      mode: 6,
      powerState: 0,
      flags: 1,
      locks: 1,
      layer: 2,
      defaultLayer: 0,
      wpm: 64,
      idleSeconds: 3,
      rgbrecChannel: null,
      rgbMode: 0,
      rgbBrightness: 90,
      wirelessVersion: 1,
      online: true,
      updatedAt: 1_700_000_000,
    };
  });

  it('显示电量、连接、层、锁定键和 WPM 卡片', () => {
    render(
      <MemoryRouter>
        <OverviewPage />
      </MemoryRouter>,
    );

    expect(screen.getByText('82')).toBeInTheDocument();
    expect(screen.getAllByText(/2.4G/).length).toBeGreaterThan(0);
    expect(screen.getAllByText(/已连接/).length).toBeGreaterThan(0);
    expect(screen.getByText('2 / 0')).toBeInTheDocument();
    expect(screen.getByText('Caps')).toBeInTheDocument();
    expect(screen.getByText('WPM 统计卡片')).toBeInTheDocument();
    expect(screen.getByText('RK98PRO 电量低')).toBeInTheDocument();
  });

  it('离线时显示连接提示', () => {
    mocks.statusData = null as never;
    render(
      <MemoryRouter>
        <OverviewPage />
      </MemoryRouter>,
    );

    expect(screen.getAllByText(/等待键盘连接/).length).toBeGreaterThan(0);
    expect(screen.getByText(/蓝牙模式不提供 raw HID/)).toBeInTheDocument();
  });
});
