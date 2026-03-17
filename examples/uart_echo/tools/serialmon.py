#!/usr/bin/env python3
"""Serial monitor for FPGA UART testing.

Usage:
    serialmon.py                                  List available serial ports
    serialmon.py <device> <baud> <config>         Simple mode (raw passthrough)
    serialmon.py -fs <device> <baud> <config>     Full-screen mode with status bar
    serialmon.py -hex <device> <baud> <config>    Hex dump mode (RX only)
    serialmon.py -debug <device> <baud> <config>  Debug mode (verbose diagnostics)
    serialmon.py -sweep <device> <config>         Baud rate sweep (auto-detect)

Config format: [parity][databits][stopbits]
    Parity:    N=none, E=even, O=odd
    Data bits: 5, 6, 7, 8
    Stop bits: 1, 2
    Example:   N81 = No parity, 8 data bits, 1 stop bit
"""

import sys
import os
import time
import threading
import atexit
import signal

try:
    import serial
    import serial.tools.list_ports
except ImportError:
    print("Error: pyserial is required. Install with: pip install pyserial")
    sys.exit(1)

# Only available on Unix
try:
    import tty
    import termios
except ImportError:
    tty = None
    termios = None


def list_ports():
    """List available serial ports."""
    ports = serial.tools.list_ports.comports()
    if not ports:
        print("No serial ports found.")
        return
    print(f"{'Device':<30} {'Description':<40} {'HWID'}")
    print("-" * 100)
    for p in sorted(ports, key=lambda x: x.device):
        print(f"{p.device:<30} {p.description:<40} {p.hwid}")


def parse_config(config_str):
    """Parse config string like 'N81' into (parity, databits, stopbits)."""
    if len(config_str) != 3:
        raise ValueError(f"Config must be 3 characters (e.g. N81), got: {config_str!r}")

    parity_char = config_str[0].upper()
    databits_char = config_str[1]
    stopbits_char = config_str[2]

    parity_map = {
        'N': serial.PARITY_NONE,
        'E': serial.PARITY_EVEN,
        'O': serial.PARITY_ODD,
    }
    if parity_char not in parity_map:
        raise ValueError(f"Parity must be N, E, or O, got: {parity_char!r}")
    parity = parity_map[parity_char]

    if databits_char not in '5678':
        raise ValueError(f"Data bits must be 5-8, got: {databits_char!r}")
    databits = int(databits_char)

    if stopbits_char not in '12':
        raise ValueError(f"Stop bits must be 1 or 2, got: {stopbits_char!r}")
    stopbits = int(stopbits_char)

    return parity, databits, stopbits


class TerminalRawMode:
    """Context manager for raw terminal mode with cleanup."""

    def __init__(self):
        self._old_settings = None
        self._fd = None

    def enter(self):
        if termios is None:
            return
        self._fd = sys.stdin.fileno()
        self._old_settings = termios.tcgetattr(self._fd)
        tty.setraw(self._fd)
        atexit.register(self.restore)

    def restore(self):
        if self._old_settings is not None and self._fd is not None:
            termios.tcsetattr(self._fd, termios.TCSADRAIN, self._old_settings)
            self._old_settings = None


class DCSFilter:
    """Filters DCS (Device Control String) sequences from a byte stream.

    Collects ESC P ... ESC \\ sequences and formats them for display
    instead of passing them raw to the terminal (which would silently
    consume them).
    """

    ST_NORMAL = 0
    ST_SAW_ESC = 1
    ST_IN_DCS = 2
    ST_DCS_SAW_ESC = 3

    EDID_FIELDS = {
        '1': 'Monitor Name (FC)',
        '2': 'Serial Number (FF)',
        '3': 'Unspecified Text (FE)',
        '4': 'Physical Size',
    }

    def __init__(self):
        self.state = self.ST_NORMAL
        self.dcs_buf = bytearray()

    def feed(self, data):
        """Process incoming bytes. Returns (passthrough, dcs_messages).

        passthrough: bytes to display normally
        dcs_messages: list of formatted DCS response strings
        """
        passthrough = bytearray()
        messages = []

        for b in data:
            if self.state == self.ST_NORMAL:
                if b == 0x1B:
                    self.state = self.ST_SAW_ESC
                else:
                    passthrough.append(b)

            elif self.state == self.ST_SAW_ESC:
                if b == 0x50:  # 'P' — DCS start
                    self.state = self.ST_IN_DCS
                    self.dcs_buf.clear()
                else:
                    # Not DCS — pass ESC + byte through
                    passthrough.append(0x1B)
                    passthrough.append(b)
                    self.state = self.ST_NORMAL

            elif self.state == self.ST_IN_DCS:
                if b == 0x1B:
                    self.state = self.ST_DCS_SAW_ESC
                else:
                    self.dcs_buf.append(b)

            elif self.state == self.ST_DCS_SAW_ESC:
                if b == 0x5C:  # '\\' — ST (String Terminator)
                    msg = self._format_dcs(bytes(self.dcs_buf))
                    if msg:
                        messages.append(msg)
                    self.dcs_buf.clear()
                    self.state = self.ST_NORMAL
                else:
                    # Malformed — dump as passthrough
                    passthrough.append(0x1B)
                    passthrough.append(0x50)
                    passthrough.extend(self.dcs_buf)
                    passthrough.append(0x1B)
                    passthrough.append(b)
                    self.dcs_buf.clear()
                    self.state = self.ST_NORMAL

        return bytes(passthrough), messages

    def _format_dcs(self, content):
        """Format a DCS content buffer into a readable string."""
        try:
            s = content.decode('ascii')
        except UnicodeDecodeError:
            return f"DCS: {content.hex()}"

        # EDID response: <digit>r45444944=<data>
        if len(s) >= 12 and s[1] == 'r' and '=' in s:
            field_id = s[0]
            eq_pos = s.index('=')
            register = s[2:eq_pos]
            data = s[eq_pos + 1:]

            field_name = self.EDID_FIELDS.get(field_id, f'Field {field_id}')
            if register == '45444944':
                return f"[EDID] {field_name}: {data}"
            return f"[DCS] {field_id}r {register}={data}"

        # Version response: >|<text>
        if s.startswith('>|'):
            return f"[Version] {s[2:]}"

        return f"[DCS] {s}"


class SimpleMonitor:
    """Raw passthrough serial monitor."""

    def __init__(self, ser):
        self.ser = ser
        self.tx_count = 0
        self.rx_count = 0
        self.running = True
        self.dcs_filter = DCSFilter()

    def reader_thread(self):
        """Read from serial port, write to stdout."""
        while self.running:
            try:
                data = self.ser.read(self.ser.in_waiting or 1)
                if data:
                    self.rx_count += len(data)
                    passthrough, dcs_msgs = self.dcs_filter.feed(data)
                    if passthrough:
                        # Translate \n to \r\n for raw terminal mode
                        display = passthrough.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
                        os.write(sys.stdout.fileno(), display)
                    for msg in dcs_msgs:
                        os.write(sys.stdout.fileno(), (f"\r\n{msg}\r\n").encode())
            except (serial.SerialException, OSError):
                if self.running:
                    os.write(sys.stdout.fileno(), b"\r\n[Serial port disconnected]\r\n")
                    self.running = False
                break

    def run(self):
        """Main loop: read stdin, write to serial."""
        reader = threading.Thread(target=self.reader_thread, daemon=True)
        reader.start()

        try:
            while self.running:
                byte = os.read(sys.stdin.fileno(), 1)
                if not byte:
                    break
                if byte == b'\x03':  # Ctrl+C
                    break
                self.ser.write(byte)
                self.tx_count += 1
        except OSError:
            pass

        self.running = False


class HexMonitor:
    """Hex dump serial monitor. Shows RX bytes in hex + ASCII."""

    def __init__(self, ser):
        self.ser = ser
        self.tx_count = 0
        self.rx_count = 0
        self.running = True
        self.line_buf = bytearray()
        self.offset = 0

    def _flush_line(self):
        """Print a hex dump line (16 bytes per line)."""
        if not self.line_buf:
            return
        # Offset
        parts = [f"{self.offset:08x}  "]
        # Hex bytes
        hex_parts = []
        for i in range(16):
            if i < len(self.line_buf):
                hex_parts.append(f"{self.line_buf[i]:02x}")
            else:
                hex_parts.append("  ")
            if i == 7:
                hex_parts.append("")  # extra space at midpoint
        parts.append(" ".join(hex_parts))
        # ASCII
        parts.append("  |")
        for b in self.line_buf:
            if 0x20 <= b <= 0x7e:
                parts.append(chr(b))
            else:
                parts.append(".")
        parts.append("|")
        line = "".join(parts)
        os.write(sys.stdout.fileno(), (line + "\r\n").encode())
        self.offset += len(self.line_buf)
        self.line_buf.clear()

    def reader_thread(self):
        """Read from serial port, display as hex dump."""
        while self.running:
            try:
                data = self.ser.read(self.ser.in_waiting or 1)
                if data:
                    self.rx_count += len(data)
                    for b in data:
                        self.line_buf.append(b)
                        if len(self.line_buf) == 16:
                            self._flush_line()
            except (serial.SerialException, OSError):
                if self.running:
                    self._flush_line()
                    os.write(sys.stdout.fileno(), b"\r\n[Serial port disconnected]\r\n")
                    self.running = False
                break

    def run(self):
        """Main loop: read stdin for keypresses, write to serial."""
        reader = threading.Thread(target=self.reader_thread, daemon=True)
        reader.start()

        try:
            while self.running:
                byte = os.read(sys.stdin.fileno(), 1)
                if not byte:
                    break
                if byte == b'\x03':  # Ctrl+C
                    break
                self.ser.write(byte)
                self.tx_count += 1
        except OSError:
            pass

        self.running = False
        # Flush any partial line
        self._flush_line()


class FullScreenMonitor:
    """Full-screen serial monitor with status bar."""

    def __init__(self, ser, device, baud, config_str):
        self.ser = ser
        self.device = device
        self.baud = baud
        self.config_str = config_str.upper()
        self.tx_count = 0
        self.rx_count = 0
        self.running = True
        self.start_time = time.time()
        self._lock = threading.Lock()
        self.dcs_filter = DCSFilter()

    def _write(self, data):
        """Write bytes to stdout."""
        if isinstance(data, str):
            data = data.encode()
        os.write(sys.stdout.fileno(), data)

    def _enter_fullscreen(self):
        """Set up full-screen terminal."""
        self._write("\033[?1049h")  # Alternate screen buffer
        self._write("\033[?25l")    # Hide cursor
        self._write("\033[2J")      # Clear screen
        self._draw_status()
        self._write("\033[2;999r")  # Scroll region: line 2 to bottom
        self._write("\033[2;1H")    # Move cursor to line 2

    def _leave_fullscreen(self):
        """Restore terminal."""
        self._write("\033[r")       # Reset scroll region
        self._write("\033[?25h")    # Show cursor
        self._write("\033[?1049l")  # Leave alternate screen

    def _draw_status(self):
        """Draw the status bar on line 1."""
        elapsed = int(time.time() - self.start_time)
        h = elapsed // 3600
        m = (elapsed % 3600) // 60
        s = elapsed % 60
        timestamp = f"{h:02d}:{m:02d}:{s:02d}"

        status = (f" {self.device} | {self.baud} {self.config_str} | "
                  f"TX: {self.tx_count}  RX: {self.rx_count} | "
                  f"connected {timestamp} ")

        # Get terminal width
        try:
            cols = os.get_terminal_size().columns
        except OSError:
            cols = 80

        # Pad/truncate to terminal width
        status = status.ljust(cols)[:cols]

        with self._lock:
            # Save cursor position, draw status bar, restore cursor
            self._write("\033[s")       # Save cursor
            self._write("\033[1;1H")    # Move to line 1
            self._write("\033[7m")      # Reverse video
            self._write(status)
            self._write("\033[0m")      # Reset attributes
            self._write("\033[u")       # Restore cursor

    def reader_thread(self):
        """Read from serial port, display in output area."""
        while self.running:
            try:
                data = self.ser.read(self.ser.in_waiting or 1)
                if data:
                    self.rx_count += len(data)
                    passthrough, dcs_msgs = self.dcs_filter.feed(data)
                    if passthrough:
                        # Translate \n to \r\n for raw terminal
                        display = passthrough.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n')
                        with self._lock:
                            self._write(display)
                    for msg in dcs_msgs:
                        with self._lock:
                            self._write(f"\r\n{msg}\r\n")
                    self._draw_status()
            except (serial.SerialException, OSError):
                if self.running:
                    with self._lock:
                        self._write(b"\r\n[Serial port disconnected]\r\n")
                    self.running = False
                break

    def status_thread(self):
        """Periodically refresh the status bar for the elapsed timer."""
        while self.running:
            time.sleep(1)
            if self.running:
                self._draw_status()

    def run(self):
        """Main loop."""
        self._enter_fullscreen()
        atexit.register(self._leave_fullscreen)

        reader = threading.Thread(target=self.reader_thread, daemon=True)
        reader.start()

        status_updater = threading.Thread(target=self.status_thread, daemon=True)
        status_updater.start()

        try:
            while self.running:
                byte = os.read(sys.stdin.fileno(), 1)
                if not byte:
                    break
                if byte == b'\x03':  # Ctrl+C
                    break
                self.ser.write(byte)
                self.tx_count += 1
                self._draw_status()
        except OSError:
            pass

        self.running = False
        self._leave_fullscreen()


class DebugMonitor:
    """Debug serial monitor with verbose diagnostics."""

    def __init__(self, ser, device, baud, config_str):
        self.ser = ser
        self.device = device
        self.baud = baud
        self.config_str = config_str.upper()
        self.rx_count = 0
        self.running = True
        self.start_time = time.time()
        self.last_rx_time = None
        self.byte_times = []

    def _log(self, msg):
        """Print a timestamped debug message."""
        elapsed = time.time() - self.start_time
        print(f"[{elapsed:10.3f}] {msg}", flush=True)

    def _print_port_info(self):
        """Print detailed port configuration."""
        ser = self.ser
        print("=" * 60)
        print(f"  Device:     {self.device}")
        print(f"  Baud rate:  {ser.baudrate}")
        print(f"  Data bits:  {ser.bytesize}")
        parity_names = {serial.PARITY_NONE: 'None', serial.PARITY_EVEN: 'Even',
                        serial.PARITY_ODD: 'Odd'}
        print(f"  Parity:     {parity_names.get(ser.parity, ser.parity)}")
        print(f"  Stop bits:  {ser.stopbits}")
        print(f"  Timeout:    {ser.timeout}s")
        print(f"  RTS/CTS:    {ser.rtscts}")
        print(f"  DSR/DTR:    {ser.dsrdtr}")
        print(f"  XON/XOFF:   {ser.xonxoff}")
        print(f"  Bit time:   {1000.0/ser.baudrate:.3f} ms")
        print(f"  Char time:  {(1 + ser.bytesize + ser.stopbits) * 1000.0 / ser.baudrate:.3f} ms (start+data+stop)")
        print()
        # Line states
        try:
            print(f"  CTS:  {ser.cts}")
            print(f"  DSR:  {ser.dsr}")
            print(f"  RI:   {ser.ri}")
            print(f"  CD:   {ser.cd}")
        except (serial.SerialException, OSError) as e:
            print(f"  Line state read error: {e}")
        print("=" * 60)
        print()

    def reader_thread(self):
        """Read from serial port with verbose diagnostics."""
        wait_counter = 0
        while self.running:
            try:
                waiting = self.ser.in_waiting
                if waiting > 0:
                    data = self.ser.read(waiting)
                    if data:
                        now = time.time()
                        dt_ms = (now - self.last_rx_time) * 1000.0 if self.last_rx_time else 0.0
                        self.last_rx_time = now
                        self.rx_count += len(data)

                        # Show hex and ASCII for each byte
                        hex_str = ' '.join(f'{b:02x}' for b in data)
                        ascii_str = ''.join(chr(b) if 0x20 <= b <= 0x7e else '.' for b in data)

                        if dt_ms > 0:
                            self._log(f"RX [{len(data):3d} bytes] gap={dt_ms:8.1f}ms  hex: {hex_str}  ascii: |{ascii_str}|  total: {self.rx_count}")
                        else:
                            self._log(f"RX [{len(data):3d} bytes] (first)          hex: {hex_str}  ascii: |{ascii_str}|  total: {self.rx_count}")

                        # Track inter-byte timing for baud analysis
                        if len(data) >= 2:
                            char_time_ms = (1 + self.ser.bytesize + self.ser.stopbits) * 1000.0 / self.ser.baudrate
                            self._log(f"    Expected char time at {self.baud} baud: {char_time_ms:.1f}ms")

                        wait_counter = 0
                else:
                    data = self.ser.read(1)  # blocking read with timeout
                    if data:
                        now = time.time()
                        dt_ms = (now - self.last_rx_time) * 1000.0 if self.last_rx_time else 0.0
                        self.last_rx_time = now
                        self.rx_count += len(data)

                        hex_str = ' '.join(f'{b:02x}' for b in data)
                        ascii_str = ''.join(chr(b) if 0x20 <= b <= 0x7e else '.' for b in data)

                        if dt_ms > 0:
                            self._log(f"RX [{len(data):3d} bytes] gap={dt_ms:8.1f}ms  hex: {hex_str}  ascii: |{ascii_str}|  total: {self.rx_count}")
                        else:
                            self._log(f"RX [{len(data):3d} bytes] (first)          hex: {hex_str}  ascii: |{ascii_str}|  total: {self.rx_count}")

                        wait_counter = 0
                    else:
                        # Timeout, no data
                        wait_counter += 1
                        if wait_counter % 50 == 1:  # every ~5 seconds
                            self._log(f"... waiting (no data received, {self.rx_count} total bytes so far)")
                            # Re-check line states periodically
                            try:
                                cts = self.ser.cts
                                dsr = self.ser.dsr
                                self._log(f"    Line states: CTS={cts} DSR={dsr}")
                            except (serial.SerialException, OSError):
                                pass

            except (serial.SerialException, OSError) as e:
                if self.running:
                    self._log(f"ERROR: {e}")
                    self.running = False
                break

    def run(self):
        """Main loop for debug mode."""
        self._print_port_info()
        self._log("Debug monitor started. Press Ctrl+C to exit.")
        self._log(f"Listening for data at {self.baud} baud...")
        print()

        reader = threading.Thread(target=self.reader_thread, daemon=True)
        reader.start()

        try:
            while self.running:
                time.sleep(0.1)
        except KeyboardInterrupt:
            pass

        self.running = False
        print()
        self._log(f"Exiting. Total RX: {self.rx_count} bytes")


class BaudSweepMonitor:
    """Sweep common baud rates to auto-detect what the FPGA is sending."""

    BAUD_RATES = [
        300, 600, 1200, 2400, 4800, 9600, 14400, 19200,
        28800, 38400, 57600, 76800, 115200, 230400, 460800, 921600,
        # Non-standard rates that might result from PLL misconfiguration
        150, 225, 450, 900, 1800, 3600, 7200, 10800, 21600, 43200,
    ]

    def __init__(self, device, config_str):
        self.device = device
        self.config_str = config_str
        self.parity, self.databits, self.stopbits = parse_config(config_str)

    def _test_baud(self, baud, duration=2.0):
        """Test a baud rate for the given duration. Returns (bytes_received, data)."""
        try:
            ser = serial.Serial(
                port=self.device,
                baudrate=baud,
                bytesize=self.databits,
                parity=self.parity,
                stopbits=self.stopbits,
                timeout=0.2,
            )
        except serial.SerialException:
            return 0, b'', False

        # Flush any stale data
        ser.reset_input_buffer()
        time.sleep(0.1)
        ser.reset_input_buffer()

        collected = bytearray()
        start = time.time()
        while time.time() - start < duration:
            try:
                data = ser.read(ser.in_waiting or 1)
                if data:
                    collected.extend(data)
            except (serial.SerialException, OSError):
                ser.close()
                return len(collected), bytes(collected), False

        ser.close()
        return len(collected), bytes(collected), True

    def _analyze_data(self, data):
        """Analyze received data for printable content and patterns."""
        if not data:
            return "no data"

        printable = sum(1 for b in data if 0x20 <= b <= 0x7e)
        pct = 100.0 * printable / len(data) if data else 0

        # Check for repeating byte patterns
        unique = set(data)
        repeat_info = ""
        if len(unique) <= 3:
            counts = {b: data.count(b) for b in unique}
            parts = [f"0x{b:02x}{'=' + repr(chr(b)) if 0x20 <= b <= 0x7e else ''}:{c}" for b, c in sorted(counts.items())]
            repeat_info = f"  values: {', '.join(parts)}"

        ascii_preview = ''.join(chr(b) if 0x20 <= b <= 0x7e else '.' for b in data[:32])

        return f"{printable}/{len(data)} printable ({pct:.0f}%)  |{ascii_preview}|{repeat_info}"

    def run(self):
        """Sweep all baud rates."""
        print(f"Baud rate sweep on {self.device} ({self.config_str})")
        print(f"Testing {len(self.BAUD_RATES)} baud rates, 2s each...")
        print("=" * 80)
        print()

        results = []
        sorted_bauds = sorted(self.BAUD_RATES)

        for i, baud in enumerate(sorted_bauds):
            sys.stdout.write(f"\r  [{i+1}/{len(sorted_bauds)}] Testing {baud:>7d} baud... ")
            sys.stdout.flush()

            count, data, ok = self._test_baud(baud)

            if not ok:
                print(f"ERROR (port issue)")
                continue

            analysis = self._analyze_data(data)

            if count > 0:
                print(f"{count:4d} bytes  {analysis}")
                results.append((baud, count, data))
            else:
                print(f"   0 bytes")

        print()
        print("=" * 80)

        if not results:
            print("No data received at any baud rate!")
            print("Check: physical connection, pin assignments, FPGA bitstream loaded")
            return

        # Sort by printable character percentage (best match)
        def score(entry):
            baud, count, data = entry
            printable = sum(1 for b in data if 0x20 <= b <= 0x7e)
            # High printable % and reasonable byte count = likely correct baud
            return (printable / max(len(data), 1), count)

        results.sort(key=score, reverse=True)
        print("\nBest candidates (by printable content):")
        for baud, count, data in results[:5]:
            printable = sum(1 for b in data if 0x20 <= b <= 0x7e)
            pct = 100.0 * printable / len(data) if data else 0
            ascii_preview = ''.join(chr(b) if 0x20 <= b <= 0x7e else '.' for b in data[:40])
            print(f"  {baud:>7d} baud: {count:4d} bytes, {pct:.0f}% printable  |{ascii_preview}|")


def main():
    args = sys.argv[1:]

    # No args: list ports
    if not args:
        list_ports()
        return

    # Parse mode flag
    mode = 'simple'
    if args[0] == '-fs':
        mode = 'fullscreen'
        args = args[1:]
    elif args[0] == '-hex':
        mode = 'hex'
        args = args[1:]
    elif args[0] == '-debug':
        mode = 'debug'
        args = args[1:]
    elif args[0] == '-sweep':
        mode = 'sweep'
        args = args[1:]

    if mode == 'sweep':
        if len(args) != 2:
            print("Usage: serialmon.py -sweep <device> <config>")
            print("Example: serialmon.py -sweep /dev/cu.usbserial-1420 N81")
            sys.exit(1)
        device = args[0]
        config_str = args[1]
        try:
            parse_config(config_str)
        except ValueError as e:
            print(f"Error: {e}")
            sys.exit(1)
        sweeper = BaudSweepMonitor(device, config_str)
        sweeper.run()
        return

    if len(args) != 3:
        print("Usage: serialmon.py [-fs|-hex|-debug|-sweep] <device> <baud> <config>")
        print("       serialmon.py                  (list ports)")
        print()
        print("Modes:")
        print("  (default)  Raw passthrough -- RX displayed as text")
        print("  -fs        Full-screen with status bar")
        print("  -hex       Hex dump -- shows RX bytes in hex + ASCII")
        print("  -debug     Debug mode -- verbose diagnostics, timing, line state")
        print("  -sweep     Baud rate sweep -- tries common baud rates to find data")
        print()
        print("Example: serialmon.py /dev/cu.usbserial-1420 115200 N81")
        print("         serialmon.py -debug /dev/cu.usbserial-1420 300 N81")
        print("         serialmon.py -sweep /dev/cu.usbserial-1420 N81")
        sys.exit(1)

    device = args[0]
    try:
        baud = int(args[1])
    except ValueError:
        print(f"Error: baud rate must be an integer, got: {args[1]!r}")
        sys.exit(1)

    config_str = args[2]
    try:
        parity, databits, stopbits = parse_config(config_str)
    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)

    # Open serial port
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

    if mode == 'debug':
        # Debug mode doesn't use raw terminal
        try:
            monitor = DebugMonitor(ser, device, baud, config_str)
            monitor.run()
        finally:
            ser.close()
        return

    # Set up raw terminal mode
    raw_mode = TerminalRawMode()
    raw_mode.enter()

    try:
        if mode == 'fullscreen':
            monitor = FullScreenMonitor(ser, device, baud, config_str)
        elif mode == 'hex':
            monitor = HexMonitor(ser)
        else:
            monitor = SimpleMonitor(ser)
        monitor.run()
    finally:
        ser.close()
        raw_mode.restore()

    # Print final newline after raw mode is restored
    if mode != 'fullscreen':
        print()


if __name__ == '__main__':
    main()
