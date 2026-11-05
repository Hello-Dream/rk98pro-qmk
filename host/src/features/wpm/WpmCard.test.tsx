import { fireEvent, render, screen } from '@testing-library/react';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import WpmCard from './WpmCard';

const mocks = vi.hoisted(() => ({
  stats: {
    current: 64,
    peak: 92,
    average5m: 58.4,
    samples: [
      { t: 1, wpm: 50 },
      { t: 2, wpm: 64 },
      { t: 3, wpm: 58 },
    ],
  },
  resetMutate: vi.fn(),
}));

vi.mock('../../lib/queries', () => ({
  useWpmStats: () => ({ data: mocks.stats }),
  useResetWpmStats: () => ({ mutate: mocks.resetMutate, isPending: false }),
}));

describe('WPM 统计卡片', () => {
  beforeEach(() => {
    mocks.resetMutate.mockClear();
  });

  it('显示当前、峰值和 5 分钟平均', () => {
    render(<WpmCard />);
    expect(screen.getByText('64')).toBeInTheDocument();
    expect(screen.getByText('92')).toBeInTheDocument();
    expect(screen.getByText('58')).toBeInTheDocument();
  });

  it('没有采样时显示空状态', () => {
    const original = mocks.stats;
    mocks.stats = { current: 0, peak: 0, average5m: 0, samples: [] };
    render(<WpmCard />);
    expect(screen.getByText('还没有打字数据')).toBeInTheDocument();
    mocks.stats = original;
  });

  it('点击重置只调用本机重置命令', () => {
    render(<WpmCard />);
    fireEvent.click(screen.getByRole('button', { name: /重置统计/ }));
    expect(mocks.resetMutate).toHaveBeenCalledTimes(1);
    expect(mocks.resetMutate.mock.calls[0][0]).toBeUndefined();
  });
});
