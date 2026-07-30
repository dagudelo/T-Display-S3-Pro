#!/usr/bin/env python3
"""
T-Display-S3-Pro Screenshot Generator (222x480, Pillow-based).
Imports real LVGL C-array icon assets and renders ALL app screens.

Usage: python3 scripts/screenshot_gen.py
Output: screenshots/*.bmp
"""

from PIL import Image, ImageDraw, ImageFont
import os, re, sys, math

W, H = 222, 480
SRC = os.path.join(os.path.dirname(__file__), '..', 'examples', 'factory', 'src')
OUT = 'screenshots'

# ─── Helpers ───────────────────────────────────────────────────────

def hex_color(h):
    return ((h >> 16) & 0xFF, (h >> 8) & 0xFF, h & 0xFF)

def new_image(bg=0xFFFFFF):
    return Image.new('RGB', (W, H), hex_color(bg))

def try_font(size, bold=False):
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
    bbox = draw.textbbox((0, 0), text, font=font)
    return bbox[2] - bbox[0], bbox[3] - bbox[1]

def draw_text_centered(draw, text, font, color, y):
    tw, th = text_size(draw, text, font)
    draw.text(((W - tw) // 2, y), text, fill=color, font=font)

def draw_rounded_rect(draw, xy, radius, fill, outline=None):
    x1, y1, x2, y2 = xy
    draw.rounded_rectangle([x1, y1, x2, y2], radius=radius, fill=fill, outline=outline)

def draw_btn(draw, xy, text, font, bg, fg, radius=4):
    x1, y1, x2, y2 = xy
    draw_rounded_rect(draw, (x1, y1, x2, y2), radius, bg)
    tw, th = text_size(draw, text, font)
    draw.text((x1 + (x2-x1-tw)//2, y1 + (y2-y1-th)//2), text, fill=fg, font=font)

def draw_icon_btn(draw, xy, img, label, font, bg, fg, radius=6):
    """Draw a button with an icon image and text below."""
    x1, y1, x2, y2 = xy
    draw_rounded_rect(draw, (x1, y1, x2, y2), radius, bg)
    # Center icon
    iw, ih = img.size
    ix = x1 + (x2 - x1 - iw) // 2
    iy = y1 + 8
    draw._image.paste(img, (ix, iy), img if img.mode == 'RGBA' else None)
    # Label below
    tw, th = text_size(draw, label, font)
    draw.text((x1 + (x2-x1-tw)//2, iy + ih + 4), label, fill=fg, font=font)

def save(img, name):
    path = os.path.join(OUT, name)
    img.save(path)
    print(f'  -> {path} ({img.size[0]}x{img.size[1]})')

# ─── LVGL C-array icon parser ──────────────────────────────────────

def parse_lvgl_image(c_file_path):
    """Parse a LVGL C-array image file and return a Pillow Image."""
    with open(c_file_path) as f:
        content = f.read()

    # Extract width and height
    w = int(re.search(r'\.header\.w\s*=\s*(\d+)', content).group(1))
    h = int(re.search(r'\.header\.h\s*=\s*(\d+)', content).group(1))
    cf = re.search(r'\.header\.cf\s*=\s*(\w+)', content).group(1)

    # Extract hex bytes from array (after first comment block)
    hex_match = re.search(r'/\*Pixel format.*?\*/\s*\n\s*((?:0x[0-9a-fA-F]{2},\s*)+)', content, re.DOTALL)
    if not hex_match:
        hex_match = re.search(r'map\[\]\s*=\s*\{[^}]*?/\*.*?\*/\s*\n\s*((?:0x[0-9a-fA-F]{2},\s*)+)', content, re.DOTALL)
    if not hex_match:
        hex_match = re.search(r'map\[\]\s*=\s*\{\s*\n\s*((?:0x[0-9a-fA-F]{2},\s*)+)', content, re.DOTALL)

    if not hex_match:
        print(f"  WARN: cannot parse {c_file_path}")
        return None

    hex_str = hex_match.group(1)
    bytes_list = [int(x.strip(), 16) for x in hex_str.replace('\n', '').split(',') if x.strip()]

    if not bytes_list:
        return None

    # Determine pixel format
    is_alpha = 'ALPHA' in cf or 'TRUE_COLOR_ALPHA' in cf

    if is_alpha:
        # LV_IMG_CF_TRUE_COLOR_ALPHA: 32-bit RGBA8888 (4 bytes/pixel)
        # or Alpha8+RGB332 (2 bytes/pixel)
        bpp = len(bytes_list) // (w * h)
        if bpp == 4:
            # RGBA8888
            img = Image.new('RGBA', (w, h))
            pixels = img.load()
            for y in range(h):
                for x in range(w):
                    i = (y * w + x) * 4
                    r, g, b, a = bytes_list[i:i+4]
                    pixels[x, y] = (r, g, b, a)
            return img
        elif bpp == 2:
            # Alpha8 + RGB332 or Alpha8 + RGB565
            img = Image.new('RGBA', (w, h))
            pixels = img.load()
            for y in range(h):
                for x in range(w):
                    i = (y * w + x) * 2
                    a = bytes_list[i]
                    c = bytes_list[i+1]
                    # Try RGB332: R3 G3 B2
                    r = ((c >> 5) & 0x07) * 255 // 7
                    g = ((c >> 2) & 0x07) * 255 // 7
                    b = (c & 0x03) * 255 // 3
                    pixels[x, y] = (r, g, b, a)
            return img
    else:
        # RGB565 or RGB888
        bpp = len(bytes_list) // (w * h)
        if bpp == 2:
            img = Image.new('RGB', (w, h))
            pixels = img.load()
            for y in range(h):
                for x in range(w):
                    i = (y * w + x) * 2
                    raw = bytes_list[i] | (bytes_list[i+1] << 8)
                    r = ((raw >> 11) & 0x1F) * 255 // 31
                    g = ((raw >> 5) & 0x3F) * 255 // 63
                    b = (raw & 0x1F) * 255 // 31
                    pixels[x, y] = (r, g, b)
            return img
        elif bpp == 3:
            img = Image.new('RGB', (w, h))
            pixels = img.load()
            for y in range(h):
                for x in range(w):
                    i = (y * w + x) * 3
                    pixels[x, y] = tuple(bytes_list[i:i+3])
            return img

    return None


def load_icon(name):
    """Load an app icon by name (e.g., 'music' -> app_music_img.c)."""
    path = os.path.join(SRC, f'app_{name}_img.c')
    if os.path.exists(path):
        img = parse_lvgl_image(path)
        if img:
            # Resize to fit button (48x48 max)
            img = img.resize((42, 42), Image.LANCZOS)
            return img
    return None

# ─── Screen renderers ──────────────────────────────────────────────

def render_home():
    img = new_image(0xFFFFFF)
    draw = ImageDraw.Draw(img)
    font10 = try_font(9)
    font12 = try_font(11)

    # Status bar
    draw_rounded_rect(draw, (0, 0, W, 26), 0, hex_color(0xF0F0F0))
    draw.text((6, 5), '14:30', fill=hex_color(0x000000), font=font12)
    draw.text((W - 36, 5), 'WiFi 85%', fill=hex_color(0x0000FF), font=font10)

    # App grid with real icons
    app_list = [
        ('facetime', 'FaceTime'), ('mail', 'Mail'), ('music', 'Music'),
        ('notes', 'Notes'), ('photos', 'Photos'), ('settings', 'Settings'),
        ('books', 'Books'), ('calculator', 'Calc'), ('contacts', 'Contacts'),
        ('camera', 'Camera'), ('clock', 'Clock'), ('stocks', 'Stocks'),
        ('files', 'Files'), ('pacman', 'Pacman'), ('tetris', 'Tetris'),
    ]
    cols, rows = 3, 5
    cw, ch = 68, 72
    sx, sy = 6, 32

    for r in range(rows):
        for c in range(cols):
            idx = r * cols + c
            if idx >= len(app_list):
                break
            icon_name, label = app_list[idx]
            x = sx + c * cw
            y = sy + r * ch
            icon = load_icon(icon_name)
            if icon:
                draw_icon_btn(draw, (x, y, x + cw - 4, y + ch - 4),
                              icon, label, font10,
                              hex_color(0xE8E8E8), hex_color(0x333333))
            else:
                draw_btn(draw, (x, y, x + cw - 4, y + ch - 4),
                         label[:8], font10,
                         hex_color(0xE8E8E8), hex_color(0x333333))

    save(img, '01_home.bmp')

def render_wifi():
    img = new_image(0x1a1a2e)
    draw = ImageDraw.Draw(img)
    font14 = try_font(11)
    font16 = try_font(13)

    draw_rounded_rect(draw, (0, 0, W, 50), 0, hex_color(0x252550))
    draw_text_centered(draw, 'WiFi Details', font16, hex_color(0xFFFFFF), 16)

    items = [
        ('SSID:', 'MyNetwork'), ('IP:', '192.168.1.100'),
        ('MAC:', 'AA:BB:CC:DD:EE:FF'), ('RSSI:', '-45 dBm'),
        ('Gateway:', '192.168.1.1'),
    ]
    for i, (label, value) in enumerate(items):
        y = 60 + i * 42
        draw_rounded_rect(draw, (6, y, W - 6, y + 36), 4, hex_color(0x252550))
        draw.text((12, y + 8), label, fill=hex_color(0x8888CC), font=font14)
        tw, _ = text_size(draw, value, font14)
        draw.text((W - 12 - tw, y + 8), value, fill=hex_color(0xFFFFFF), font=font14)

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

    draw_text_centered(draw, 'Apple', font16, hex_color(0xFFFFFF), 10)
    draw.text((60, 6), '<', fill=hex_color(0x8888FF), font=font20)
    draw.text((W - 70, 6), '>', fill=hex_color(0x8888FF), font=font20)

    draw_text_centered(draw, '$192.53', font32, hex_color(0xFFFFFF), 170)
    draw_text_centered(draw, '+3.21', font20, hex_color(0x00FF00), 220)
    draw_text_centered(draw, '1D', font16, hex_color(0x8888FF), 270)

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

    keys = ['C', 'D', 'E', 'F', 'G', 'A', 'B', 'C5', 'D5', 'E5', 'F5', 'G5', 'A5', 'C6']
    for i, key in enumerate(keys):
        x = i * 15
        draw_btn(draw, (x, 370, x + 13, 430), key, font12,
                 hex_color(0x444488), hex_color(0xFFFFFF), radius=2)

    draw_btn(draw, (81, 330, 141, 360), 'STOP', font16, hex_color(0x884444), hex_color(0xFFFFFF))
    save(img, '05_music_tone_player.bmp')

def render_photos():
    img = new_image(0x000000)
    draw = ImageDraw.Draw(img)
    font14 = try_font(13)

    draw.text((6, 4), 'File', fill=hex_color(0x888888), font=font14)
    files = [
        ('D', 'photos/'), ('D', '2026-07-30/'),
        ('F', 'img1000.bmp'), ('F', 'img1001.bmp'),
        ('F', 'img1002.bmp'), ('F', 'img1003.bmp'),
    ]
    for i, (ftype, name) in enumerate(files):
        y = 40 + i * 30
        icon = '[DIR]' if ftype == 'D' else '[FILE]'
        draw.text((10, y), f'{icon} {name}', fill=hex_color(0xCCCCCC), font=font14)
    save(img, '06_photos_browser.bmp')

def render_settings():
    """Settings screen with menu items."""
    img = new_image(0xE6E6E6)
    draw = ImageDraw.Draw(img)
    font14 = try_font(13)
    font16 = try_font(14, bold=True)

    # Header
    draw_rounded_rect(draw, (0, 0, W, 45), 0, hex_color(0xFFFFFF))
    draw_text_centered(draw, 'Settings', font16, hex_color(0x000000), 12)
    draw.text((8, 10), '<', fill=hex_color(0x0079F6), font=try_font(18))

    items = [
        'Wi-Fi          MyNetwork', 'Bluetooth      Off',
        'Display & Brightness', 'Sounds & Haptics',
        'Wallpaper', 'About',
    ]
    for i, item in enumerate(items):
        y = 50 + i * 38
        draw_rounded_rect(draw, (0, y, W, y + 36), 0, hex_color(0xFFFFFF))
        draw.text((14, y + 8), item, fill=hex_color(0x000000), font=font14)
        if i < 2:
            tw, _ = text_size(draw, '>', font14)
            draw.text((W - 20, y + 9), '>', fill=hex_color(0x787878), font=font14)

    save(img, '07_settings.bmp')

def render_about():
    """About screen with system info."""
    img = new_image(0xE6E6E6)
    draw = ImageDraw.Draw(img)
    font14 = try_font(13)
    font16 = try_font(14, bold=True)

    draw_rounded_rect(draw, (0, 0, W, 45), 0, hex_color(0xFFFFFF))
    draw_text_centered(draw, 'About', font16, hex_color(0x000000), 12)

    items = [
        ('Name', 'LILYGO 1.0.0'), ('IDF versions', 'IDF 5.1.0'),
        ('CPU model', 'ESP32S3'), ('CPU core data', 'Two'),
        ('Memory capacity', '16MB'), ('Screen size', '2.33"'),
        ('USB', 'Connected'), ('USB Voltage', '5000mV'),
        ('Battery Voltage', '3840mV'),
    ]
    for i, (label, value) in enumerate(items):
        y = 50 + i * 34
        draw_rounded_rect(draw, (0, y, W, y + 32), 0, hex_color(0xFFFFFF))
        draw.text((14, y + 7), label, fill=hex_color(0x000000), font=font14)
        tw, _ = text_size(draw, value, font14)
        draw.text((W - 14 - tw, y + 7), value, fill=hex_color(0x787878), font=font14)

    save(img, '08_about.bmp')

def render_camera():
    """Camera viewfinder screen."""
    img = new_image(0x000000)
    draw = ImageDraw.Draw(img)
    font12 = try_font(11)
    font14 = try_font(13)

    # Viewfinder area (simulated)
    draw_rounded_rect(draw, (10, 10, W - 10, 340), 2, hex_color(0x1a1a1a),
                      outline=hex_color(0x444444))
    draw_text_centered(draw, '[ Camera Preview ]', font14, hex_color(0x666666), 160)

    # Controls
    draw_btn(draw, (6, 370, 60, 410), 'FL', font12, hex_color(0x444444), hex_color(0xFFFFFF))
    draw_btn(draw, (70, 370, 130, 410), 'AF', font12, hex_color(0x444444), hex_color(0xFFFFFF))
    draw_btn(draw, (140, 370, 195, 410), 'Rot', font12, hex_color(0x444444), hex_color(0xFFFFFF))

    # Capture button
    draw_btn(draw, (61, 430, 161, 462), 'CAPTURE', font14,
             hex_color(0xFFFFFF), hex_color(0x000000), radius=30)
    save(img, '09_camera.bmp')

def render_files():
    """File browser screen."""
    img = new_image(0x1a1a2e)
    draw = ImageDraw.Draw(img)
    font12 = try_font(10)
    font14 = try_font(13)
    font16 = try_font(14, bold=True)

    draw_rounded_rect(draw, (0, 0, W, 45), 0, hex_color(0x252550))
    draw.text((8, 10), '< Back', fill=hex_color(0xFFFFFF), font=font14)
    draw_text_centered(draw, 'SD Browser', font16, hex_color(0xFFFFFF), 12)

    # Path
    draw.text((8, 50), '/photos/2026-07-30', fill=hex_color(0x888888), font=font12)

    entries = [
        ('DIR', '..'), ('FILE', 'img1000.bmp'), ('FILE', 'img1001.bmp'),
        ('FILE', 'img1002.bmp'), ('FILE', 'img1003.bmp'),
        ('FILE', 'img1004.bmp'), ('FILE', 'img1005.bmp'),
    ]
    for i, (etype, name) in enumerate(entries):
        y = 75 + i * 32
        draw_rounded_rect(draw, (6, y, W - 6, y + 28), 4, hex_color(0x222244))
        icon = '[DIR] ' if etype == 'DIR' else '      '
        draw.text((14, y + 5), f'{icon}{name}', fill=hex_color(0xCCCCCC), font=font14)
    save(img, '10_files.bmp')

def render_pacman():
    """Pac-Man game screen mockup."""
    img = new_image(0x000000)
    draw = ImageDraw.Draw(img)
    font16 = try_font(14, bold=True)
    font12 = try_font(11)

    # HUD
    draw.text((8, 4), 'SCORE: 1250', fill=hex_color(0xFFFF00), font=font16)
    draw.text((W - 50, 4), '3 UP', fill=hex_color(0xFFFF00), font=font16)

    # Maze area (simplified grid)
    draw_rounded_rect(draw, (10, 28, W - 10, 380), 2, hex_color(0x000033),
                      outline=hex_color(0x2222AA))

    # Dots
    for y in range(50, 370, 16):
        for x in range(20, 200, 16):
            draw.ellipse([x-1, y-1, x+1, y+1], fill=hex_color(0xFFFF88))

    # Pac-Man (yellow circle)
    draw.pieslice([70, 170, 86, 186], 30, 330, fill=hex_color(0xFFFF00))

    # Ghosts
    ghosts = [(0xFF0000, 120, 170), (0xFFB8FF, 136, 170),
              (0x00FFFF, 152, 170), (0xFFB852, 168, 170)]
    for color, gx, gy in ghosts:
        draw_rounded_rect(draw, (gx, gy, gx + 12, gy + 12), 4, hex_color(color))

    # Controls hint
    draw_text_centered(draw, 'BTN1:move  BTN2:direction', font12, hex_color(0x888888), 420)

    save(img, '11_pacman.bmp')

def render_tetris():
    """Tetris game screen mockup."""
    img = new_image(0x000000)
    draw = ImageDraw.Draw(img)
    font16 = try_font(14, bold=True)
    font12 = try_font(11)

    # HUD
    draw.text((8, 4), 'SCORE: 850', fill=hex_color(0x00FFFF), font=font16)
    draw.text((W - 55, 4), 'LVL: 3', fill=hex_color(0x00FFFF), font=font16)

    # Board (10x20 grid)
    board_x, board_y = 10, 30
    cell = 16
    colors = [0x00FFFF, 0x0000FF, 0xFF8800, 0xFFFF00, 0x00FF00, 0x8800FF, 0xFF0000]
    # Draw some placed blocks
    for y in range(12, 20):
        for x in range(10):
            if (x + y) % 3 != 0:
                c = colors[(x * y) % 7]
                draw_rounded_rect(draw, (board_x + x*cell, board_y + y*cell,
                                         board_x + x*cell + cell - 2, board_y + y*cell + cell - 2),
                                  1, hex_color(c))

    # Falling piece
    for dx, dy in [(0, 0), (1, 0), (0, 1), (1, 1)]:
        x, y = 4 + dx, 8 + dy
        draw_rounded_rect(draw, (board_x + x*cell, board_y + y*cell,
                                 board_x + x*cell + cell - 2, board_y + y*cell + cell - 2),
                          1, hex_color(0xFF4444))

    # Grid outline
    draw_rounded_rect(draw, (board_x, board_y, board_x + 10*cell, board_y + 20*cell),
                      1, None, outline=hex_color(0x333333))

    # Next piece preview
    draw.text((board_x + 10*cell + 10, board_y), 'NEXT', fill=hex_color(0x888888), font=font12)
    draw_rounded_rect(draw, (board_x + 10*cell + 10, board_y + 20, board_x + 10*cell + 54, board_y + 60),
                      2, None, outline=hex_color(0x333333))

    draw_text_centered(draw, 'BTN1:rotate  BTN2:drop', font12, hex_color(0x888888), 450)
    save(img, '12_tetris.bmp')

def render_counter():
    """Calculator/Counter screen."""
    img = new_image(0xFFFFFF)
    draw = ImageDraw.Draw(img)
    font16 = try_font(16, bold=True)
    font14 = try_font(13)

    # Display
    draw_rounded_rect(draw, (6, 10, W - 6, 90), 6, hex_color(0xF0F0F0))
    draw_text_centered(draw, '0', font16, hex_color(0x000000), 40)

    # Keypad (4x5 grid)
    keys = [
        'C', '/', '*', 'Del',
        '7', '8', '9', '-',
        '4', '5', '6', '+',
        '1', '2', '3', '=',
        '%', '0', '.', ''
    ]
    for i, key in enumerate(keys):
        if not key: continue
        col = i % 4
        row = i // 4
        x = 6 + col * 52
        y = 110 + row * 52
        draw_btn(draw, (x, y, x + 48, y + 48), key, font14,
                 hex_color(0xE8E8E8), hex_color(0x000000))
    save(img, '13_counter.bmp')

def render_control_center():
    """Control center (slide-up panel)."""
    img = new_image(0x1a1a1a)
    draw = ImageDraw.Draw(img)
    font12 = try_font(11)
    font14 = try_font(13)

    # Panel
    draw_rounded_rect(draw, (0, 40, W, H), 12, hex_color(0x2a2a2a))
    draw_text_centered(draw, 'Control Center', font14, hex_color(0xFFFFFF), 50)

    # Toggles row 1
    toggles = [
        ('WiFi', 0x0000FF), ('Bluetooth', 0x0044FF),
        ('Airplane', 0xFF8800), ('Flashlight', 0xFFFF00),
    ]
    for i, (name, color) in enumerate(toggles):
        x = 10 + i * 52
        y = 90
        draw_rounded_rect(draw, (x, y, x + 44, y + 44), 8, hex_color(color))
        tw, _ = text_size(draw, name, font12)
        draw.text((x + (44-tw)//2, y + 14), name, fill=hex_color(0xFFFFFF), font=font12)

    # Brightness slider
    draw.text((10, 150), 'Brightness', fill=hex_color(0xCCCCCC), font=font12)
    draw_rounded_rect(draw, (10, 170, W - 10, 180), 4, hex_color(0x555555))
    draw_rounded_rect(draw, (10, 170, 140, 180), 4, hex_color(0x0079F6))

    # Volume slider
    draw.text((10, 200), 'Volume', fill=hex_color(0xCCCCCC), font=font12)
    draw_rounded_rect(draw, (10, 220, W - 10, 230), 4, hex_color(0x555555))
    draw_rounded_rect(draw, (10, 220, 100, 230), 4, hex_color(0x0079F6))

    # Motor slider
    draw.text((10, 250), 'Vibrating Motor  0%', fill=hex_color(0xCCCCCC), font=font12)
    draw_rounded_rect(draw, (10, 270, W - 10, 280), 4, hex_color(0x555555))
    draw_rounded_rect(draw, (10, 270, 10, 280), 4, hex_color(0x0079F6))

    save(img, '14_control_center.bmp')

def render_wifi_settings():
    """WiFi settings (scan + connect) screen."""
    img = new_image(0xE6E6E6)
    draw = ImageDraw.Draw(img)
    font12 = try_font(10)
    font14 = try_font(13)
    font16 = try_font(14, bold=True)

    draw_rounded_rect(draw, (0, 0, W, 45), 0, hex_color(0xFFFFFF))
    draw.text((8, 10), '<', fill=hex_color(0x0079F6), font=try_font(18))
    draw_text_centered(draw, 'Wi-Fi', font16, hex_color(0x000000), 12)

    # Current network
    draw_rounded_rect(draw, (0, 45, W, 75), 0, hex_color(0xFFFFFF))
    draw.text((14, 52), 'MyNetwork', fill=hex_color(0x000000), font=font16)
    draw.text((14, 66), 'Connected', fill=hex_color(0x00AA00), font=font12)

    # Available networks
    nets = ['Xiaomi_33BB', 'LilyGo-AABB', 'ysw-office', 'GL-MT1300-44e']
    for i, net in enumerate(nets):
        y = 80 + i * 36
        draw_rounded_rect(draw, (0, y, W, y + 34), 0, hex_color(0xFFFFFF))
        draw.text((14, y + 8), net, fill=hex_color(0x000000), font=font14)
        draw.text((W - 36, y + 8), '>', fill=hex_color(0x787878), font=font14)

    # Scan button
    draw_btn(draw, (6, 230, W - 6, 260), 'Scan for networks', font14,
             hex_color(0x0079F6), hex_color(0xFFFFFF))
    save(img, '15_wifi_settings.bmp')

# ─── Main ──────────────────────────────────────────────────────────

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
    render_settings()
    render_about()
    render_camera()
    render_files()
    render_pacman()
    render_tetris()
    render_counter()
    render_control_center()
    render_wifi_settings()

    print(f'\nDone. 15 screenshots in {OUT}/')
