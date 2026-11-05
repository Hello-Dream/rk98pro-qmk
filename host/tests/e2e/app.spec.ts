import { expect, test } from '@playwright/test';

test('renders the shell and navigates between status and battery', async ({ page }) => {
  await page.goto('/');
  await expect(page.getByText('RK98PRO', { exact: true }).first()).toBeVisible();
  await expect(page.getByRole('heading', { name: '键盘状态' })).toBeVisible();

  await page.getByRole('link', { name: '电量' }).first().click();
  await expect(page.getByRole('heading', { name: '电量历史' })).toBeVisible();
});
