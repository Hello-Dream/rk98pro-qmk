import { render, screen } from '@testing-library/react';
import userEvent from '@testing-library/user-event';
import { describe, expect, it, vi } from 'vitest';
import { Button } from './button';

describe('Button', () => {
  it('renders and handles keyboard activation', async () => {
    const onClick = vi.fn();
    render(<Button onClick={onClick}>保存</Button>);
    const button = screen.getByRole('button', { name: '保存' });
    button.focus();
    await userEvent.keyboard('{Enter}');
    expect(onClick).toHaveBeenCalledOnce();
  });
});
