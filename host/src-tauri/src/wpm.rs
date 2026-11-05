// SPDX-License-Identifier: GPL-2.0-or-later

use std::collections::VecDeque;

use serde::Serialize;

const WINDOW_SECONDS: u64 = 5 * 60;
const MAX_SAMPLES: usize = 300;

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct WpmSample {
    pub t: u64,
    pub wpm: u8,
}

#[derive(Debug, Clone, Serialize)]
#[serde(rename_all = "camelCase")]
pub struct WpmStats {
    pub current: u8,
    pub peak: u8,
    pub average5m: f32,
    pub samples: Vec<WpmSample>,
}

#[derive(Debug, Default)]
pub struct WpmTracker {
    current: u8,
    peak: u8,
    samples: VecDeque<WpmSample>,
}

impl WpmTracker {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn record(&mut self, wpm: u8, now: u64) {
        self.current = wpm;
        self.peak = self.peak.max(wpm);
        self.samples.push_back(WpmSample { t: now, wpm });
        while self
            .samples
            .front()
            .map(|sample| sample.t + WINDOW_SECONDS < now)
            .unwrap_or(false)
        {
            self.samples.pop_front();
        }
        while self.samples.len() > MAX_SAMPLES {
            self.samples.pop_front();
        }
    }

    pub fn mark_offline(&mut self) {
        self.current = 0;
    }

    pub fn snapshot(&self) -> WpmStats {
        let active: Vec<u8> = self
            .samples
            .iter()
            .map(|sample| sample.wpm)
            .filter(|wpm| *wpm > 0)
            .collect();
        let average5m = if active.is_empty() {
            0.0
        } else {
            let sum: u32 = active.iter().map(|wpm| *wpm as u32).sum();
            (sum as f32 / active.len() as f32 * 10.0).round() / 10.0
        };

        WpmStats {
            current: self.current,
            peak: self.peak,
            average5m,
            samples: self.samples.iter().cloned().collect(),
        }
    }

    pub fn reset(&mut self) -> WpmStats {
        self.current = 0;
        self.peak = 0;
        self.samples.clear();
        self.snapshot()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn tracks_current_peak_and_average() {
        let mut tracker = WpmTracker::new();
        tracker.record(60, 1_000);
        tracker.record(80, 1_001);
        tracker.record(0, 1_002);

        let stats = tracker.snapshot();
        assert_eq!(stats.current, 0);
        assert_eq!(stats.peak, 80);
        assert_eq!(stats.average5m, 70.0);
        assert_eq!(stats.samples.len(), 3);
    }

    #[test]
    fn resets_all_stats() {
        let mut tracker = WpmTracker::new();
        tracker.record(80, 1_000);
        let stats = tracker.reset();
        assert_eq!(stats.current, 0);
        assert_eq!(stats.peak, 0);
        assert_eq!(stats.average5m, 0.0);
        assert!(stats.samples.is_empty());
    }

    #[test]
    fn prunes_samples_outside_window() {
        let mut tracker = WpmTracker::new();
        tracker.record(50, 1_000);
        tracker.record(60, 1_000 + WINDOW_SECONDS + 1);
        assert_eq!(tracker.snapshot().samples.len(), 1);
    }
}
