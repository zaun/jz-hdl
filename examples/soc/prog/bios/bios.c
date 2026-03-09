/*
 * bios.c - BIOS firmware for the JZ-HDL SOC
 *
 * Initializes hardware, runs POST (Power-On Self Test), and provides
 * a basic boot flow with ESC-to-enter-BIOS functionality.
 *
 * Memory map (byte addresses, RV32I):
 *   ROM:      0x00000000 - 0x00003FFF  (4096 words)
 *   RAM:      0x10000000 - 0x10004FFF  (5120 words, 20KB)
 *               0x10000000 - 0x100027FF  BIOS data area (10KB)
 *                 - SD sector buffer (1KB)
 *                 - FAT filesystem state
 *                 - UART RX ring buffer
 *                 - BIOS variables
 *               0x10002800 - 0x10004FFF  Stack (10KB)
 *   LED:      0x20000000               (lower 6 bits)
 *   UART:     0x30000000               (write=TX byte, read={30'b0, rx_has_data, tx_ready})
 *   UART RX:  0x30000004               (read=RX byte, clears rx_has_data)
 *   SDRAM:    0x40000000 - 0x407FFFFF  (8MB)
 *               0x40000000 - 0x402FFFFF  Program space (3MB)
 *               0x40300000 - 0x403FFFFF  Audio ring buffer (1MB, PCM data)
 *               0x40400000 - 0x407FFFFF  Video RAM (4MB, 2x 1080p framebuffer)
 *   TERMINAL: 0x50000000               (cell*8+0=attr, cell*8+4=char)
 *   SD CARD:  0x60000000               (register interface, see below)
 *
 * SD Card registers (base 0x60000000):
 *   +0x00: COMMAND   [W]  bits[1:0] = 00=NONE, 01=READ, 10=WRITE
 *   +0x04: STATUS    [R]  bits[4:0] = {DMA_ACTIVE, SDHC, READY, ERROR, BUSY}
 *   +0x08: SECTOR_LO [RW] bits[15:0] = sector address low
 *   +0x0C: SECTOR_HI [RW] bits[15:0] = sector address high
 *   +0x10: (reserved)
 *   +0x14: DATA      [RW] bits[15:0] = buffer auto-increment access
 *   +0x18: IRQ_CTRL  [RW] bits[1:0] = {clear, enable}
 *
 * CSRs:
 *   0xBC0: CLK_FREQ_MHZ     (clock frequency in MHz)
 *   0xBC1: video_mode       (0=720p 80x22, 1=1080p 120x33)
 *   0xBC2: baud_div         (16-bit UART baud divider)
 *   0xBC3: SDRAM_SIZE_BYTES 
 *   0xBC4: uartvec          (UART RX interrupt handler address)
 *   0xBC5: sdcardvec        (SD card interrupt handler address)
 * 
 * ECALLS (BIOS-provided):
 *  ┌──────┬──────────────────┬──────────┬────────────┬────────┬─────────────────────────────┐
 *  │  #   │   Name           │   a1     │     a2     │   a3   │           Returns           │
 *  ├──────┼──────────────────┼──────────┼────────────┼────────┼─────────────────────────────┤
 *  │ 0x00 │ register_handler │ fn_ptr   │ —          │ —      │ a0=0                        │
 *  │ 0x01 │ tty_read         │ ptr buf  │ length     │ —      │ a0=bytes_read or neg err    │
 *  │ 0x02 │ tty_size         │ device   │ —          │ —      │ a0=rows, a1=cols            │
 *  │ 0x03 │ tty_write        │ device   │ ptr buf    │ length │ a0=bytes_written or neg err │
 *  │ 0x04 │ tty_clear        │ device   │ —          │ —      │ a0=0 or neg err             │
 *  │ 0x05 │ tty_goto         │ device   │ row        │ col    │ a0=0 or neg err             │
 *  │ 0x06 │ tty_fg           │ device   │ 0x00RRGGBB │ —      │ a0=0 or neg err             │
 *  │ 0x07 │ tty_bg           │ device   │ 0x00RRGGBB │ —      │ a0=0 or neg err             │
 *  │ 0x08 │ vid_mode         │ mode     │ —          │ —      │ a0=0, a1=prev_mode          │
 *  │ 0x10 │ sd_status        │ —        │ —          │ —      │ a0=SD_STATUS register       │
 *  │ 0x11 │ sd_read_sector   │ lba      │ dest_ptr   │ —      │ a0=0 success, 1 error       │
 *  │ 0x12 │ sd_write_sector  │ lba      │ src_ptr    │ —      │ a0=0 success, 1 error       │
 *  └──────┴──────────────────┴──────────┴────────────┴────────┴─────────────────────────────┘
 *
 * Unhandled ECALLs are forwarded to a registered handler (kernel).
 */

#define LED         (*(volatile unsigned int *)0x20000000)
#define UART        (*(volatile unsigned int *)0x30000000)
#define UART_RXDATA (*(volatile unsigned int *)0x30000004)

static volatile unsigned int *const SDRAM = (volatile unsigned int *)0x40000000;

/* Terminal registers (command interface) */
#define TERM_CELL_ADDR  (*(volatile unsigned int *)0x50000000)
#define TERM_CELL_CHAR  (*(volatile unsigned int *)0x50000004)
#define TERM_CELL_ATTR  (*(volatile unsigned int *)0x50000008)
#define TERM_FILL_CHAR  (*(volatile unsigned int *)0x5000000C)
#define TERM_FILL_ATTR  (*(volatile unsigned int *)0x50000010)
#define TERM_COLS_REG   (*(volatile unsigned int *)0x50000014)
#define TERM_CELLS_REG  (*(volatile unsigned int *)0x50000018)
#define TERM_COMMAND    (*(volatile unsigned int *)0x5000001C)
#define TERM_STATUS     (*(volatile unsigned int *)0x50000020)
#define TERM_CURSOR     (*(volatile unsigned int *)0x50000024)

#define TERM_CMD_CLEAR     1
#define TERM_CMD_SCROLL_UP 2

/* Cursor styles */
#define CURSOR_NONE       0
#define CURSOR_LINE        1
#define CURSOR_BLOCK       2
#define CURSOR_LINE_BLINK  3
#define CURSOR_BLOCK_BLINK 4

/* SD Card registers */
#define SD_COMMAND   (*(volatile unsigned int *)0x60000000)
#define SD_STATUS    (*(volatile unsigned int *)0x60000004)
#define SD_SECTOR_LO (*(volatile unsigned int *)0x60000008)
#define SD_SECTOR_HI (*(volatile unsigned int *)0x6000000C)
#define SD_DATA      (*(volatile unsigned int *)0x60000014)
#define SD_IRQ_CTRL  (*(volatile unsigned int *)0x60000018)

/* SD commands */
#define SD_CMD_READ  1
#define SD_CMD_WRITE 2

/* SD status bits */
#define SD_BUSY   0x01
#define SD_ERROR  0x02
#define SD_READY  0x04
#define SD_SDHC   0x08

/* Max terminal size (1080p: 120x33 = 3960 cells) */
#define MAX_TERM_CELLS 3960

/* RGB565 colors */
#define WHITE_FG   0xFFFF
#define BLACK_FG   0x0000
#define BLACK_BG   0x0000
#define GREEN_FG   0x07E0
#define GREEN_BG   0x02E0
#define YELLOW_FG  0xFFE0
#define YELLOW_BG  0x8400
#define CYAN_FG    0x07FF
#define BLUE_BG    0x001F

/* Terminal dimensions (set at boot from CSR) */
static int term_cols;
static int term_rows;
static int term_cells;

/* Last UART RX byte — set by trap handler, read by uartvec handler */
static volatile unsigned int uart_rx_char;

/* UART terminal size (queried via ANSI escape) */
static unsigned int uart_rows;
static unsigned int uart_cols;
static unsigned int uart_size_queried;

/* ANSI size response parser state */
#define ANSI_IDLE    0
#define ANSI_ESC     1
#define ANSI_BRACKET 2
#define ANSI_EIGHT   3
#define ANSI_SEMI1   4
#define ANSI_ROWS    5
#define ANSI_SEMI2   6
#define ANSI_COLS    7
static int ansi_state;
static unsigned int ansi_rows_acc;
static unsigned int ansi_cols_acc;

/* SD sector buffer (in BIOS RAM instead of SDRAM) */
static unsigned int sd_buf[256];  /* 1024 bytes: one 512-byte sector as 16-bit words */

/* Partition table (cached from MBR at boot) */
struct part_info {
    unsigned int type;
    unsigned int lba;
    unsigned int size;
    /* FAT boot sector params (cached on first access) */
    unsigned int fat_start;
    unsigned int data_start;
    unsigned int spc_shift;
    unsigned int root_cluster;
    int          is_fat32;
    int          fat_valid;  /* 1 = FAT params cached */
};
static struct part_info parts[4];
static int mbr_valid;  /* 1 = MBR has been read */

/* Current working state (loaded from parts[] as needed) */
static unsigned int fs_fat_start;
static unsigned int fs_data_start;
static unsigned int fs_spc_shift;
static unsigned int fs_root_cluster;
static unsigned int fs_is_fat32;
static unsigned int fs_cur_cluster;
static char         fs_path[128];  /* "N:/path" or empty */

/* Open file state (for ECALLs) */
static unsigned int fs_file_size;     /* total file size in bytes */
static unsigned int fs_file_pos;      /* current read byte offset */
static unsigned int fs_file_cur_cl;   /* current cluster in chain */
static int          fs_file_open;     /* 1 if a file is open */

static int row_to_cell(int r)
{
    return r * term_cols;
}

/* Terminal state */
static int cursor_col;
static int cursor_row;
static unsigned int term_fg;
static unsigned int term_bg;

static void term_wait_ready(void)
{
    while (TERM_STATUS & 1)
        ;
}

static void term_write_cell(int col, int row, unsigned int ch, unsigned int fg, unsigned int bg)
{
    int cell = row_to_cell(row) + col;
    TERM_CELL_ADDR = (unsigned int)cell;
    TERM_CELL_ATTR = (bg << 16) | fg;
    TERM_CELL_CHAR = ch;
}

static void term_update_cursor(void)
{
    int cell = row_to_cell(cursor_row) + cursor_col;
    TERM_CURSOR = ((unsigned int)cell << 3) | CURSOR_LINE_BLINK;
}

static void term_clear(void)
{
    TERM_FILL_CHAR = ' ';
    TERM_FILL_ATTR = (term_bg << 16) | term_fg;
    TERM_COMMAND = TERM_CMD_CLEAR;
    term_wait_ready();
    cursor_col = 0;
    cursor_row = 0;
    term_update_cursor();
}

static void term_scroll(void)
{
    TERM_FILL_CHAR = ' ';
    TERM_FILL_ATTR = (term_bg << 16) | term_fg;
    TERM_COMMAND = TERM_CMD_SCROLL_UP;
    term_wait_ready();
}

static void term_putchar(int c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
    } else if (c == '\b') {
        if (cursor_col > 0)
            cursor_col--;
    } else {
        term_write_cell(cursor_col, cursor_row, (unsigned int)c, term_fg, term_bg);
        cursor_col++;
        if (cursor_col >= term_cols) {
            cursor_col = 0;
            cursor_row++;
        }
    }
    if (cursor_row >= term_rows) {
        term_scroll();
        cursor_row = term_rows - 1;
    }
    term_update_cursor();
}

static void term_puts(const char *s)
{
    while (*s)
        term_putchar(*s++);
}

static void term_set_color(unsigned int fg, unsigned int bg)
{
    term_fg = fg;
    term_bg = bg;
}

/* UART output */
static void uart_putchar(int c)
{
    while (!(UART & 1))
        ;
    UART = (unsigned int)c;
}

/* UART-only string output */
static void uart_puts(const char *s)
{
    while (*s)
        uart_putchar(*s++);
}

/* UART-only decimal output (for ANSI escape params) */
static void uart_put_dec(unsigned int val)
{
    char buf[11];
    int i = 0;
    if (val == 0) {
        uart_putchar('0');
        return;
    }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val = val / 10;
    }
    while (i > 0)
        uart_putchar(buf[--i]);
}

/* Output to both UART and terminal */
static void putchar(int c)
{
    uart_putchar(c);
    term_putchar(c);
}

static void puts(const char *s)
{
    while (*s)
        putchar(*s++);
}

static void put_hex(unsigned int val)
{
    int i;
    char buf[9];
    for (i = 7; i >= 0; i--) {
        unsigned int nib = val & 0xF;
        buf[i] = (nib < 10) ? ('0' + nib) : ('A' - 10 + nib);
        val >>= 4;
    }
    buf[8] = '\0';
    puts(buf);
}

/* Print unsigned int as decimal */
static void put_dec(unsigned int val)
{
    char buf[11];
    int i = 0;
    if (val == 0) {
        putchar('0');
        return;
    }
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val = val / 10;
    }
    /* Reverse */
    while (i > 0)
        putchar(buf[--i]);
}

/* Set cursor position (for in-place updates) */
static void term_set_cursor(int col, int row)
{
    cursor_col = col;
    cursor_row = row;
    term_update_cursor();
}

/* ----------------------------------------------------------------
 * Blue Screen of Death
 * ---------------------------------------------------------------- */
static void bsod(unsigned int mcause, unsigned int mepc, unsigned int mstatus)
{
    term_fg = WHITE_FG;
    term_bg = BLUE_BG;
    TERM_CURSOR = 0;  /* hide cursor */
    term_clear();
    cursor_col = 0;
    cursor_row = 1;

    term_puts("*** CRASH ***\n\n");
    term_puts("mcause:  0x");
    {
        char buf[9];
        int j;
        unsigned int v = mcause;
        for (j = 7; j >= 0; j--) {
            unsigned int nib = v & 0xF;
            buf[j] = (nib < 10) ? ('0' + nib) : ('A' - 10 + nib);
            v >>= 4;
        }
        buf[8] = '\0';
        term_puts(buf);
    }
    term_puts("\nmepc:    0x");
    {
        char buf[9];
        int j;
        unsigned int v = mepc;
        for (j = 7; j >= 0; j--) {
            unsigned int nib = v & 0xF;
            buf[j] = (nib < 10) ? ('0' + nib) : ('A' - 10 + nib);
            v >>= 4;
        }
        buf[8] = '\0';
        term_puts(buf);
    }
    term_puts("\nmstatus: 0x");
    {
        char buf[9];
        int j;
        unsigned int v = mstatus;
        for (j = 7; j >= 0; j--) {
            unsigned int nib = v & 0xF;
            buf[j] = (nib < 10) ? ('0' + nib) : ('A' - 10 + nib);
            v >>= 4;
        }
        buf[8] = '\0';
        term_puts(buf);
    }
    term_puts("\n");

    /* Also print to UART */
    uart_putchar('!'); uart_putchar('C'); uart_putchar('R');
    uart_putchar('A'); uart_putchar('S'); uart_putchar('H');
    uart_putchar(' '); uart_putchar('m'); uart_putchar('c');
    uart_putchar('=');
    {
        int j;
        unsigned int v = mcause;
        for (j = 7; j >= 0; j--) {
            unsigned int nib = (v >> (j << 2)) & 0xF;
            uart_putchar((nib < 10) ? ('0' + nib) : ('A' - 10 + nib));
        }
    }
    uart_putchar(' '); uart_putchar('p'); uart_putchar('c');
    uart_putchar('=');
    {
        int j;
        unsigned int v = mepc;
        for (j = 7; j >= 0; j--) {
            unsigned int nib = (v >> (j << 2)) & 0xF;
            uart_putchar((nib < 10) ? ('0' + nib) : ('A' - 10 + nib));
        }
    }
    uart_putchar('\n');

    for (;;)
        __asm__ volatile("" ::: "memory");
}

/* ----------------------------------------------------------------
 * Trap Handler
 * ---------------------------------------------------------------- */
void trap_handler(void)
{
    unsigned int mcause, mepc, mstatus, pending;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));
    __asm__ volatile("csrr %0, mepc" : "=r"(mepc));
    __asm__ volatile("csrr %0, mstatus" : "=r"(mstatus));

    /* External IRQ dispatch */
    if (mcause == 0x8000000Bu) {
        __asm__ volatile("csrr %0, 0xFC0" : "=r"(pending));

        /* IRQ bits: 0=UART TX ready, 1=UART RX data, 2=SD card */

        if (pending & 0x2) {
            unsigned int ch = UART_RXDATA;  /* always read to clear IRQ */

            /* Always feed ANSI size parser if a query is active */
            if (uart_size_queried == 1) {
                switch (ansi_state) {
                case ANSI_IDLE:
                    if (ch == 0x1B) ansi_state = ANSI_ESC;
                    break;
                case ANSI_ESC:
                    ansi_state = (ch == '[') ? ANSI_BRACKET : ANSI_IDLE;
                    break;
                case ANSI_BRACKET:
                    ansi_state = (ch == '8') ? ANSI_EIGHT : ANSI_IDLE;
                    break;
                case ANSI_EIGHT:
                    if (ch == ';') { ansi_state = ANSI_ROWS; ansi_rows_acc = 0; }
                    else ansi_state = ANSI_IDLE;
                    break;
                case ANSI_ROWS:
                    if (ch >= '0' && ch <= '9') ansi_rows_acc = ansi_rows_acc * 10 + (ch - '0');
                    else if (ch == ';') { ansi_state = ANSI_COLS; ansi_cols_acc = 0; }
                    else ansi_state = ANSI_IDLE;
                    break;
                case ANSI_COLS:
                    if (ch >= '0' && ch <= '9') ansi_cols_acc = ansi_cols_acc * 10 + (ch - '0');
                    else if (ch == 't') { uart_rows = ansi_rows_acc; uart_cols = ansi_cols_acc; uart_size_queried = 2; ansi_state = ANSI_IDLE; }
                    else ansi_state = ANSI_IDLE;
                    break;
                default:
                    ansi_state = ANSI_IDLE;
                    break;
                }
            }

            /* Always pass to uartvec handler if set */
            uart_rx_char = ch;
            {
                unsigned int handler;
                __asm__ volatile("csrr %0, 0xBC4" : "=r"(handler));
                if (handler)
                    ((void (*)(void))handler)();
            }
        }

        if (pending & 0x4) {
            unsigned int handler;
            __asm__ volatile("csrr %0, 0xBC5" : "=r"(handler));
            if (handler) {
                ((void (*)(void))handler)();
            } else {
                SD_IRQ_CTRL = 0x2;  /* acknowledge to clear IRQ */
            }
        }
        return;
    }

    /* Not an IRQ — crash */
    bsod(mcause, mepc, mstatus);
}

/* ----------------------------------------------------------------
 * ECALL System Call Interface
 * ---------------------------------------------------------------- */

struct ecall_ret {
    unsigned int a0;
    unsigned int a1;
};

/* UART RX ring buffer (filled by IRQ handler, read by shell and tty_read) */
#define RX_BUF_SIZE 64
static volatile unsigned char rx_buf[RX_BUF_SIZE];
static volatile unsigned int rx_head;
static volatile unsigned int rx_tail;

/* Registered kernel ECALL handler (set via ECALL 0x00) */
static unsigned int kernel_ecall_handler;

/* Forward declarations for SD/FAT functions used by ECALLs */
static int sd_read_sector(unsigned int lba);
static unsigned int sb(unsigned int offset);
static unsigned int sb16(unsigned int offset);
static unsigned int sb32(unsigned int offset);
static unsigned int cluster_to_lba(unsigned int cluster);
static unsigned int fat_next_cluster(unsigned int cluster);
static int match_dir_entry(unsigned int off, const char *name, int namelen);
static unsigned int find_in_dir(unsigned int cluster, const char *name,
                                int namelen, int *is_dir);
static int fs_use_partition(unsigned int idx);

/* 0x01: tty_read - non-blocking read from ring buffer */
static struct ecall_ret sys_tty_read(unsigned int buf_ptr, unsigned int length)
{
    struct ecall_ret ret;
    volatile unsigned char *buf = (volatile unsigned char *)buf_ptr;
    unsigned int count = 0;

    while (count < length && rx_head != rx_tail) {
        buf[count] = rx_buf[rx_tail];
        rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
        count++;
    }

    ret.a0 = count;
    ret.a1 = 0;
    return ret;
}

/* 0x02: tty_size - get terminal dimensions */
static struct ecall_ret sys_tty_size(unsigned int device)
{
    struct ecall_ret ret;
    ret.a0 = 0;
    ret.a1 = 0;

    if (device & 1) {
        /* Terminal */
        ret.a0 = (unsigned int)term_rows;
        ret.a1 = (unsigned int)term_cols;
    } else if (device & 2) {
        /* UART - send query if not yet done */
        if (uart_size_queried == 0) {
            /* Send ANSI query: ESC[18t (trap handler parses response) */
            uart_putchar(0x1B);
            uart_putchar('[');
            uart_putchar('1');
            uart_putchar('8');
            uart_putchar('t');
            uart_size_queried = 1; /* query sent, awaiting response */
            ret.a0 = 0;
            ret.a1 = 0;
        } else if (uart_size_queried == 2) {
            /* Response received */
            ret.a0 = uart_rows;
            ret.a1 = uart_cols;
        } else {
            /* Query sent but no response yet */
            ret.a0 = 0;
            ret.a1 = 0;
        }
    }
    return ret;
}

/* 0x03: tty_write - write buffer to device(s) */
static struct ecall_ret sys_tty_write(unsigned int device, unsigned int buf_ptr, unsigned int length)
{
    struct ecall_ret ret;
    volatile unsigned char *buf = (volatile unsigned char *)buf_ptr;
    unsigned int i;

    for (i = 0; i < length; i++) {
        int ch = buf[i];
        if (device & 1)
            term_putchar(ch);
        if (device & 2)
            uart_putchar(ch);
    }
    ret.a0 = length;
    ret.a1 = 0;
    return ret;
}

/* 0x04: tty_clear - clear screen */
static struct ecall_ret sys_tty_clear(unsigned int device)
{
    struct ecall_ret ret;

    if (device & 1)
        term_clear();
    if (device & 2) {
        /* ANSI: clear screen + home cursor */
        uart_puts("\033[2J\033[H");
    }
    ret.a0 = 0;
    ret.a1 = 0;
    return ret;
}

/* 0x05: tty_goto - move cursor */
static struct ecall_ret sys_tty_goto(unsigned int device, unsigned int row, unsigned int col)
{
    struct ecall_ret ret;

    if (device & 1)
        term_set_cursor((int)col, (int)row);
    if (device & 2) {
        /* ANSI: ESC[row+1;col+1H (1-based) */
        uart_putchar(0x1B);
        uart_putchar('[');
        uart_put_dec(row + 1);
        uart_putchar(';');
        uart_put_dec(col + 1);
        uart_putchar('H');
    }
    ret.a0 = 0;
    ret.a1 = 0;
    return ret;
}

/* 0x06: tty_fg - set foreground color (0x00RRGGBB) */
static struct ecall_ret sys_tty_fg(unsigned int device, unsigned int color)
{
    struct ecall_ret ret;
    unsigned int r = (color >> 16) & 0xFF;
    unsigned int g = (color >> 8) & 0xFF;
    unsigned int b = color & 0xFF;

    if (device & 1) {
        /* Convert RGB888 to RGB565 */
        term_fg = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }
    if (device & 2) {
        /* ANSI: ESC[38;2;R;G;Bm */
        uart_puts("\033[38;2;");
        uart_put_dec(r);
        uart_putchar(';');
        uart_put_dec(g);
        uart_putchar(';');
        uart_put_dec(b);
        uart_putchar('m');
    }
    ret.a0 = 0;
    ret.a1 = 0;
    return ret;
}

/* 0x07: tty_bg - set background color (0x00RRGGBB) */
static struct ecall_ret sys_tty_bg(unsigned int device, unsigned int color)
{
    struct ecall_ret ret;
    unsigned int r = (color >> 16) & 0xFF;
    unsigned int g = (color >> 8) & 0xFF;
    unsigned int b = color & 0xFF;

    if (device & 1) {
        /* Convert RGB888 to RGB565 */
        term_bg = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }
    if (device & 2) {
        /* ANSI: ESC[48;2;R;G;Bm */
        uart_puts("\033[48;2;");
        uart_put_dec(r);
        uart_putchar(';');
        uart_put_dec(g);
        uart_putchar(';');
        uart_put_dec(b);
        uart_putchar('m');
    }
    ret.a0 = 0;
    ret.a1 = 0;
    return ret;
}

/* 0x08: vid_mode - set video mode (0=720p, 1=1080p)
 *   Clears the screen after switching modes.
 *   Returns a0=0, a1=previous mode. */
static struct ecall_ret sys_vid_mode(unsigned int mode)
{
    struct ecall_ret ret;
    unsigned int prev;

    __asm__ volatile("csrr %0, 0xBC1" : "=r"(prev));

    mode = mode & 1;
    __asm__ volatile("csrw 0xBC1, %0" :: "r"(mode));

    if (mode) {
        term_cols = 120;
        term_rows = 33;
    } else {
        term_cols = 80;
        term_rows = 22;
    }
    term_cells = row_to_cell(term_rows);

    TERM_COLS_REG = (unsigned int)term_cols;
    TERM_CELLS_REG = (unsigned int)term_cells;

    cursor_col = 0;
    cursor_row = 0;
    term_clear();

    ret.a0 = 0;
    ret.a1 = prev & 1;
    return ret;
}

/* 0x10: sd_status - return SD_STATUS register */
static struct ecall_ret sys_sd_status(void)
{
    struct ecall_ret ret;
    ret.a0 = SD_STATUS;
    ret.a1 = 0;
    return ret;
}

/* 0x11: sd_read_sector - read one 512-byte sector to dest_ptr
 *   a1=LBA, a2=destination address (256 x 16-bit words written)
 *   Returns a0=0 success, 1 error */
static struct ecall_ret sys_sd_read_sector_ecall(unsigned int lba, unsigned int dest_ptr)
{
    struct ecall_ret ret;
    volatile unsigned int *dest = (volatile unsigned int *)dest_ptr;
    unsigned int status;
    int w;

    ret.a1 = 0;

    SD_SECTOR_LO = lba & 0xFFFF;
    SD_SECTOR_HI = (lba >> 16) & 0xFFFF;
    SD_COMMAND = SD_CMD_READ;
    for (;;) {
        status = SD_STATUS;
        if (!(status & SD_BUSY))
            break;
    }
    if (status & SD_ERROR) {
        ret.a0 = 1;
        return ret;
    }
    for (w = 0; w < 256; w++)
        dest[w] = SD_DATA;

    ret.a0 = 0;
    return ret;
}

/* 0x12: sd_write_sector - write one 512-byte sector from src_ptr
 *   a1=LBA, a2=source address (256 x 16-bit words read from src)
 *   Returns a0=0 success, 1 error */
static struct ecall_ret sys_sd_write_sector_ecall(unsigned int lba, unsigned int src_ptr)
{
    struct ecall_ret ret;
    volatile unsigned int *src = (volatile unsigned int *)src_ptr;
    unsigned int status;
    int w;

    ret.a1 = 0;

    /* Write 256 words into the SD data buffer */
    for (w = 0; w < 256; w++)
        SD_DATA = src[w];

    SD_SECTOR_LO = lba & 0xFFFF;
    SD_SECTOR_HI = (lba >> 16) & 0xFFFF;
    SD_COMMAND = SD_CMD_WRITE;
    for (;;) {
        status = SD_STATUS;
        if (!(status & SD_BUSY))
            break;
    }
    if (status & SD_ERROR) {
        ret.a0 = 1;
        return ret;
    }

    ret.a0 = 0;
    return ret;
}

/* Internal file open/read/close — used by BIOS shell and auto-boot.
 * These call sd_read_sector() internally (not via ECALL). */

static struct ecall_ret bios_sd_open(unsigned int path_ptr)
{
    struct ecall_ret ret;
    const char *path = (const char *)path_ptr;
    unsigned int idx;
    const char *p;
    unsigned int clust;

    ret.a0 = 0xFFFFFFFFu;
    ret.a1 = 0;

    /* Parse partition number */
    if (path[0] < '0' || path[0] > '3' || path[1] != ':') {
        return ret;
    }
    idx = (unsigned int)(path[0] - '0');

    if (fs_use_partition(idx))
        return ret;

    /* Navigate path to find the file */
    p = path + 2;
    if (*p == '/')
        p++;

    clust = fs_root_cluster;
    while (*p) {
        const char *start = p;
        int len = 0;
        int is_dir = 0;
        unsigned int found;

        while (*p && *p != '/')  {
            p++;
            len++;
        }
        if (len == 0) {
            if (*p == '/') p++;
            continue;
        }

        found = find_in_dir(clust, start, len, &is_dir);
        if (found == 0)
            return ret;

        if (*p == '/') {
            /* Intermediate component must be a directory */
            if (!is_dir)
                return ret;
            p++;
            clust = found;
        } else {
            /* Final component — could be file or dir */
            if (is_dir)
                return ret;  /* can't "open" a directory */

            /* Get file size from the directory entry we just matched.
             * find_in_dir left the sector in sd_buf, so we need to
             * re-read the sector containing this entry to get size. */
            {
                /* Re-scan parent dir to find the entry and read its size */
                unsigned int pc = clust;
                while (pc < 0x0FFFFFF8u) {
                    unsigned int lba = cluster_to_lba(pc);
                    unsigned int spc = 1u << fs_spc_shift;
                    unsigned int s;
                    for (s = 0; s < spc; s++) {
                        int j;
                        if (sd_read_sector(lba + s))
                            return ret;
                        for (j = 0; j < 16; j++) {
                            unsigned int eoff = (unsigned int)(j << 5);
                            unsigned int first = sb(eoff);
                            unsigned int attr = sb(eoff + 11);
                            if (first == 0x00)
                                return ret;
                            if (first == 0xE5 || attr == 0x0F || (attr & 0x08))
                                continue;
                            if (match_dir_entry(eoff, start, len)) {
                                fs_file_size = sb32(eoff + 28);
                                fs_file_pos = 0;
                                fs_file_cur_cl = found;
                                fs_file_open = 1;
                                ret.a0 = fs_file_size;
                                return ret;
                            }
                        }
                    }
                    pc = fat_next_cluster(pc);
                }
            }
            return ret;
        }
    }

    return ret;
}

static struct ecall_ret bios_sd_read(unsigned int dest_ptr, unsigned int length)
{
    struct ecall_ret ret;
    volatile unsigned char *dest = (volatile unsigned char *)dest_ptr;
    unsigned int bytes_read = 0;
    unsigned int bytes_per_cluster = (1u << fs_spc_shift) << 9;

    ret.a0 = 0;
    ret.a1 = 0;

    if (!fs_file_open)
        return ret;

    /* Clamp to remaining file */
    if (fs_file_pos + length > fs_file_size)
        length = fs_file_size - fs_file_pos;
    if (length == 0)
        return ret;

    while (bytes_read < length) {
        unsigned int offset_in_cluster = fs_file_pos % bytes_per_cluster;
        unsigned int sector_in_cluster = offset_in_cluster >> 9;
        unsigned int offset_in_sector = fs_file_pos & 0x1FF;
        unsigned int lba = cluster_to_lba(fs_file_cur_cl) + sector_in_cluster;
        unsigned int chunk, i;

        if (sd_read_sector(lba)) {
            ret.a0 = bytes_read;
            return ret;
        }

        /* How many bytes to copy from this sector */
        chunk = 512 - offset_in_sector;
        if (chunk > length - bytes_read)
            chunk = length - bytes_read;

        /* Copy bytes from sd_buf to destination */
        for (i = 0; i < chunk; i++) {
            unsigned int byte_off = offset_in_sector + i;
            unsigned int word = sd_buf[byte_off >> 1];
            unsigned char b = (byte_off & 1) ? (unsigned char)(word >> 8) : (unsigned char)(word & 0xFF);
            dest[bytes_read + i] = b;
        }

        bytes_read += chunk;
        fs_file_pos += chunk;

        /* Check if we need to advance to next cluster */
        if (fs_file_pos % bytes_per_cluster == 0 && bytes_read < length) {
            fs_file_cur_cl = fat_next_cluster(fs_file_cur_cl);
            if (fs_file_cur_cl >= 0x0FFFFFF8u)
                break;
        }
    }

    ret.a0 = bytes_read;
    return ret;
}

static void bios_sd_close(void)
{
    fs_file_open = 0;
}

/* ECALL dispatch - called from _ecall_entry in start.S
 * a0=syscall number, a1-a3=args.
 * Returns struct ecall_ret in a0,a1 per RV32 ILP32 ABI. */
struct ecall_ret ecall_dispatch(unsigned int num, unsigned int arg1,
                                unsigned int arg2, unsigned int arg3)
{
    struct ecall_ret ret;
    ret.a0 = (unsigned int)-1;  /* default: invalid syscall */
    ret.a1 = 0;

    switch (num) {
    case 0x00:
        kernel_ecall_handler = arg1;
        ret.a0 = 0;
        return ret;
    case 0x01: return sys_tty_read(arg1, arg2);
    case 0x02: return sys_tty_size(arg1);
    case 0x03: return sys_tty_write(arg1, arg2, arg3);
    case 0x04: return sys_tty_clear(arg1);
    case 0x05: return sys_tty_goto(arg1, arg2, arg3);
    case 0x06: return sys_tty_fg(arg1, arg2);
    case 0x07: return sys_tty_bg(arg1, arg2);
    case 0x08: return sys_vid_mode(arg1);
    case 0x10: return sys_sd_status();
    case 0x11: return sys_sd_read_sector_ecall(arg1, arg2);
    case 0x12: return sys_sd_write_sector_ecall(arg1, arg2);
    default:
        if (kernel_ecall_handler) {
            typedef struct ecall_ret (*handler_fn)(unsigned int, unsigned int,
                                                   unsigned int, unsigned int);
            return ((handler_fn)kernel_ecall_handler)(num, arg1, arg2, arg3);
        }
        return ret;
    }
}

/* ----------------------------------------------------------------
 * Hardware Init
 * ---------------------------------------------------------------- */
static void init_terminal(void)
{
    unsigned int vmode;
    __asm__ volatile("csrr %0, 0xBC1" : "=r"(vmode));
    if (vmode & 1) {
        term_cols = 120;
        term_rows = 33;
    } else {
        term_cols = 80;
        term_rows = 22;
    }
    term_cells = row_to_cell(term_rows);

    /* Configure terminal hardware */
    TERM_COLS_REG = (unsigned int)term_cols;
    TERM_CELLS_REG = (unsigned int)term_cells;

    term_fg = WHITE_FG;
    term_bg = BLACK_BG;
    term_clear();
}

/* Read mcycle CSR (0xB00) */
static unsigned int read_mcycle(void)
{
    unsigned int val;
    __asm__ volatile("csrr %0, 0xB00" : "=r"(val));
    return val;
}

/* ----------------------------------------------------------------
 * SDRAM Memory Test
 *
 * Updates status line in-place with format:
 *   SDRAM 8MB:   1% Write
 *   SDRAM 8MB: 100% Read
 *
 * progress_col is the column where "  1% Write" starts (after "SDRAM 8MB: ")
 * ---------------------------------------------------------------- */
static void sdram_progress(int status_row, int progress_col,
                           unsigned int pct, const char *desc)
{
    int saved_col = cursor_col;
    int saved_row = cursor_row;

    /* Position at progress_col, blank old text first */
    term_set_cursor(progress_col, status_row);
    puts("          ");  /* clear 10 chars */
    term_set_cursor(progress_col, status_row);

    /* Right-align percentage in 3 chars: "  1", " 50", "100" */
    if (pct < 10) {
        putchar(' '); putchar(' ');
    } else if (pct < 100) {
        putchar(' ');
    }
    put_dec(pct);
    puts("% ");
    puts(desc);

    cursor_col = saved_col;
    cursor_row = saved_row;
}

static int sdram_test(unsigned int size_bytes, int status_row, int progress_col)
{
    unsigned int total_words = size_bytes >> 2;
    unsigned int chunk = total_words >> 5; /* 1/32 for ~3% steps */
    unsigned int addr, end, done;

    if (chunk == 0)
        chunk = 1;

    /* Pass 1: Write pattern */
    done = 0;
    for (addr = 0; addr < total_words; ) {
        end = addr + chunk;
        if (end > total_words)
            end = total_words;
        for (; addr < end; addr++)
            SDRAM[addr] = addr ^ 0xA5A5A5A5u;
        done += chunk;
        {
            unsigned int pct = done * 100 / total_words;
            if (pct > 100) pct = 100;
            sdram_progress(status_row, progress_col, pct, "Write");
        }
    }
    sdram_progress(status_row, progress_col, 100, "Write");

    /* Pass 2: Read back and verify */
    done = 0;
    for (addr = 0; addr < total_words; ) {
        end = addr + chunk;
        if (end > total_words)
            end = total_words;
        for (; addr < end; addr++) {
            unsigned int expected = addr ^ 0xA5A5A5A5u;
            unsigned int got = SDRAM[addr];
            if (got != expected) {
                putchar('\n');
                puts("FAIL @ 0x");
                put_hex(addr << 2);
                puts(" exp=0x");
                put_hex(expected);
                puts(" got=0x");
                put_hex(got);
                putchar('\n');
                return 1;
            }
        }
        done += chunk;
        {
            unsigned int pct = done * 100 / total_words;
            if (pct > 100) pct = 100;
            sdram_progress(status_row, progress_col, pct, "Read");
        }
    }
    sdram_progress(status_row, progress_col, 100, "Read");
    return 0;
}

static void uart_rx_to_buf(void)
{
    unsigned int next = (rx_head + 1) % RX_BUF_SIZE;
    if (next != rx_tail) {
        rx_buf[rx_head] = (unsigned char)uart_rx_char;
        rx_head = next;
    }
}

/* ----------------------------------------------------------------
 * BIOS Interactive Shell
 * ---------------------------------------------------------------- */

static int shell_getchar(void)
{
    while (rx_head == rx_tail)
        ;
    unsigned char ch = rx_buf[rx_tail];
    rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
    return (int)ch;
}

static int shell_readline(char *buf, int maxlen)
{
    int len = 0;
    for (;;) {
        int ch = shell_getchar();
        if (ch == '\r' || ch == '\n') {
            buf[len] = '\0';
            putchar('\n');
            return len;
        }
        if (ch == 0x08 || ch == 0x7F) {
            /* Backspace */
            if (len > 0) {
                len--;
                putchar('\b');
                putchar(' ');
                putchar('\b');
            }
            continue;
        }
        if (len < maxlen - 1 && ch >= 0x20) {
            buf[len++] = (char)ch;
            putchar(ch);
        }
    }
}

static unsigned int parse_hex(const char *s)
{
    unsigned int val = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        s += 2;
    while (*s) {
        char c = *s++;
        if (c >= '0' && c <= '9')
            val = (val << 4) | (unsigned int)(c - '0');
        else if (c >= 'a' && c <= 'f')
            val = (val << 4) | (unsigned int)(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F')
            val = (val << 4) | (unsigned int)(c - 'A' + 10);
        else
            break;
    }
    return val;
}

static unsigned int parse_dec(const char *s)
{
    unsigned int val = 0;
    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (unsigned int)(*s - '0');
        s++;
    }
    return val;
}

/* Skip whitespace, return pointer to next non-space */
static const char *skip_spaces(const char *s)
{
    while (*s == ' ')
        s++;
    return s;
}

/* Skip non-whitespace, return pointer to next space or end */
static const char *skip_token(const char *s)
{
    while (*s && *s != ' ')
        s++;
    return s;
}

/* ----------------------------------------------------------------
 * SD / FAT Filesystem Helpers
 * ---------------------------------------------------------------- */

static int sd_read_sector(unsigned int lba)
{
    unsigned int status;
    int w;
    SD_SECTOR_LO = lba & 0xFFFF;
    SD_SECTOR_HI = (lba >> 16) & 0xFFFF;
    SD_COMMAND = SD_CMD_READ;
    for (;;) {
        status = SD_STATUS;
        if (!(status & SD_BUSY))
            break;
    }
    if (status & SD_ERROR)
        return 1;
    for (w = 0; w < 256; w++)
        sd_buf[w] = SD_DATA;
    return 0;
}

static unsigned int sb(unsigned int offset)
{
    unsigned int word = sd_buf[offset >> 1];
    if (offset & 1)
        return (word >> 8) & 0xFF;
    return word & 0xFF;
}

static unsigned int sb16(unsigned int offset)
{
    return sd_buf[offset >> 1] & 0xFFFF;
}

static unsigned int sb32(unsigned int offset)
{
    return (sd_buf[(offset >> 1) + 1] << 16) | (sd_buf[offset >> 1] & 0xFFFF);
}

static unsigned int cluster_to_lba(unsigned int cluster)
{
    return fs_data_start + ((cluster - 2) << fs_spc_shift);
}

static unsigned int fat_next_cluster(unsigned int cluster)
{
    unsigned int fat_offset, fat_sector, ent_offset, val;
    if (fs_is_fat32) {
        fat_offset = cluster << 2;
        fat_sector = fs_fat_start + (fat_offset >> 9);
        ent_offset = fat_offset & 0x1FF;
        if (sd_read_sector(fat_sector))
            return 0x0FFFFFFFu;
        val = sb32(ent_offset) & 0x0FFFFFFFu;
        if (val >= 0x0FFFFFF8u)
            return 0x0FFFFFFFu;
    } else {
        fat_offset = cluster << 1;
        fat_sector = fs_fat_start + (fat_offset >> 9);
        ent_offset = fat_offset & 0x1FF;
        if (sd_read_sector(fat_sector))
            return 0xFFFFu;
        val = sb16(ent_offset);
        if (val >= 0xFFF8u)
            return 0x0FFFFFFFu;
    }
    return val;
}

static int is_fat_type(unsigned int t)
{
    return t == 0x01 || t == 0x04 || t == 0x06 ||
           t == 0x0B || t == 0x0C || t == 0x0E;
}

static void put_fname83(unsigned int off)
{
    int i, last;
    last = 7;
    while (last >= 0 && sb(off + (unsigned int)last) == ' ')
        last--;
    for (i = 0; i <= last; i++)
        putchar((int)sb(off + (unsigned int)i));
    last = 2;
    while (last >= 0 && sb(off + 8 + (unsigned int)last) == ' ')
        last--;
    if (last >= 0) {
        putchar('.');
        for (i = 0; i <= last; i++)
            putchar((int)sb(off + 8 + (unsigned int)i));
    }
}

static unsigned int dir_entry_cluster(unsigned int off)
{
    unsigned int hi = sb16(off + 20);
    unsigned int lo = sb16(off + 26);
    return (hi << 16) | lo;
}

static char to_upper(char c)
{
    if (c >= 'a' && c <= 'z')
        return c - 32;
    return c;
}

/* Match a path component against an 8.3 dir entry at offset off in sector buf.
 * name is the component, namelen its length. Returns 1 on match. */
static int match_dir_entry(unsigned int off, const char *name, int namelen)
{
    char entry83[11];
    int i, dot;

    /* Build padded 8.3 from name */
    for (i = 0; i < 11; i++)
        entry83[i] = ' ';

    dot = -1;
    for (i = 0; i < namelen; i++) {
        if (name[i] == '.') {
            dot = i;
            break;
        }
    }
    if (dot < 0) {
        /* No extension */
        for (i = 0; i < namelen && i < 8; i++)
            entry83[i] = to_upper(name[i]);
    } else {
        for (i = 0; i < dot && i < 8; i++)
            entry83[i] = to_upper(name[i]);
        for (i = 0; i < namelen - dot - 1 && i < 3; i++)
            entry83[8 + i] = to_upper(name[dot + 1 + i]);
    }

    /* Compare against sector buffer */
    for (i = 0; i < 11; i++) {
        if ((char)sb(off + (unsigned int)i) != entry83[i])
            return 0;
    }
    return 1;
}

/* Search directory starting at cluster for a name component.
 * Returns cluster of found entry, or 0 on failure.
 * Sets *is_dir to 1 if entry is a directory. */
static unsigned int find_in_dir(unsigned int cluster, const char *name,
                                int namelen, int *is_dir)
{
    unsigned int clust = cluster;
    while (clust < 0x0FFFFFF8u) {
        unsigned int lba = cluster_to_lba(clust);
        unsigned int spc = 1u << fs_spc_shift;
        unsigned int s;
        for (s = 0; s < spc; s++) {
            int j;
            if (sd_read_sector(lba + s))
                return 0;
            for (j = 0; j < 16; j++) {
                unsigned int eoff = (unsigned int)(j << 5);
                unsigned int first = sb(eoff);
                unsigned int attr = sb(eoff + 11);
                if (first == 0x00)
                    return 0;
                if (first == 0xE5 || attr == 0x0F || (attr & 0x08))
                    continue;
                if (match_dir_entry(eoff, name, namelen)) {
                    *is_dir = (attr & 0x10) ? 1 : 0;
                    return dir_entry_cluster(eoff);
                }
            }
        }
        clust = fat_next_cluster(clust);
    }
    return 0;
}

/* ----------------------------------------------------------------
 * FAT Shell Commands
 * ---------------------------------------------------------------- */

/* Read MBR and cache all 4 partition entries. Called once at boot. */
static void fs_cache_mbr(void)
{
    int i;
    if (sd_read_sector(0))
        return;
    if (sb16(510) != 0xAA55)
        return;
    for (i = 0; i < 4; i++) {
        unsigned int base = 446 + (unsigned int)(i << 4);
        parts[i].type = sb(base + 4);
        parts[i].lba  = sb32(base + 8);
        parts[i].size = sb32(base + 12);
        parts[i].fat_valid = 0;
    }
    mbr_valid = 1;
}

/* Load FAT params for partition idx into working globals.
 * Reads boot sector on first access, caches for subsequent use.
 * Returns 0 on success, -1 on error. */
static int fs_use_partition(unsigned int idx)
{
    if (idx > 3 || !mbr_valid)
        return -1;
    if (!is_fat_type(parts[idx].type))
        return -1;

    /* Read boot sector on first access */
    if (!parts[idx].fat_valid) {
        unsigned int plba = parts[idx].lba;
        unsigned int spc, reserved, num_fats, fat_size, tmp;
        int is32 = (parts[idx].type == 0x0B || parts[idx].type == 0x0C);

        if (sd_read_sector(plba))
            return -1;

        spc = sb(13);
        reserved = sb16(14);
        num_fats = sb(16);

        if (is32) {
            fat_size = sb32(36);
            parts[idx].root_cluster = sb32(44);
        } else {
            fat_size = sb16(22);
            parts[idx].root_cluster = 2;
        }

        parts[idx].fat_start  = plba + reserved;
        parts[idx].data_start = parts[idx].fat_start + fat_size * num_fats;
        parts[idx].is_fat32   = is32;

        /* Compute log2(spc) */
        tmp = spc;
        parts[idx].spc_shift = 0;
        while (tmp > 1) {
            tmp >>= 1;
            parts[idx].spc_shift++;
        }

        parts[idx].fat_valid = 1;
    }

    /* Load into working globals */
    fs_fat_start    = parts[idx].fat_start;
    fs_data_start   = parts[idx].data_start;
    fs_spc_shift    = parts[idx].spc_shift;
    fs_root_cluster = parts[idx].root_cluster;
    fs_is_fat32     = (unsigned int)parts[idx].is_fat32;

    return 0;
}

static void cmd_part(void)
{
    int i;
    if (!mbr_valid) {
        puts("No SD card\n");
        return;
    }
    for (i = 0; i < 4; i++) {
        unsigned int ptype = parts[i].type;
        if (ptype == 0)
            continue;
        putchar(' ');
        if (is_fat_type(ptype))
            putchar('*');
        else
            putchar(' ');
        putchar('0' + i);
        puts(": type=0x");
        {
            char h[3];
            h[0] = "0123456789ABCDEF"[(ptype >> 4) & 0xF];
            h[1] = "0123456789ABCDEF"[ptype & 0xF];
            h[2] = '\0';
            puts(h);
        }
        puts(" LBA=");
        put_dec(parts[i].lba);
        puts(" size=");
        put_dec(parts[i].size);
        putchar('\n');
    }
}

static void cmd_pwd(void)
{
    if (!fs_path[0]) {
        puts("Use: cd 0:/\n");
        return;
    }
    puts(fs_path);
    putchar('\n');
}

static void cmd_ls(void)
{
    unsigned int clust;

    if (!fs_path[0]) {
        puts("Use: cd 0:/\n");
        return;
    }

    /* Load FAT params for current partition */
    if (fs_use_partition((unsigned int)(fs_path[0] - '0'))) {
        puts("Partition error\n");
        return;
    }

    clust = fs_cur_cluster;
    while (clust < 0x0FFFFFF8u) {
        unsigned int lba = cluster_to_lba(clust);
        unsigned int spc = 1u << fs_spc_shift;
        unsigned int s;
        for (s = 0; s < spc; s++) {
            int j;
            if (sd_read_sector(lba + s)) {
                puts("Read error\n");
                return;
            }
            for (j = 0; j < 16; j++) {
                unsigned int eoff = (unsigned int)(j << 5);
                unsigned int first = sb(eoff);
                unsigned int attr = sb(eoff + 11);
                if (first == 0x00)
                    return;
                if (first == 0xE5 || attr == 0x0F || (attr & 0x08))
                    continue;
                if (attr & 0x10) {
                    puts("  <DIR>  ");
                } else {
                    unsigned int fsize = sb32(eoff + 28);
                    /* Right-align size in 7 chars */
                    {
                        char sbuf[8];
                        int si = 0, k;
                        unsigned int v = fsize;
                        if (v == 0)
                            sbuf[si++] = '0';
                        else
                            while (v > 0) { sbuf[si++] = '0' + (v % 10); v /= 10; }
                        puts("  ");
                        for (k = si; k < 7; k++)
                            putchar(' ');
                        while (si > 0)
                            putchar(sbuf[--si]);
                        putchar(' ');
                    }
                }
                put_fname83(eoff);
                putchar('\n');
            }
        }
        clust = fat_next_cluster(clust);
    }
}

/* Resolve a path from root, returning the final cluster.
 * Path may start with "N:/" prefix which is skipped. */
static unsigned int resolve_path(const char *path)
{
    unsigned int clust = fs_root_cluster;
    const char *p = path;
    int is_dir;

    /* Skip "N:/" prefix if present */
    if (p[0] >= '0' && p[0] <= '3' && p[1] == ':')
        p += 2;
    if (*p == '/')
        p++;
    while (*p) {
        const char *start = p;
        int len = 0;
        while (*p && *p != '/') {
            p++;
            len++;
        }
        if (len == 0)
            break;
        clust = find_in_dir(clust, start, len, &is_dir);
        if (clust == 0)
            return 0;
        if (*p == '/')
            p++;
    }
    return clust;
}

static void cmd_cd(const char *arg)
{
    unsigned int part_idx;

    arg = skip_spaces(arg);

    if (!*arg) {
        puts("Usage: cd <path>  (e.g. cd 0:/ or cd SUBDIR)\n");
        return;
    }

    /* Check for partition prefix: "N:/" */
    if (arg[0] >= '0' && arg[0] <= '3' && arg[1] == ':') {
        part_idx = (unsigned int)(arg[0] - '0');
        if (fs_use_partition(part_idx)) {
            puts("Not a FAT partition\n");
            return;
        }
        /* Skip past "N:" */
        arg += 2;
        /* Reset to root of this partition */
        fs_cur_cluster = fs_root_cluster;
        fs_path[0] = (char)('0' + part_idx);
        fs_path[1] = ':';
        fs_path[2] = '/';
        fs_path[3] = '\0';
        /* If just "N:/" or "N:", done */
        if (arg[0] == '\0' || (arg[0] == '/' && arg[1] == '\0'))
            return;
        /* Skip leading / after "N:" */
        if (arg[0] == '/')
            arg++;
    }

    if (!fs_path[0]) {
        puts("Use: cd 0:/\n");
        return;
    }

    /* Load FAT params for current partition */
    if (fs_use_partition((unsigned int)(fs_path[0] - '0'))) {
        puts("Partition error\n");
        return;
    }

    /* cd .. — pop last component and re-resolve */
    if (arg[0] == '.' && arg[1] == '.' && arg[2] == '\0') {
        int len = 0;
        unsigned int clust;
        while (fs_path[len])
            len++;
        /* "N:/" is the root - 3 chars */
        if (len > 3 && fs_path[len - 1] == '/')
            len--;
        while (len > 3 && fs_path[len - 1] != '/')
            len--;
        fs_path[len] = '\0';
        if (len <= 3) {
            fs_path[2] = '/';
            fs_path[3] = '\0';
            fs_cur_cluster = fs_root_cluster;
        } else {
            clust = resolve_path(fs_path);
            if (clust == 0) {
                fs_cur_cluster = fs_root_cluster;
                fs_path[2] = '/';
                fs_path[3] = '\0';
            } else {
                fs_cur_cluster = clust;
            }
        }
        return;
    }

    /* cd dir or cd dir/dir/dir — tokenize by / */
    {
        const char *p = arg;
        unsigned int clust = fs_cur_cluster;
        int pathlen = 0;
        char newpath[128];
        int i;

        /* Copy current path */
        while (fs_path[pathlen]) {
            newpath[pathlen] = fs_path[pathlen];
            pathlen++;
        }

        while (*p) {
            const char *start = p;
            int len = 0;
            int is_dir = 0;
            unsigned int found;

            while (*p && *p != '/') {
                p++;
                len++;
            }
            if (len == 0) {
                if (*p == '/') p++;
                continue;
            }

            found = find_in_dir(clust, start, len, &is_dir);
            if (found == 0) {
                puts("Not found: ");
                for (i = 0; i < len; i++)
                    putchar(start[i]);
                putchar('\n');
                return;
            }
            if (!is_dir) {
                puts("Not a directory: ");
                for (i = 0; i < len; i++)
                    putchar(start[i]);
                putchar('\n');
                return;
            }

            clust = found;

            /* Append to path — ensure trailing / before component */
            if (pathlen > 0 && newpath[pathlen - 1] != '/')
                newpath[pathlen++] = '/';
            for (i = 0; i < len && pathlen < 126; i++)
                newpath[pathlen++] = start[i];
            newpath[pathlen] = '\0';

            if (*p == '/')
                p++;
        }

        fs_cur_cluster = clust;
        for (i = 0; i <= pathlen; i++)
            fs_path[i] = newpath[i];
    }
}

static void cmd_help(void)
{
    puts("JZ-HDL SOC BIOS v1.0\n");
    puts("Commands:\n");
    puts("  ?             Show this help\n");
    puts("  v <0|1>       Set video mode (0=720p, 1=1080p)\n");
    puts("  b             Show baud rate info\n");
    puts("  b <baud>      Set UART baud rate\n");
    puts("  r <addr>      Read memory word (hex addr)\n");
    puts("  w <addr> <d>  Write memory word (hex addr, hex data)\n");
    puts("  d <addr> <n>  Dump n words (hex addr, dec count)\n");
    puts("  j <addr>      Jump to address (hex)\n");
    puts("  l <path>      Load and run file (e.g. l 0:/PROG.BIN)\n");
    puts("  rd            Dump CSR registers\n");
    puts("  p             List SD card partitions\n");
    puts("  pwd           Print current path\n");
    puts("  cd <path>     Change directory (e.g. cd 0:/ or cd SUBDIR)\n");
    puts("  ls            List directory contents\n");
}

static void cmd_video(const char *arg)
{
    unsigned int mode;
    arg = skip_spaces(arg);
    if (*arg == '0')
        mode = 0;
    else if (*arg == '1')
        mode = 1;
    else {
        puts("Usage: v <0|1>\n");
        return;
    }
    __asm__ volatile("csrw 0xBC1, %0" :: "r"(mode));
    init_terminal();
    term_set_color(WHITE_FG, BLUE_BG);
    term_clear();
    puts("Video mode set to ");
    puts(mode ? "1080p" : "720p");
    putchar('\n');
}

static void cmd_baud_show(void)
{
    unsigned int clk_freq, baud_div, baud;
    __asm__ volatile("csrr %0, 0xBC0" : "=r"(clk_freq));
    __asm__ volatile("csrr %0, 0xBC2" : "=r"(baud_div));
    baud = (clk_freq * 1000000u) / (baud_div + 1);
    puts("Baud: ");
    put_dec(baud);
    puts("  (div=");
    put_dec(baud_div);
    puts(")\n");
    puts("Standard rates (clk=");
    put_dec(clk_freq);
    puts("MHz):\n");
    {
        static const unsigned int rates[] = {9600, 19200, 38400, 57600, 115200};
        int i;
        unsigned int clk_hz = clk_freq * 1000000u;
        for (i = 0; i < 5; i++) {
            unsigned int d = clk_hz / rates[i] - 1;
            puts("  ");
            put_dec(rates[i]);
            puts(" -> div=");
            put_dec(d);
            putchar('\n');
        }
    }
}

static void cmd_baud_set(const char *arg)
{
    unsigned int clk_freq, baud, div;
    arg = skip_spaces(arg);
    baud = parse_dec(arg);
    if (baud == 0) {
        puts("Usage: b <baud>\n");
        return;
    }
    __asm__ volatile("csrr %0, 0xBC0" : "=r"(clk_freq));
    div = (clk_freq * 1000000u) / baud - 1;
    __asm__ volatile("csrw 0xBC2, %0" :: "r"(div));
    puts("Baud set to ");
    put_dec(baud);
    puts(" (div=");
    put_dec(div);
    puts(")\n");
}

static void cmd_read(const char *arg)
{
    unsigned int addr, val;
    arg = skip_spaces(arg);
    if (!*arg) {
        puts("Usage: r <addr>\n");
        return;
    }
    addr = parse_hex(arg);
    val = *(volatile unsigned int *)addr;
    puts("[0x");
    put_hex(addr);
    puts("] = 0x");
    put_hex(val);
    putchar('\n');
}

static void cmd_write(const char *arg)
{
    unsigned int addr, data;
    const char *p;
    arg = skip_spaces(arg);
    if (!*arg) {
        puts("Usage: w <addr> <data>\n");
        return;
    }
    addr = parse_hex(arg);
    p = skip_spaces(skip_token(arg));
    if (!*p) {
        puts("Usage: w <addr> <data>\n");
        return;
    }
    data = parse_hex(p);
    *(volatile unsigned int *)addr = data;
    puts("[0x");
    put_hex(addr);
    puts("] <- 0x");
    put_hex(data);
    putchar('\n');
}

static void cmd_jump(const char *arg)
{
    unsigned int addr;
    arg = skip_spaces(arg);
    if (!*arg) {
        puts("Usage: j <addr>\n");
        return;
    }
    addr = parse_hex(arg);
    puts("Jumping to 0x");
    put_hex(addr);
    putchar('\n');
    ((void (*)(void))addr)();
}

static void cmd_load(const char *arg)
{
    unsigned int dest = 0x40000000u;  /* program space base */
    unsigned int total = 0;
    unsigned int chunk;
    const char *path;

    arg = skip_spaces(arg);
    if (!*arg) {
        puts("Usage: l <path>  (e.g. l 0:/BOOT/PROG.BIN)\n");
        return;
    }
    path = arg;

    /* Open file */
    {
        struct ecall_ret r = bios_sd_open((unsigned int)path);
        if (r.a0 == 0xFFFFFFFFu) {
            puts("File not found\n");
            return;
        }
        puts("Loading ");
        put_dec(r.a0);
        puts(" bytes...\n");
    }

    /* Read file into SDRAM program space */
    for (;;) {
        struct ecall_ret r = bios_sd_read(dest + total, 512);
        chunk = r.a0;
        if (chunk == 0)
            break;
        total += chunk;
    }

    bios_sd_close();

    puts("Loaded ");
    put_dec(total);
    puts(" bytes at 0x");
    put_hex(dest);
    putchar('\n');

    /* Check for JZEX header: "JZEX" magic = 0x5845_5A4A little-endian */
    {
        unsigned int entry = dest;
        volatile unsigned int *p = (volatile unsigned int *)dest;
        if (*p == 0x58455A4Au) {
            puts("JZEX format detected\n");
            entry = dest + 4;
        }
        puts("Jumping...\n");
        ((void (*)(void))entry)();
    }
}

static void cmd_dump(const char *arg)
{
    unsigned int addr, count, i;
    const char *p;
    arg = skip_spaces(arg);
    if (!*arg) {
        puts("Usage: d <addr> <count>\n");
        return;
    }
    addr = parse_hex(arg);
    p = skip_spaces(skip_token(arg));
    count = *p ? parse_dec(p) : 16;
    addr &= ~3u;  /* word-align */
    for (i = 0; i < count; i++) {
        if ((i & 3) == 0) {
            put_hex(addr + i * 4);
            puts(": ");
        }
        put_hex(*(volatile unsigned int *)(addr + i * 4));
        if ((i & 3) == 3 || i == count - 1)
            putchar('\n');
        else
            putchar(' ');
    }
}

static void cmd_csrdump(void)
{
    unsigned int val;
    puts("CSR Dump:\n");

    __asm__ volatile("csrr %0, 0xB00" : "=r"(val));
    puts("  mcycle     = 0x"); put_hex(val); putchar('\n');

    __asm__ volatile("csrr %0, mstatus" : "=r"(val));
    puts("  mstatus    = 0x"); put_hex(val); putchar('\n');

    __asm__ volatile("csrr %0, mtvec" : "=r"(val));
    puts("  mtvec      = 0x"); put_hex(val); putchar('\n');

    __asm__ volatile("csrr %0, mepc" : "=r"(val));
    puts("  mepc       = 0x"); put_hex(val); putchar('\n');

    __asm__ volatile("csrr %0, mcause" : "=r"(val));
    puts("  mcause     = 0x"); put_hex(val); putchar('\n');

    __asm__ volatile("csrr %0, mie" : "=r"(val));
    puts("  mie        = 0x"); put_hex(val); putchar('\n');

    __asm__ volatile("csrr %0, 0xBC0" : "=r"(val));
    puts("  clk_freq   = 0x"); put_hex(val); puts(" (");
    put_dec(val); puts("MHz)\n");

    __asm__ volatile("csrr %0, 0xBC1" : "=r"(val));
    puts("  video_mode = 0x"); put_hex(val); puts(" (");
    put_dec(val); puts(")\n");

    __asm__ volatile("csrr %0, 0xBC2" : "=r"(val));
    puts("  baud_div   = 0x"); put_hex(val); puts(" (");
    put_dec(val); puts(")\n");

    __asm__ volatile("csrr %0, 0xBC3" : "=r"(val));
    puts("  sdram_size = 0x"); put_hex(val); puts(" (");
    put_dec(val >> 20); puts("MB)\n");

    __asm__ volatile("csrr %0, 0xBC4" : "=r"(val));
    puts("  uartvec    = 0x"); put_hex(val); putchar('\n');

    __asm__ volatile("csrr %0, 0xBC5" : "=r"(val));
    puts("  sdcardvec  = 0x"); put_hex(val); putchar('\n');

    __asm__ volatile("csrr %0, 0xFC0" : "=r"(val));
    puts("  irq_pend   = 0x"); put_hex(val); putchar('\n');
}

static void bios_shell(void)
{
    char line[80];

    /* Register UART RX handler to buffer incoming chars */
    {
        unsigned int handler = (unsigned int)uart_rx_to_buf;
        __asm__ volatile("csrw 0xBC4, %0" :: "r"(handler));
    }

    /* Blue screen, white text */
    term_set_color(WHITE_FG, BLUE_BG);
    term_clear();
    puts("JZ-HDL SOC BIOS v1.0\n\n");

    for (;;) {
        const char *cmd, *arg;
        puts("> ");
        shell_readline(line, sizeof(line));

        cmd = skip_spaces(line);
        if (!*cmd)
            continue;

        arg = skip_spaces(skip_token(cmd));

        if (cmd[0] == '?' && (cmd[1] == '\0' || cmd[1] == ' ')) {
            cmd_help();
        } else if (cmd[0] == 'v' && (cmd[1] == '\0' || cmd[1] == ' ')) {
            cmd_video(arg);
        } else if (cmd[0] == 'b' && (cmd[1] == '\0' || cmd[1] == ' ')) {
            if (*arg)
                cmd_baud_set(arg);
            else
                cmd_baud_show();
        } else if (cmd[0] == 'r' && cmd[1] == 'd' && (cmd[2] == '\0' || cmd[2] == ' ')) {
            cmd_csrdump();
        } else if (cmd[0] == 'r' && (cmd[1] == '\0' || cmd[1] == ' ')) {
            cmd_read(arg);
        } else if (cmd[0] == 'w' && (cmd[1] == '\0' || cmd[1] == ' ')) {
            cmd_write(arg);
        } else if (cmd[0] == 'j' && (cmd[1] == '\0' || cmd[1] == ' ')) {
            cmd_jump(arg);
        } else if (cmd[0] == 'l' && cmd[1] == 's' && (cmd[2] == '\0' || cmd[2] == ' ')) {
            cmd_ls();
        } else if (cmd[0] == 'l' && (cmd[1] == '\0' || cmd[1] == ' ')) {
            cmd_load(arg);
        } else if (cmd[0] == 'd' && (cmd[1] == '\0' || cmd[1] == ' ')) {
            cmd_dump(arg);
        } else if (cmd[0] == 'p' && cmd[1] == 'w' && cmd[2] == 'd' && (cmd[3] == '\0' || cmd[3] == ' ')) {
            cmd_pwd();
        } else if (cmd[0] == 'p' && (cmd[1] == '\0' || cmd[1] == ' ')) {
            cmd_part();
        } else if (cmd[0] == 'c' && cmd[1] == 'd' && (cmd[2] == '\0' || cmd[2] == ' ')) {
            cmd_cd(arg);
        } else {
            puts("Unknown command. Type ? for help.\n");
        }
    }
}

/* ----------------------------------------------------------------
 * Main - POST and Boot Flow
 * ---------------------------------------------------------------- */
void main(void)
{
    unsigned int clk_freq, vmode, sdram_size, baud_div;
    unsigned int status;
    int timeout;
    int sdram_row;

    LED = 0x01;  /* LED 0 = start */
    fs_file_open = 0;

    /* Step 1: Init terminal */
    init_terminal();
    LED = 0x03;  /* LED 1 = terminal ok */

    /* Line 1: System identification */
    term_set_color(CYAN_FG, BLACK_BG);
    __asm__ volatile("csrr %0, 0xBC0" : "=r"(clk_freq));
    puts("JZ-HDL SOC - RV32IM @ ");
    put_dec(clk_freq);
    puts("MHz\n");

    /* Line 2: Video output info */
    __asm__ volatile("csrr %0, 0xBC1" : "=r"(vmode));
    term_set_color(WHITE_FG, BLACK_BG);
    if (vmode & 1) {
        puts("HDMI 1920x1080 120x33\n");
    } else {
        puts("HDMI 1280x720 80x22\n");
    }

    /* Line 3: SDRAM test - format: "SDRAM 8MB: ###% Write/Read" */
    __asm__ volatile("csrr %0, 0xBC3" : "=r"(sdram_size));
    puts("SDRAM ");
    put_dec(sdram_size >> 20);
    puts("MB: ");
    sdram_row = cursor_row;
    {
        int progress_col = cursor_col;
        if (sdram_test(sdram_size, sdram_row, progress_col)) {
            /* Test failed - halt */
            LED = 0x15;  /* error pattern */
            for (;;)
                __asm__ volatile("" ::: "memory");
        }
        /* Final: show OK */
        sdram_progress(sdram_row, progress_col, 100, "OK");
    }
    /* Move to next line */
    cursor_col = 0;
    cursor_row = sdram_row + 1;
    LED = 0x07;  /* LED 2 = SDRAM ok */

    /* Line 4: UART settings */
    __asm__ volatile("csrr %0, 0xBC2" : "=r"(baud_div));
    puts("UART: 115200 8N1 (div=");
    put_dec(baud_div);
    puts(")\n");
    LED = 0x0F;  /* LED 3 = UART ok */

    /* Line 5: SD card status */
    puts("SD Card: ");
    timeout = 0;
    for (;;) {
        status = SD_STATUS;
        if (!(status & SD_BUSY) && (status & (SD_READY | SD_ERROR)))
            break;
        timeout++;
        if (timeout > 500000) {
            puts("TIMEOUT\n");
            goto post_done;
        }
    }
    if (status & SD_ERROR) {
        puts("ERROR\n");
        goto post_done;
    }
    puts("READY");
    if (status & SD_SDHC)
        puts(" [SDHC]");
    putchar('\n');
    LED = 0x1F;  /* LED 4 = SD ok */

    /* Cache MBR partition table */
    fs_cache_mbr();

post_done:
    LED = 0x3F;  /* All LEDs = POST complete */

    /* Separator */
    term_set_color(GREEN_FG, BLACK_BG);
    {
        int i;
        int sep_len = (term_cols < 80) ? term_cols : 80;
        for (i = 0; i < sep_len; i++)
            putchar('-');
        putchar('\n');
    }

    /* ESC wait: 2 seconds — use ring buffer handler so IRQ doesn't eat chars */
    term_set_color(YELLOW_FG, BLACK_BG);
    puts("Press ESC to enter BIOS...");
    term_set_color(WHITE_FG, BLACK_BG);

    {
        unsigned int handler = (unsigned int)uart_rx_to_buf;
        __asm__ volatile("csrw 0xBC4, %0" :: "r"(handler));
    }

    {
        unsigned int start = read_mcycle();
        unsigned int elapsed;
        unsigned int limit;
        int esc_pressed = 0;

        limit = clk_freq * 2000000u;  /* 2 seconds worth of cycles */

        while (!esc_pressed) {
            elapsed = read_mcycle() - start;
            if (elapsed >= limit)
                break;

            /* Check ring buffer for ESC */
            while (rx_head != rx_tail) {
                unsigned char ch = rx_buf[rx_tail];
                rx_tail = (rx_tail + 1) % RX_BUF_SIZE;
                if (ch == 0x1B) {
                    esc_pressed = 1;
                    break;
                }
            }
        }

        LED = 0x00;  /* LEDs off, system running */

        putchar('\n');

        if (esc_pressed) {
            bios_shell();
            /* bios_shell never returns */
        } else {
            /* Try to load kernel from SD card */
            if (mbr_valid) {
                struct ecall_ret r = bios_sd_open((unsigned int)"0:/KERNEL.BIN");
                if (r.a0 != 0xFFFFFFFFu) {
                    unsigned int dest = 0x40000000u;
                    unsigned int total = 0;
                    unsigned int entry;
                    puts("Loading KERNEL.BIN (");
                    put_dec(r.a0);
                    puts(" bytes)...\n");
                    for (;;) {
                        struct ecall_ret rd = bios_sd_read(dest + total, 512);
                        if (rd.a0 == 0)
                            break;
                        total += rd.a0;
                    }
                    bios_sd_close();
                    puts("Loaded ");
                    put_dec(total);
                    puts(" bytes. Starting...\n");
                    entry = dest;
                    if (*(volatile unsigned int *)dest == 0x58455A4Au)
                        entry = dest + 4;
                    ((void (*)(void))entry)();
                }
            }

            /* No kernel found */
            term_fg = WHITE_FG;
            term_bg = GREEN_BG;
            TERM_CURSOR = 0;  /* hide cursor */
            term_clear();
            cursor_col = 0;
            cursor_row = 1;

            term_puts("No kernel found. Reset to try again.\n");

            uart_putchar('N'); uart_putchar('K'); uart_putchar('\n');

            for (;;)
                __asm__ volatile("" ::: "memory");
        }
    }
}
