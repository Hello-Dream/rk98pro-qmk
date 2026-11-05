// SPDX-License-Identifier: GPL-2.0-or-later

//! 托盘电池图标绘制。

use tauri::image::Image;

use crate::status::{Status, CHARGING_CHARGING, CHARGING_FULL};

const SIZE: usize = 32;

const BOLT: [&str; 11] = [
    "..##..",
    ".###..",
    "####..",
    "..##..",
    ".#####",
    "...##.",
    "..##..",
    ".###..",
    ".##...",
    ".#....",
    "#.....",
];

fn put(buffer: &mut [u8], x: i32, y: i32, color: [u8; 4]) {
    if x < 0 || y < 0 || x >= SIZE as i32 || y >= SIZE as i32 {
        return;
    }
    let index = ((y as usize * SIZE) + x as usize) * 4;
    buffer[index..index + 4].copy_from_slice(&color);
}

fn fill_rect(buffer: &mut [u8], left: i32, top: i32, right: i32, bottom: i32, color: [u8; 4]) {
    for y in top..=bottom {
        for x in left..=right {
            put(buffer, x, y, color);
        }
    }
}

fn fill_circle(buffer: &mut [u8], center_x: i32, center_y: i32, radius: i32, color: [u8; 4]) {
    for y in (center_y - radius)..=(center_y + radius) {
        for x in (center_x - radius)..=(center_x + radius) {
            let dx = x - center_x;
            let dy = y - center_y;
            if dx * dx + dy * dy <= radius * radius {
                put(buffer, x, y, color);
            }
        }
    }
}

fn draw_line(buffer: &mut [u8], mut x0: i32, mut y0: i32, x1: i32, y1: i32, color: [u8; 4]) {
    let dx = (x1 - x0).abs();
    let sx = if x0 < x1 { 1 } else { -1 };
    let dy = -(y1 - y0).abs();
    let sy = if y0 < y1 { 1 } else { -1 };
    let mut error = dx + dy;

    loop {
        put(buffer, x0, y0, color);
        if x0 == x1 && y0 == y1 {
            break;
        }
        let doubled = error * 2;
        if doubled >= dy {
            error += dy;
            x0 += sx;
        }
        if doubled <= dx {
            error += dx;
            y0 += sy;
        }
    }
}

fn level_color(status: Option<&Status>) -> [u8; 4] {
    let Some(status) = status else {
        return [135, 135, 135, 255];
    };
    if !status.online {
        return [135, 135, 135, 255];
    }
    match status.battery {
        Some(battery) if battery <= 15 => [231, 76, 60, 255],
        Some(battery) if battery <= 40 => [241, 196, 15, 255],
        _ => [46, 204, 113, 255],
    }
}

pub fn battery_icon(status: Option<&Status>) -> Image<'static> {
    let mut pixels = vec![0u8; SIZE * SIZE * 4];
    let outline = [236, 236, 236, 255];
    let dark = [23, 23, 23, 245];

    fill_circle(&mut pixels, 15, 15, 15, dark);
    fill_rect(&mut pixels, 5, 9, 24, 22, outline);
    fill_rect(&mut pixels, 25, 13, 27, 18, outline);
    fill_rect(&mut pixels, 7, 11, 22, 20, [40, 40, 40, 255]);

    let level = status
        .and_then(|value| value.battery)
        .unwrap_or(0)
        .min(100) as i32;
    let fill_width = 15 * level / 100;
    if fill_width > 0 {
        fill_rect(&mut pixels, 8, 12, 8 + fill_width, 19, level_color(status));
    }

    if status
        .map(|value| value.charging == CHARGING_CHARGING || value.charging == CHARGING_FULL)
        .unwrap_or(false)
    {
        for (row, line) in BOLT.iter().enumerate() {
            for (column, cell) in line.chars().enumerate() {
                if cell == '#' {
                    put(
                        &mut pixels,
                        12 + column as i32,
                        8 + row as i32,
                        [255, 255, 255, 255],
                    );
                }
            }
        }
    }

    if status.map(|value| !value.online).unwrap_or(false) {
        draw_line(&mut pixels, 8, 23, 23, 8, [231, 76, 60, 255]);
    }

    Image::new_owned(pixels, SIZE as u32, SIZE as u32)
}
