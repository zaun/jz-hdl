1. IRQ Vector Table Setup
Initialize mtvec, setup the trap, check if its a known IRQ (uart or sdcart) and just the irqvec and sdcardvec CSR if not 0. If its an unknow IRQ (a crash) show a blue screen with mepc/mcause/mstatus displayed then halt.

2. Hardware Init & Self-Test (POST)
Initialize terminal, initialize SDRAM, configure baud rate, blink an LED pattern to signal life. Without this, nothing else works. A simple LED progress code (LED 0 = start, LED 1 = SDRAM ok, etc.) is invaluable for debugging a board that won't boot.
Should display status on the terminal and uart
- Line 1: JZ-HSL SOC - RV32I @ <clock_speed_from_csr>MHz
- Line 2: Video output resolution, terminal size
- Line 3: SDRam init update the current % done. Shoud Write then Read each word.
- Line 4: List initial UART Settings
- Line 5: List SDCard status

3. Wait 2 seconds for ESC input over the UART to enter BIOS.

4. If no input, just to a green screen that says "System up. No kernel. Rset to try again." or something similar

5. If entered BIOS, just show a yellow screen with "Bios" on it.

Update BIOS to be a shell rather than a yellow screen...
Should be a blue screen, white text.
  - ? - print a help screent including the current version.
  - v <mode> - 0 or 1 to change video mode
  - b - list supported baud rates
  - b <baud> - set the baud rate
  - r <addr> — read memory word
  - w <addr> <data> — write memory word
  - j <addr> — jump to address
  - d <addr> <len> — hex dump
  - rd - dump all registers (including CSRs)
  - p - list partisions on sd card
  - p <num> - select partition
  - pwd - print current path
  - cd <path> - move into a path (should support dir/dir/dir, does not need to support inline ..)
  - cd .. - move "up" one path
  - cd / - move to root path
  - ls - list current path

Provide ECALLs:

ECALL 0x01: tty_read (uart only)
  a0 = ptr buffer
  a1 = length
  returns: a0 = bytes_read or negative error

ECALL 0x02: tty_size (not sure how to get the uart size)
  a0 = device 0 = none, 1 = terminal, 3 = uart
  returns: a0 = rows, a1 = cols

ECALL 0x03: tty_write
  a0 = device 0 = none, 1 = terminal, 3 = uart, 4 = both
  a1 = ptr to buffer
  a0 = length
  returns: a0 = bytes_written (>=0) or negative error

ECALL 0x04: tty_clear (uart should send ANSI code)
  a0 = device 0 = none, 1 = terminal, 3 = uart, 4 = both
  returns: a0 = 0 or negative error

ECALL 0x05: tty_goto (uart should send ANSI code)
  a0 = device 0 = none, 1 = terminal, 3 = uart, 4 = both
  a1 = row
  a2 = col
  returns: a0 = 0 or negative error

ECALL 0x06: tty_fg (uart should send ANSI code)
  a0 = device 0 = none, 1 = terminal, 3 = uart, 4 = both
  a1 = 32bit color
  returns: a0 = 0 or negative error

ECALL 0x07: tty_bg (uart should send ANSI code)
  a0 = device 0 = none, 1 = terminal, 3 = uart, 4 = both
  a1 = 32bit color
  returns: a0 = 0 or negative error
