#!/usr/bin/env python3
"""Demo screensaver for the JZ-HDL FPGA 720p terminal (142x45 chars).

Shows two effects:
  1. Star Warp — stars flying toward you from a central vanishing point (30s)
  2. Starry Night Cityscape — twinkling stars over a city skyline (30s)

Usage:
    demo.py                          List available serial ports
    demo.py <device> <baud> <config> Run over serial
    demo.py --local                  Run locally in terminal

Example:
    demo.py /dev/cu.usbserial-1420 115200 N81
"""

import sys
import os
import random
import time
import math

try:
    import serial
    import serial.tools.list_ports
    HAS_SERIAL = True
except ImportError:
    HAS_SERIAL = False

try:
    import tty
    import termios
except ImportError:
    tty = None
    termios = None

# Terminal dimensions (720p @ 9x16 font)
COLS = 142
ROWS = 45

# ANSI
CSI = "\033["
RESET = f"{CSI}0m"
BOLD = f"{CSI}1m"
DIM = f"{CSI}2m"
HIDE_CURSOR = f"{CSI}?25l"
SHOW_CURSOR = f"{CSI}?25h"
CLEAR_SCREEN = f"{CSI}2J"
HOME = f"{CSI}H"
WRAP_OFF = f"{CSI}?7l"
WRAP_ON = f"{CSI}?7h"


def fg256(n):
    return f"{CSI}38;5;{n}m"

def bg256(n):
    return f"{CSI}48;5;{n}m"

def move(row, col):
    return f"{CSI}{row+1};{col+1}H"


# ---------------------------------------------------------------------------
# Star Warp Effect
# ---------------------------------------------------------------------------

class Star:
    __slots__ = ('x', 'y', 'z', 'psx', 'psy', 'drawn')

    def __init__(self):
        self.reset()
        self.z = random.uniform(0.1, 1.0)
        self.psx = -1
        self.psy = -1
        self.drawn = False

    def reset(self):
        self.x = random.uniform(-1.0, 1.0)
        self.y = random.uniform(-1.0, 1.0)
        self.z = 1.0
        self.psx = -1
        self.psy = -1
        self.drawn = False


WARP_CHARS = ['.', '+', '*', '#', '@']


class StarWarp:
    """Stars flying toward viewer from central vanishing point."""

    def __init__(self, writer, num_stars=180):
        self.writer = writer
        self.cx = COLS // 2
        self.cy = ROWS // 2
        self.stars = [Star() for _ in range(num_stars)]
        self.speed = 0.012
        self.buf = []

    def _flush(self):
        if self.buf:
            self.writer(''.join(self.buf))
            self.buf.clear()

    def _put(self, row, col, text):
        self.buf.append(f"{move(row, col)}{text}")

    def setup(self):
        self.writer(CLEAR_SCREEN)

    def tick(self, dt, elapsed):
        # Accelerate over time
        self.speed = 0.008 + 0.025 * (elapsed / 30.0)

        for star in self.stars:
            # Erase old position
            if star.drawn and 0 <= star.psy < ROWS and 0 <= star.psx < COLS:
                self._put(star.psy, star.psx, ' ')
                star.drawn = False

            # Move star closer
            star.z -= self.speed * dt * 30
            if star.z <= 0.005:
                star.reset()
                continue

            # Project to screen
            sx = int(self.cx + star.x / star.z * self.cx)
            sy = int(self.cy + star.y / star.z * self.cy * 0.5)

            if sx < 0 or sx >= COLS or sy < 0 or sy >= ROWS:
                star.reset()
                continue

            # Brightness based on depth (closer = brighter)
            bright = 1.0 - star.z
            if bright < 0.15:
                ch = WARP_CHARS[0]
                color = fg256(236)
            elif bright < 0.35:
                ch = WARP_CHARS[1]
                color = fg256(240)
            elif bright < 0.55:
                ch = WARP_CHARS[2]
                color = fg256(248)
            elif bright < 0.75:
                ch = WARP_CHARS[3]
                color = fg256(255)
            else:
                ch = WARP_CHARS[4]
                color = f"{BOLD}{fg256(231)}"

            # Draw streak for fast-moving close stars
            if bright > 0.6 and star.psx >= 0:
                # Draw a short trail
                dx = sx - star.psx
                dy = sy - star.psy
                if abs(dx) <= 3 and abs(dy) <= 2 and (dx != 0 or dy != 0):
                    trail_color = fg256(238)
                    if 0 <= star.psy < ROWS and 0 <= star.psx < COLS:
                        self._put(star.psy, star.psx, f"{trail_color}.{RESET}")

            self._put(sy, sx, f"{color}{ch}{RESET}")
            star.psx = sx
            star.psy = sy
            star.drawn = True

        # Title
        title = "< S T A R   W A R P >"
        tx = (COLS - len(title)) // 2
        pulse = int(abs(math.sin(elapsed * 2.0)) * 4)
        title_color = fg256(21 + pulse)
        self._put(1, tx, f"{BOLD}{title_color}{title}{RESET}")

        self._flush()


# ---------------------------------------------------------------------------
# Starry Night Cityscape
# ---------------------------------------------------------------------------

class TwinkleStar:
    __slots__ = ('row', 'col', 'phase', 'speed', 'base_char', 'drawn_char')

    CHARS = [' ', '.', '+', '*', '+', '.']

    def __init__(self, row, col):
        self.row = row
        self.col = col
        self.phase = random.uniform(0, math.pi * 2)
        self.speed = random.uniform(0.5, 2.5)
        self.base_char = random.choice(['.', '*', '+'])
        self.drawn_char = ''


# City skyline building definitions: (left_col, width, height)
# Heights are from bottom of screen
def generate_skyline():
    buildings = []
    col = 0
    while col < COLS:
        w = random.randint(4, 12)
        h = random.randint(6, 22)
        gap = random.randint(0, 2)
        if col + w > COLS:
            w = COLS - col
        buildings.append((col, w, h))
        col += w + gap
    return buildings


class StarryNight:
    """Twinkling stars over a dark city skyline with lit windows."""

    def __init__(self, writer):
        self.writer = writer
        self.buildings = generate_skyline()
        self.sky_stars = []
        self.building_mask = set()  # (row, col) occupied by buildings
        self.window_states = {}     # (row, col) -> on/off
        self.window_coords = []
        self.buf = []
        self.last_window_toggle = 0
        self.shooting_stars = []
        self._build_city()

    def _flush(self):
        if self.buf:
            self.writer(''.join(self.buf))
            self.buf.clear()

    def _put(self, row, col, text):
        self.buf.append(f"{move(row, col)}{text}")

    def _build_city(self):
        # Mark building cells
        for left, w, h in self.buildings:
            for r in range(ROWS - h, ROWS):
                for c in range(left, min(left + w, COLS)):
                    self.building_mask.add((r, c))

        # Create sky stars (only in non-building cells)
        for _ in range(200):
            r = random.randint(0, ROWS - 2)
            c = random.randint(0, COLS - 1)
            if (r, c) not in self.building_mask:
                self.sky_stars.append(TwinkleStar(r, c))

        # Create windows on buildings
        for left, w, h in self.buildings:
            top = ROWS - h
            # Windows start 2 rows from top of building, every other row, every 3 cols
            for r in range(top + 2, ROWS - 1, 2):
                for c in range(left + 1, left + w - 1, 3):
                    if c < COLS:
                        self.window_coords.append((r, c))
                        self.window_states[(r, c)] = random.random() < 0.4

    def setup(self):
        self.writer(CLEAR_SCREEN)

        # Draw sky background (dark blue-black)
        # We'll let the stars handle sky content

        # Draw buildings
        for left, w, h in self.buildings:
            top = ROWS - h
            # Roof
            roof_shade = random.choice([234, 235, 236])
            self._put(top, left, f"{fg256(roof_shade)}{'_' * w}{RESET}")
            # Body
            for r in range(top + 1, ROWS):
                shade = 233 + (r - top) // 4  # slightly lighter near bottom
                shade = min(shade, 237)
                self._put(r, left, f"{fg256(shade)}{'|'}{' ' * (w - 2)}{'|' if left + w - 1 < COLS else ''}{RESET}")

        # Draw initial windows
        for (r, c) in self.window_coords:
            on = self.window_states[(r, c)]
            if on:
                color = random.choice([fg256(228), fg256(229), fg256(230)])  # warm yellow
                self._put(r, c, f"{color}#{RESET}")
            else:
                self._put(r, c, f"{fg256(235)} {RESET}")

        # Draw initial stars
        for star in self.sky_stars:
            self._put(star.row, star.col, f"{fg256(240)}{star.base_char}{RESET}")
            star.drawn_char = star.base_char

        # Ground line
        for c in range(COLS):
            if (ROWS - 1, c) not in self.building_mask:
                self._put(ROWS - 1, c, f"{fg256(236)}_{RESET}")

        self._flush()

    def tick(self, dt, elapsed):
        # Twinkle stars
        for star in self.sky_stars:
            phase_val = math.sin(elapsed * star.speed + star.phase)
            if phase_val > 0.6:
                ch = '*'
                color = fg256(255)
            elif phase_val > 0.2:
                ch = '+'
                color = fg256(248)
            elif phase_val > -0.2:
                ch = star.base_char
                color = fg256(242)
            elif phase_val > -0.6:
                ch = '.'
                color = fg256(238)
            else:
                ch = ' '
                color = ''

            if ch != star.drawn_char:
                if ch == ' ':
                    self._put(star.row, star.col, ' ')
                else:
                    self._put(star.row, star.col, f"{color}{ch}{RESET}")
                star.drawn_char = ch

        # Toggle some windows periodically
        if elapsed - self.last_window_toggle > 2.0:
            self.last_window_toggle = elapsed
            for _ in range(random.randint(1, 4)):
                if self.window_coords:
                    r, c = random.choice(self.window_coords)
                    self.window_states[(r, c)] = not self.window_states[(r, c)]
                    on = self.window_states[(r, c)]
                    if on:
                        color = random.choice([fg256(228), fg256(229), fg256(230)])
                        self._put(r, c, f"{color}#{RESET}")
                    else:
                        self._put(r, c, f"{fg256(235)} {RESET}")

        # Shooting stars
        if random.random() < 0.008:
            # Spawn a new shooting star
            sr = random.randint(1, ROWS // 3)
            sc = random.randint(10, COLS - 10)
            self.shooting_stars.append({
                'row': sr, 'col': sc,
                'dr': 0.4, 'dc': random.choice([-1.5, 1.5]),
                'life': 0, 'max_life': random.randint(6, 14),
                'trail': [],
            })

        new_shooters = []
        for ss in self.shooting_stars:
            # Erase old trail end
            if len(ss['trail']) > 4:
                old_r, old_c = ss['trail'][0]
                if 0 <= old_r < ROWS and 0 <= old_c < COLS and (old_r, old_c) not in self.building_mask:
                    self._put(old_r, old_c, ' ')
                ss['trail'].pop(0)

            # Move
            ss['row'] += ss['dr']
            ss['col'] += ss['dc']
            ss['life'] += 1
            r, c = int(ss['row']), int(ss['col'])

            if ss['life'] >= ss['max_life'] or r < 0 or r >= ROWS or c < 0 or c >= COLS:
                # Erase remaining trail
                for tr, tc in ss['trail']:
                    if 0 <= tr < ROWS and 0 <= tc < COLS and (tr, tc) not in self.building_mask:
                        self._put(tr, tc, ' ')
                continue

            if (r, c) not in self.building_mask:
                ss['trail'].append((r, c))
                # Draw head
                self._put(r, c, f"{BOLD}{fg256(231)}*{RESET}")
                # Dim trail
                for i, (tr, tc) in enumerate(ss['trail'][:-1]):
                    if 0 <= tr < ROWS and 0 <= tc < COLS and (tr, tc) not in self.building_mask:
                        shade = 240 + (i * 2)
                        shade = min(shade, 250)
                        self._put(tr, tc, f"{fg256(shade)}.{RESET}")
                new_shooters.append(ss)
            else:
                for tr, tc in ss['trail']:
                    if 0 <= tr < ROWS and 0 <= tc < COLS and (tr, tc) not in self.building_mask:
                        self._put(tr, tc, ' ')

        self.shooting_stars = new_shooters

        # Title
        title = "- S T A R R Y   N I G H T -"
        tx = (COLS - len(title)) // 2
        glow = int((math.sin(elapsed * 1.5) + 1) * 2)
        title_color = fg256(17 + glow)
        self._put(0, tx, f"{DIM}{title_color}{title}{RESET}")

        self._flush()


# ---------------------------------------------------------------------------
# Main Loop
# ---------------------------------------------------------------------------

def run_demo(writer, check_quit):
    """Run both effects sequentially."""
    writer(WRAP_OFF)
    writer(HIDE_CURSOR)

    effects = [
        ("warp", StarWarp(writer)),
        ("night", StarryNight(writer)),
    ]

    for name, effect in effects:
        effect.setup()
        start = time.time()
        last = start
        while True:
            now = time.time()
            elapsed = now - start
            dt = now - last
            last = now

            if elapsed >= 30.0:
                break

            if check_quit():
                return

            effect.tick(dt, elapsed)

            # ~30 fps target
            frame_time = time.time() - now
            sleep_time = (1.0 / 30.0) - frame_time
            if sleep_time > 0:
                time.sleep(sleep_time)


def run_local():
    """Run locally in the current terminal."""
    import select

    def writer(data):
        sys.stdout.write(data)
        sys.stdout.flush()

    old_settings = None
    fd = sys.stdin.fileno()
    if termios:
        old_settings = termios.tcgetattr(fd)
        tty.setraw(fd)

    quit_flag = [False]

    def check_quit():
        if select.select([sys.stdin], [], [], 0)[0]:
            ch = os.read(fd, 1)
            if ch in (b'\x1b', b'\x03', b'q', b'Q'):
                quit_flag[0] = True
        return quit_flag[0]

    try:
        writer(f"{CSI}?1049h")  # Alternate screen buffer
        run_demo(writer, check_quit)
    finally:
        writer(SHOW_CURSOR)
        writer(WRAP_ON)
        writer(RESET)
        if old_settings and termios:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        writer(f"{CSI}?1049l")  # Leave alternate screen
        print("\nDemo complete!")


def run_serial(device, baud, config_str):
    """Run over serial connection to FPGA terminal."""
    if not HAS_SERIAL:
        print("Error: pyserial is required. Install with: pip install pyserial")
        sys.exit(1)

    import select

    parity_map = {'N': serial.PARITY_NONE, 'E': serial.PARITY_EVEN, 'O': serial.PARITY_ODD}
    if len(config_str) != 3:
        print(f"Config must be 3 chars (e.g. N81), got: {config_str!r}")
        sys.exit(1)

    parity = parity_map.get(config_str[0].upper())
    if parity is None:
        print("Parity must be N, E, or O")
        sys.exit(1)
    databits = int(config_str[1])
    stopbits = int(config_str[2])

    try:
        ser = serial.Serial(
            port=device, baudrate=baud,
            bytesize=databits, parity=parity, stopbits=stopbits,
            timeout=0.1,
        )
    except serial.SerialException as e:
        print(f"Error opening {device}: {e}")
        sys.exit(1)

    def writer(data):
        ser.write(data.encode('utf-8'))
        time.sleep(0.002)

    old_settings = None
    fd = sys.stdin.fileno()
    if termios:
        old_settings = termios.tcgetattr(fd)
        tty.setraw(fd)

    quit_flag = [False]

    def check_quit():
        if select.select([sys.stdin], [], [], 0)[0]:
            ch = os.read(fd, 1)
            if ch in (b'\x1b', b'\x03', b'q', b'Q'):
                quit_flag[0] = True
        return quit_flag[0]

    try:
        run_demo(writer, check_quit)
    finally:
        writer(SHOW_CURSOR)
        writer(WRAP_ON)
        writer(RESET)
        writer(f"{CLEAR_SCREEN}{HOME}")
        ser.close()
        if old_settings and termios:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        print("\nDemo complete!")


def list_ports():
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print(f"{'Device':<30} {'Description':<40} {'HWID'}")
    print("-" * 100)
    for p in sorted(ports, key=lambda x: x.device):
        print(f"{p.device:<30} {p.description:<40} {p.hwid}")


def main():
    args = sys.argv[1:]

    if not args:
        if HAS_SERIAL:
            list_ports()
        else:
            print("Usage: demo.py --local              (run locally)")
            print("       demo.py <device> <baud> <config>  (run over serial)")
            print("\nNote: pyserial not installed, serial mode unavailable")
        return

    if args[0] == '--local':
        run_local()
        return

    if len(args) != 3:
        print("Usage: demo.py <device> <baud> <config>")
        print("       demo.py --local")
        print("\nExample: demo.py /dev/cu.usbserial-1420 115200 N81")
        sys.exit(1)

    device = args[0]
    try:
        baud = int(args[1])
    except ValueError:
        print(f"Baud rate must be an integer, got: {args[1]!r}")
        sys.exit(1)

    run_serial(device, baud, args[2])


if __name__ == '__main__':
    main()
