#!/usr/bin/env python3
"""Hangman game for the JZ-HDL FPGA 720p terminal (142x45 chars).

Connects over serial and renders a full-screen hangman game using
box-drawing characters and ANSI colors.

Usage:
    hangman.py                          List available serial ports
    hangman.py <device> <baud> <config> Start game
    hangman.py --local                  Play locally (no serial)

Example:
    hangman.py /dev/cu.usbserial-1420 115200 N81
"""

import sys
import os
import random
import time

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

# ANSI color codes
CSI = "\033["
RESET = f"{CSI}0m"
BOLD = f"{CSI}1m"
DIM = f"{CSI}2m"
BLINK = f"{CSI}5m"
REVERSE = f"{CSI}7m"

FG_BLACK   = f"{CSI}30m"
FG_RED     = f"{CSI}31m"
FG_GREEN   = f"{CSI}32m"
FG_YELLOW  = f"{CSI}33m"
FG_BLUE    = f"{CSI}34m"
FG_MAGENTA = f"{CSI}35m"
FG_CYAN    = f"{CSI}36m"
FG_WHITE   = f"{CSI}37m"

BG_BLACK   = f"{CSI}40m"
BG_RED     = f"{CSI}41m"
BG_GREEN   = f"{CSI}42m"
BG_YELLOW  = f"{CSI}43m"
BG_BLUE    = f"{CSI}44m"
BG_MAGENTA = f"{CSI}45m"
BG_CYAN    = f"{CSI}46m"
BG_WHITE   = f"{CSI}47m"

# Word list - common English words, various difficulty
WORDS = [
    # Easy (4-5 letters)
    "code", "chip", "wire", "gate", "flip", "flop", "port", "sync",
    "wave", "byte", "data", "loop", "tree", "node", "path", "link",
    "hash", "heap", "pipe", "core", "fuse", "cell", "load", "test",
    "fish", "bird", "cake", "moon", "star", "rain", "snow", "wind",
    # Medium (6-7 letters)
    "buffer", "signal", "module", "driver", "output", "kernel",
    "memory", "switch", "bridge", "socket", "serial", "binary",
    "decode", "encode", "toggle", "latch", "parser", "syntax",
    "garden", "castle", "puzzle", "rocket", "planet", "stream",
    "window", "dragon", "knight", "sunset", "winter", "summer",
    # Hard (8+ letters)
    "register", "compiler", "hardware", "protocol", "function",
    "overflow", "pipeline", "firmware", "ethernet", "keyboard",
    "terminal", "processor", "interrupt", "resonance", "amplifier",
    "capacitor", "frequency", "oscillate", "transform", "algorithm",
    "adventure", "butterfly", "challenge", "dangerous", "elaborate",
]

# Hangman stages (box-drawing gallows, double-size)
GALLOWS = [
    # Stage 0: empty gallows
    [
        "    ╔════════════╗     ",
        "    ║            │     ",
        "    ║            │     ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "════╩════════          ",
    ],
    # Stage 1: head
    [
        "    ╔════════════╗     ",
        "    ║            │     ",
        "    ║            │     ",
        "    ║           ┌─┐    ",
        "    ║           │O│    ",
        "    ║           └─┘    ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "════╩════════          ",
    ],
    # Stage 2: body
    [
        "    ╔════════════╗     ",
        "    ║            │     ",
        "    ║            │     ",
        "    ║           ┌─┐    ",
        "    ║           │O│    ",
        "    ║           └─┘    ",
        "    ║            │     ",
        "    ║            │     ",
        "    ║            │     ",
        "    ║            │     ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "════╩════════          ",
    ],
    # Stage 3: left arm
    [
        "    ╔════════════╗     ",
        "    ║            │     ",
        "    ║            │     ",
        "    ║           ┌─┐    ",
        "    ║           │O│    ",
        "    ║           └─┘    ",
        "    ║           /│     ",
        "    ║          / │     ",
        "    ║         /  │     ",
        "    ║            │     ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "════╩════════          ",
    ],
    # Stage 4: right arm
    [
        "    ╔════════════╗     ",
        "    ║            │     ",
        "    ║            │     ",
        "    ║           ┌─┐    ",
        "    ║           │O│    ",
        "    ║           └─┘    ",
        "    ║           /│\\    ",
        "    ║          / │ \\   ",
        "    ║         /  │  \\  ",
        "    ║            │     ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "    ║                  ",
        "════╩════════          ",
    ],
    # Stage 5: left leg
    [
        "    ╔════════════╗     ",
        "    ║            │     ",
        "    ║            │     ",
        "    ║           ┌─┐    ",
        "    ║           │O│    ",
        "    ║           └─┘    ",
        "    ║           /│\\    ",
        "    ║          / │ \\   ",
        "    ║         /  │  \\  ",
        "    ║            │     ",
        "    ║           /      ",
        "    ║          /       ",
        "    ║         /        ",
        "    ║                  ",
        "    ║                  ",
        "════╩════════          ",
    ],
    # Stage 6: right leg (dead)
    [
        "    ╔════════════╗     ",
        "    ║            │     ",
        "    ║            │     ",
        "    ║           ┌─┐    ",
        "    ║           │X│    ",
        "    ║           └─┘    ",
        "    ║           /│\\    ",
        "    ║          / │ \\   ",
        "    ║         /  │  \\  ",
        "    ║            │     ",
        "    ║           / \\    ",
        "    ║          /   \\   ",
        "    ║         /     \\  ",
        "    ║                  ",
        "    ║                  ",
        "════╩════════          ",
    ],
]

MAX_WRONG = 6


class HangmanGame:
    def __init__(self):
        self.word = ""
        self.guessed = set()
        self.wrong = 0
        self.wins = 0
        self.losses = 0
        self.game_over = False
        self.won = False
        self.message = ""
        self.new_game()

    def new_game(self):
        self.word = random.choice(WORDS).upper()
        self.guessed = set()
        self.wrong = 0
        self.game_over = False
        self.won = False
        self.message = ""

    def guess(self, letter):
        letter = letter.upper()
        if not letter.isalpha() or len(letter) != 1:
            return
        if letter in self.guessed:
            self.message = f"Already guessed '{letter}'!"
            return
        self.guessed.add(letter)
        if letter in self.word:
            self.message = f"'{letter}' is correct!"
            if all(c in self.guessed for c in self.word):
                self.game_over = True
                self.won = True
                self.wins += 1
                self.message = "You WIN!"
        else:
            self.wrong += 1
            self.message = f"'{letter}' is not in the word."
            if self.wrong >= MAX_WRONG:
                self.game_over = True
                self.won = False
                self.losses += 1
                self.message = f"Game over! The word was: {self.word}"

    def display_word(self):
        if self.game_over and not self.won:
            return " ".join(self.word)
        return " ".join(c if c in self.guessed else "_" for c in self.word)


class Renderer:
    """Renders the hangman game to the FPGA terminal using ANSI positioning."""

    # Layout constants
    GALLOWS_LEFT = 6
    GALLOWS_TOP = 4
    GALLOWS_BOX_LEFT = 4
    GALLOWS_BOX_TOP = 3
    GALLOWS_BOX_W = 26
    GALLOWS_BOX_H = 20
    WORD_ROW = 25
    WORD_LEFT = 4
    BOARD_LEFT = 34
    BOARD_TOP = 4
    LETTERS_PER_ROW = 9
    CELL_W = 7
    WRONG_TOP = 30
    WRONG_LEFT = 4
    MSG_ROW = ROWS - 5
    PROMPT_ROW = ROWS - 3

    def __init__(self, writer):
        self.writer = writer
        self.inner_w = 1 + self.LETTERS_PER_ROW * self.CELL_W + 1

    def write_at(self, row, col, text):
        """Write text with ANSI codes at position (0-based)."""
        self.writer(f"{CSI}{row+1};{col+1}H{text}")

    def _letter_position(self, ch):
        """Return (row_on_screen, col_on_screen) for a letter's cell top-left."""
        idx = ord(ch) - ord('A')
        grid_row = idx // self.LETTERS_PER_ROW
        grid_col = idx % self.LETTERS_PER_ROW
        # Each grid_row is 3 screen rows + 1 separator (except after last)
        screen_row = self.BOARD_TOP + 2 + grid_row * 4
        screen_col = self.BOARD_LEFT + 2 + grid_col * self.CELL_W
        return screen_row, screen_col

    def _draw_letter_cell(self, ch, game):
        """Draw a single letter cell (3 rows of 5 chars)."""
        row, col = self._letter_position(ch)
        if ch in game.guessed:
            color = FG_GREEN if ch in game.word else FG_RED
            self.write_at(row,     col, f"{color}┌───┐{RESET}")
            self.write_at(row + 1, col, f"{color}│   │{RESET}")
            self.write_at(row + 2, col, f"{color}└───┘{RESET}")
        else:
            self.write_at(row,     col, f"{FG_WHITE}┌───┐{RESET}")
            self.write_at(row + 1, col, f"{FG_WHITE}│{FG_CYAN}{BOLD} {ch} {RESET}{FG_WHITE}│{RESET}")
            self.write_at(row + 2, col, f"{FG_WHITE}└───┘{RESET}")

    def _draw_gallows(self, game):
        """Draw the gallows figure."""
        stage = min(game.wrong, MAX_WRONG)
        gallows = GALLOWS[stage]
        body_color = FG_RED if game.game_over and not game.won else FG_WHITE
        for i, line in enumerate(gallows):
            self.write_at(self.GALLOWS_TOP + i, self.GALLOWS_LEFT, f"{body_color}{line}{RESET}")

    def _draw_word(self, game):
        """Draw the word display area."""
        word_display = game.display_word()
        # Clear old word area (word length can vary between games)
        clear_w = len(game.word) * 2 + 5  # max visible width + box padding
        self.write_at(self.WORD_ROW,     self.WORD_LEFT, " " * (clear_w + 2))
        self.write_at(self.WORD_ROW + 1, self.WORD_LEFT, " " * (clear_w + 2))
        self.write_at(self.WORD_ROW + 2, self.WORD_LEFT, " " * (clear_w + 2))
        # Draw box
        self.write_at(self.WORD_ROW,     self.WORD_LEFT, f"{FG_CYAN}{BOLD}┌{'─' * (len(word_display) + 4)}┐{RESET}")
        self.write_at(self.WORD_ROW + 1, self.WORD_LEFT, f"{FG_CYAN}{BOLD}│  {RESET}{FG_WHITE}{BOLD}{word_display}{RESET}{FG_CYAN}{BOLD}  │{RESET}")
        self.write_at(self.WORD_ROW + 2, self.WORD_LEFT, f"{FG_CYAN}{BOLD}└{'─' * (len(word_display) + 4)}┘{RESET}")

    def _draw_wrong_and_lives(self, game):
        """Draw wrong letters list and lives display."""
        # Clear line then draw
        self.write_at(self.WRONG_TOP, self.WRONG_LEFT, " " * 36)
        wrong_letters = [c for c in sorted(game.guessed) if c not in game.word]
        if wrong_letters:
            self.write_at(self.WRONG_TOP, self.WRONG_LEFT, f"{FG_RED}Wrong: {' '.join(wrong_letters)}{RESET}")
        # Lives
        self.write_at(self.WRONG_TOP + 1, self.WRONG_LEFT, " " * 20)
        lives = MAX_WRONG - game.wrong
        lives_str = "♥ " * lives + "♡ " * game.wrong
        self.write_at(self.WRONG_TOP + 1, self.WRONG_LEFT, f"{FG_RED}{lives_str}{RESET}")

    def _draw_message(self, game):
        """Draw the message and prompt area."""
        # Clear message line
        self.write_at(self.MSG_ROW, 4, " " * 60)
        if game.message:
            if game.game_over and game.won:
                color = f"{FG_GREEN}{BOLD}"
            elif game.game_over and not game.won:
                color = f"{FG_RED}{BOLD}"
            else:
                color = f"{FG_YELLOW}"
            self.write_at(self.MSG_ROW, 4, f"{color}{game.message}{RESET}")

        # Clear and draw prompt
        self.write_at(self.PROMPT_ROW, 4, " " * 50)
        if game.game_over:
            self.write_at(self.PROMPT_ROW, 4, f"{FG_CYAN}{BOLD}Press ENTER for new game, ESC to quit{RESET}")
        else:
            self.write_at(self.PROMPT_ROW, 4, f"{FG_CYAN}Guess a letter: {RESET}")

    def _draw_score(self, game):
        """Draw the score bar."""
        score = f"  Wins: {game.wins}  │  Losses: {game.losses}  │  Press ESC to quit  "
        self.write_at(1, 0, f"{BG_BLACK}{FG_CYAN}{score}{' ' * (COLS - len(score))}{RESET}")

    def _position_cursor(self, game):
        """Move cursor to the appropriate input position."""
        if not game.game_over:
            self.writer(f"{CSI}{self.PROMPT_ROW + 1};21H")
        else:
            self.writer(f"{CSI}{self.PROMPT_ROW + 1};44H")

    def render_full(self, game):
        """Full screen render — called once at game start."""
        self.writer(f"{CSI}2J")   # Clear screen
        self.writer(f"{CSI}H")    # Home cursor

        # ── Title bar ──
        title = " HANGMAN "
        pad = (COLS - len(title)) // 2
        self.write_at(0, 0, f"{BG_BLUE}{FG_WHITE}{BOLD}{'═' * pad}{title}{'═' * (COLS - pad - len(title))}{RESET}")

        # ── Score bar ──
        self._draw_score(game)

        # ── Gallows box border ──
        bl = self.GALLOWS_BOX_LEFT
        bt = self.GALLOWS_BOX_TOP
        bw = self.GALLOWS_BOX_W
        bh = self.GALLOWS_BOX_H
        self.write_at(bt, bl, f"{FG_YELLOW}╔{'═' * (bw - 2)}╗{RESET}")
        for i in range(1, bh - 1):
            self.write_at(bt + i, bl, f"{FG_YELLOW}║{RESET}")
            self.write_at(bt + i, bl + bw - 1, f"{FG_YELLOW}║{RESET}")
        self.write_at(bt + bh - 1, bl, f"{FG_YELLOW}╚{'═' * (bw - 2)}╝{RESET}")

        # ── Gallows ──
        self._draw_gallows(game)

        # ── Word display ──
        self._draw_word(game)
        hint = f"( {len(game.word)} letters )"
        self.write_at(self.WORD_ROW + 3, self.WORD_LEFT + 2, f"{DIM}{FG_WHITE}{hint}{RESET}")

        # ── Letter board frame ──
        iw = self.inner_w
        bl = self.BOARD_LEFT
        bt = self.BOARD_TOP
        self.write_at(bt - 1, bl, f"{FG_YELLOW}{BOLD}╔{'═' * iw}╗{RESET}")
        title_text = "LETTERS"
        tpl = (iw - len(title_text)) // 2
        tpr = iw - len(title_text) - tpl
        self.write_at(bt, bl, f"{FG_YELLOW}{BOLD}║{RESET}{' ' * tpl}{FG_WHITE}{BOLD}{title_text}{RESET}{' ' * tpr}{FG_YELLOW}{BOLD}║{RESET}")
        self.write_at(bt + 1, bl, f"{FG_YELLOW}{BOLD}╠{'═' * iw}╣{RESET}")

        # Side borders and separators for letter grid
        row_idx = bt + 2
        for start in range(0, 26, self.LETTERS_PER_ROW):
            for sub_row in range(3):
                self.write_at(row_idx + sub_row, bl, f"{FG_YELLOW}{BOLD}║{RESET}")
                self.write_at(row_idx + sub_row, bl + iw + 1, f"{FG_YELLOW}{BOLD}║{RESET}")
            row_idx += 3
            if start + self.LETTERS_PER_ROW < 26:
                self.write_at(row_idx, bl, f"{FG_YELLOW}{BOLD}║{RESET}{' ' * iw}{FG_YELLOW}{BOLD}║{RESET}")
                row_idx += 1
        self.write_at(row_idx, bl, f"{FG_YELLOW}{BOLD}╚{'═' * iw}╝{RESET}")

        # ── All letter cells ──
        for ch in "ABCDEFGHIJKLMNOPQRSTUVWXYZ":
            self._draw_letter_cell(ch, game)

        # ── Dynamic areas ──
        self._draw_wrong_and_lives(game)
        self._draw_message(game)

        # ── Bottom bar ──
        bottom = f"  FPGA Terminal Hangman  │  142×45  │  JZ-HDL  "
        self.write_at(ROWS - 1, 0, f"{BG_BLUE}{FG_WHITE}{bottom}{' ' * (COLS - len(bottom))}{RESET}")

        self._position_cursor(game)

    def render_update(self, game, guessed_letter=None):
        """Incremental update — only redraws changed parts."""
        # Update the letter cell that was just guessed
        if guessed_letter:
            self._draw_letter_cell(guessed_letter.upper(), game)

        # Update gallows (always, in case wrong count changed)
        self._draw_gallows(game)

        # Update word display
        self._draw_word(game)

        # Update wrong letters & lives
        self._draw_wrong_and_lives(game)

        # Update score (in case win/loss changed)
        self._draw_score(game)

        # Update message and prompt
        self._draw_message(game)

        # Reposition cursor
        self._position_cursor(game)


def run_serial(device, baud, config_str):
    """Run game over serial connection to FPGA terminal."""
    if not HAS_SERIAL:
        print("Error: pyserial is required. Install with: pip install pyserial")
        sys.exit(1)

    parity_map = {'N': serial.PARITY_NONE, 'E': serial.PARITY_EVEN, 'O': serial.PARITY_ODD}

    if len(config_str) != 3:
        print(f"Config must be 3 chars (e.g. N81), got: {config_str!r}")
        sys.exit(1)

    parity = parity_map.get(config_str[0].upper())
    if parity is None:
        print(f"Parity must be N, E, or O")
        sys.exit(1)
    databits = int(config_str[1])
    stopbits = int(config_str[2])

    try:
        ser = serial.Serial(
            port=device,
            baudrate=baud,
            bytesize=databits,
            parity=parity,
            stopbits=stopbits,
            timeout=0.1,
        )
    except serial.SerialException as e:
        print(f"Error opening {device}: {e}")
        sys.exit(1)

    def writer(data):
        ser.write(data.encode('utf-8'))
        time.sleep(0.005)  # small delay for UART flow

    game = HangmanGame()
    renderer = Renderer(writer)

    # Set up local raw terminal for keyboard input
    old_settings = None
    fd = sys.stdin.fileno()
    if termios:
        old_settings = termios.tcgetattr(fd)
        tty.setraw(fd)

    try:
        # Disable auto-wrap, hide cursor, clear screen
        writer(f"{CSI}?7l")    # Auto Wrap OFF
        writer(f"{CSI}?25l")   # Hide cursor
        renderer.render_full(game)
        while True:
            byte = os.read(fd, 1)
            if not byte:
                break
            ch = byte[0]

            if ch == 0x1B:  # ESC
                break
            elif ch == 0x03:  # Ctrl+C
                break
            elif ch in (0x0D, 0x0A):  # Enter
                if game.game_over:
                    game.new_game()
                    renderer.render_full(game)
            elif 0x41 <= ch <= 0x5A or 0x61 <= ch <= 0x7A:  # A-Z or a-z
                if not game.game_over:
                    letter = chr(ch)
                    game.guess(letter)
                    renderer.render_update(game, letter)
    finally:
        # Restore FPGA terminal
        writer(f"{CSI}?7h")    # Auto Wrap ON
        writer(f"{CSI}?25h")   # Show cursor
        writer(f"{CSI}0m")     # Reset attributes
        writer(f"{CSI}2J{CSI}H")  # Clear screen
        ser.close()
        if old_settings and termios:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        print("\nThanks for playing!")


def run_local():
    """Run game locally in the current terminal."""
    def writer(data):
        sys.stdout.write(data)
        sys.stdout.flush()

    game = HangmanGame()
    renderer = Renderer(writer)

    # Enter alternate screen buffer
    writer(f"{CSI}?1049h")
    writer(f"{CSI}?7l")    # Auto Wrap OFF
    writer(f"{CSI}?25l")   # hide cursor

    old_settings = None
    fd = sys.stdin.fileno()
    if termios:
        old_settings = termios.tcgetattr(fd)
        tty.setraw(fd)

    try:
        renderer.render_full(game)
        while True:
            byte = os.read(fd, 1)
            if not byte:
                break
            ch = byte[0]

            if ch == 0x1B:  # ESC
                break
            elif ch == 0x03:  # Ctrl+C
                break
            elif ch in (0x0D, 0x0A):  # Enter
                if game.game_over:
                    game.new_game()
                    renderer.render_full(game)
            elif 0x41 <= ch <= 0x5A or 0x61 <= ch <= 0x7A:  # A-Z or a-z
                if not game.game_over:
                    letter = chr(ch)
                    game.guess(letter)
                    renderer.render_update(game, letter)
    finally:
        if old_settings and termios:
            termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
        writer(f"{CSI}?25h")    # show cursor
        writer(f"{CSI}?1049l")  # leave alternate screen
        print("\nThanks for playing!")


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
            print("Usage: hangman.py --local              (play locally)")
            print("       hangman.py <device> <baud> <config>  (play over serial)")
            print("\nNote: pyserial not installed, serial mode unavailable")
        return

    if args[0] == '--local':
        run_local()
        return

    if len(args) != 3:
        print("Usage: hangman.py <device> <baud> <config>")
        print("       hangman.py --local")
        print("\nExample: hangman.py /dev/cu.usbserial-1420 115200 N81")
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
