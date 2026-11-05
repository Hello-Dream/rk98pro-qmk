import { lazy, Suspense, type ReactNode } from 'react';
import { createHashRouter } from 'react-router-dom';
import AppShell from './components/AppShell';

const Overview = lazy(() => import('./features/overview/OverviewPage'));
const Battery = lazy(() => import('./features/battery/BatteryPage'));

function page(element: ReactNode) {
  return <Suspense fallback={<div className="text-sm text-slate-400">加载中…</div>}>{element}</Suspense>;
}

export const router = createHashRouter([
  {
    path: '/',
    element: <AppShell />,
    children: [
      { index: true, element: page(<Overview />) },
      { path: 'battery', element: page(<Battery />) },
    ],
  },
]);
