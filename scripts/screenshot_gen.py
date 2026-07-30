#!/usr/bin/env python3
"""
T-Display-S3-Pro Screenshot Generator (222x480, Pillow-based).
Renders accurate mockups of all app screens as BMP files.

Usage: python3 scripts/screenshot_gen.py
Output: screenshots/*.bmp
"""

from PIL import Image, ImageDraw, ImageFont
import os, math

W, H = 222, 480
OUT = 'screenshots'

def rgb565(r, g, b):
    """Convert 8-bit RGB to 16-bit RGB565 style hex color."""
    r5 = (r >> 3) & 0x1F
    g6 = (g >> 2) & 0x3F
    b5 = (b >> 3) & 0x1F
    val = (r5 << 11) | (g6 << 5) | b5
    r8 = ((val >> 11) & 0x1F) * 255 // 31
    g8 = ((val >> 5) & 0x3F) * 255 // 63
    b8 = (val & 0x1F) * 255 // 31
    return (r8, g8, b8)

def hex_color(h):
    """Convert 0xRRGGBB hex to (R,G,B) tuple."""
    return ((h >> 16) & 0xFF, (h >> 8) & 0xFF, h & 0xFF)

def new_image(bg=0xFFFFFF):
    """Create a new 222x480 image with given background color."""
    return Image.new('RGB', (W, H), hex_color(bg))

def try_font(size, bold=False):
    """Try to load a font, falling back to default."""
    paths = [
        '/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf',
        '/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf',
        '/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf',
        '/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf',
    ]
    if bold:
        paths = [p.replace('Sans.ttf', 'Sans-Bold.ttf').replace('-Regular', '-Bold') for p in paths]
    for p in paths:
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()

def text_size(draw, text, font):
    """Get text size using textbbox."""
    bbox = draw.textbbox((0, 0), text, font=font)
    return bbox[2] - bbox[0], bbox[3] - bbox[1]

def draw_text_centered(draw, text, font, color, y):
    """Draw text horizontally centered at given y."""
    tw, th = text_size(draw, text, font)
    draw.text(((W - tw) // 2, y), text, fill=color, font=font)

def draw_rounded_rect(draw, xy, radius, fill):
    """Draw a rounded rectangle."""
    x1, y1, x2, y2 = xy
    draw.rounded_rectangle([x1, y1, x2, y2], radius=radius, fill=fill)

def draw_btn(draw, xy, text, font, bg, fg, radius=4):
    """Draw a button with centered text."""
    x1, y1, x2, y2 = xy
    draw_rounded_rect(draw, (x1, y1, x2, y2), radius, bg)
    tw, th = text_size(draw, text, font)
    draw.text((x1 + (x2-x1-tw)//2, y1 + (y2-y1-th)//2), text, fill=fg, font=font)

def save(img, name):
    path = os.path.join(OUT, name)
    img.save(path)
    print(f'  -> {path}')

# ────────────────────────────────────────────────────────────────────

def render_home():
    img = new_image(0xFFFFFF)
    draw = ImageDraw.Draw(img)
    font12 = try_font(10)
    font14 = try_font(12)

    # Status bar
    draw_rounded_rect(draw, (0, 0, W, 28), 0, hex_color(0xF0F0F0))
    draw.text((6, 6), '14:30', fill=hex_color(0x000000), font=font14)
    draw.text((W - 24, 6), 'WiFi', fill=hex_color(0x0000FF), font=font12)

    # App grid
    apps = [
        'Facetime', 'Mail', 'Music', 'Notes', 'Photos', 'Settings',
        'Books', 'Counter', 'Contacts', 'Camera', 'Clock', 'Stocks',
        'Files', 'Pacman', 'Tetris'
    ]
    cols, rows = 3, 5
    cw, ch = 66, 70
    sx, sy = 6, 36
    for r in range(rows):
        for c in range(cols):
            idx = r * cols + c
            if idx >= len(apps): break
            x = sx + c * cw
            y = sy + r * ch
            draw_btn(draw, (x, y, x + cw - 4, y + ch - 4),
                     apps[idx][:8], font12, hex_color(0xE8E8E8), hex_color(0x000000))

    save(img, '01_home.bmp')

def render_wifi():
    img = new_image(0x1a1a2e)
    draw = ImageDraw.Draw(img)
    font14 = try_font(11)
    font16 = try_font(13)

    # Header
    draw_rounded_rect(draw, (0, 0, W, 50), 0, hex_color(0x252550))
    draw_text_centered(draw, 'WiFi Details', font16, hex_color(0xFFFFFF), 16)

    # Details
    items = [
        ('SSID:', 'MyNetwork'),
        ('IP:', '192.168.1.100'),
        ('MAC:', 'AA:BB:CC:DD:EE:FF'),
        ('RSSI:', '-45 dBm'),
        ('Gateway:', '192.168.1.1'),
    ]
    for i, (label, value) in enumerate(items):
        y = 60 + i * 42
        draw_rounded_rect(draw, (6, y, W - 6, y + 36), 4, hex_color(0x252550))
        draw.text((12, y + 8), label, fill=hex_color(0x8888CC), font=font14)
        tw, _ = text_size(draw, value, font14)
        draw.text((W - 12 - tw, y + 8), value, fill=hex_color(0xFFFFFF), font=font14)

    # Refresh button
    draw_btn(draw, (61, 280, 161, 316), 'Refresh', font14,
             hex_color(0x444488), hex_color(0xFFFFFF))

    save(img, '02_wifi_details.bmp')

def render_clock():
    img = new_image(0x1a1a2e)
    draw = ImageDraw.Draw(img)
    font18 = try_font(18)
    font36 = try_font(34, bold=True)

    draw_text_centered(draw, '2026-07-30', font18, hex_color(0xAAAAFF), 40)
    draw_text_centered(draw, '14:30:45', font36, hex_color(0xFFFFFF), 190)
    draw_text_centered(draw, '22.5 C  Clear', font18, hex_color(0x88CC88), 270)

    save(img, '03_clock_weather.bmp')

def render_stocks():
    img = new_image(0x0d0d1a)
    draw = ImageDraw.Draw(img)
    font16 = try_font(14)
    font32 = try_font(30, bold=True)
    font20 = try_font(18)

    # Ticker name + arrows
    draw_text_centered(draw, 'Apple', font16, hex_color(0xFFFFFF), 10)
    draw.text((60, 6), '<', fill=hex_color(0x8888FF), font=font20)
    draw.text((W - 70, 6), '>', fill=hex_color(0x8888FF), font=font20)

    # Price + change
    draw_text_centered(draw, '$192.53', font32, hex_color(0xFFFFFF), 170)
    draw_text_centered(draw, '+3.21', font20, hex_color(0x00FF00), 220)
    draw_text_centered(draw, '1D', font16, hex_color(0x8888FF), 270)

    # Range buttons
    ranges = ['12H', '1D', '1W', '1M']
    for i, rng in enumerate(ranges):
        x = 30 + i * 48
        draw_btn(draw, (x, 430, x + 42, 458), rng, font16,
                 hex_color(0x333366), hex_color(0xFFFFFF))

    save(img, '04_stocks.bmp')

def render_music():
    img = new_image(0x1a0a1a)
    draw = ImageDraw.Draw(img)
    font16 = try_font(14)
    font12 = try_font(10)

    draw_text_centered(draw, 'Tone Player', font16, hex_color(0xFF88FF), 36)
    draw_text_centered(draw, 'No DAC on board\nPWM tones via vibrator', font12, hex_color(0x888888), 180)

    # Piano keys
    keys = ['C', 'D', 'E', 'F', 'G', 'A', 'B', 'C5', 'D5', 'E5', 'F5', 'G5', 'A5', 'C6']
    for i, key in enumerate(keys):
        x = i * 15
        draw_btn(draw, (x, 370, x + 13, 430), key, font12,
                 hex_color(0x444488), hex_color(0xFFFFFF), radius=2)

    # Stop button
    draw_btn(draw, (81, 330, 141, 360), 'STOP', font16,
             hex_color(0x884444), hex_color(0xFFFFFF))

    save(img, '05_music_tone_player.bmp')

def render_photos():
    img = new_image(0x000000)
    draw = ImageDraw.Draw(img)
    font14 = try_font(13)

    # Header
    draw.text((6, 4), 'File', fill=hex_color(0x888888), font=font14)

    # File list
    files = [
        ('D', 'photos/'),
        ('D', '2026-07-30/'),
        ('F', 'img1000.bmp'),
        ('F', 'img1001.bmp'),
        ('F', 'img1002.bmp'),
        ('F', 'img1003.bmp'),
    ]
    for i, (ftype, name) in enumerate(files):
        y = 40 + i * 30
        icon = '[DIR]' if ftype == 'D' else '[FILE]'
        draw.text((10, y), f'{icon} {name}', fill=hex_color(0xCCCCCC), font=font14)

    save(img, '06_photos_browser.bmp')

# ────────────────────────────────────────────────────────────────────

if __name__ == '__main__':
    os.makedirs(OUT, exist_ok=True)
    print(f'=== T-Display-S3-Pro Screenshot Generator ===')
    print(f'Output: {OUT}/ (222x480 BMP)\n')
    render_home()
    render_wifi()
    render_clock()
    render_stocks()
    render_music()
    render_photos()
    print(f'\nDone. 6 screenshots in {OUT}/')
