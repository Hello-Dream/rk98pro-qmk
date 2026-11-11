import { render, screen } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import MonitorErrorBanner from './MonitorErrorBanner';
import type { MonitorError } from '../lib/types';

const mocks = vi.hoisted(() => ({
  error: null as MonitorError | null,
}));

vi.mock('../lib/queries', () => ({
  useMonitorError: () => ({ data: mocks.error }),
}));

describe('键盘诊断提示', () => {
  beforeEach(() => {
    mocks.error = null;
  });

  it('旧固件提示更新固件', () => {
    mocks.error = { kind: 'unsupported', message: '快照版本 3 不受支持' };
    render(<MonitorErrorBanner />);
    expect(screen.getByText('固件不支持只读状态通道')).toBeInTheDocument();
    expect(screen.getByText(/更新固件/)).toBeInTheDocument();
  });

  it('权限失败提示 udev 与重新插拔', () => {
    mocks.error = { kind: 'access_denied', message: 'Permission denied' };
    render(<MonitorErrorBanner />);
    expect(screen.getByText('无法访问键盘 HID 接口')).toBeInTheDocument();
    expect(screen.getByText(/udev/)).toBeInTheDocument();
  });

  it('未找到设备提示 USB 或 2.4G', () => {
    mocks.error = { kind: 'not_found', message: '没有接口' };
    render(<MonitorErrorBanner />);
    expect(screen.getByText('未找到键盘')).toBeInTheDocument();
    expect(screen.getByText(/2\.4G/)).toBeInTheDocument();
  });

  it('没有错误时不渲染', () => {
    const { container } = render(<MonitorErrorBanner />);
    expect(container).toBeEmptyDOMElement();
  });
});
