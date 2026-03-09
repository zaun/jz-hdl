/*
 * blink.c - RV32I program for the JZ-HDL SOC
 *
 * Memory map (byte addresses, RV32I):
 *   ROM:      0x00000000 - 0x00001FFF  (2048 words)
 *   RAM:      0x10000000 - 0x10004FFF  (5120 words, 20KB)
 *   LED:      0x20000000              (lower 6 bits)
 *   UART:     0x30000000              (write=TX byte, read={30'b0, rx_has_data, tx_ready})
 *   UART RX:  0x30000004              (read=RX byte, clears rx_has_data)
 *   SDRAM:    0x40000000 - 0x407FFFFF  (8MB)
 *   TERMINAL: 0x50000000              (cell*8+0=attr, cell*8+4=char)
 *   SD CARD:  0x60000000              (register interface, see below)
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
 * CSR 0xBC1: video_mode (0=720p 80x22, 1=1080p 120x33)
 * CSR 0xBC2: baud_div   (16-bit UART baud divider, default 644 = 115200 baud @ 74.25 MHz)
 */

#define LED    (*(volatile unsigned int *)0x20000000)
#define UART   (*(volatile unsigned int *)0x30000000)
#define UART_RXDATA (*(volatile unsigned int *)0x30000004)

static volatile unsigned int *const RAM   = (volatile unsigned int *)0x10000000;
static volatile unsigned int *const SDRAM = (volatile unsigned int *)0x40000000;
static volatile unsigned int *const TERM  = (volatile unsigned int *)0x50000000;

/* SD Card registers */
#define SD_COMMAND   (*(volatile unsigned int *)0x60000000)
#define SD_STATUS    (*(volatile unsigned int *)0x60000004)
#define SD_SECTOR_LO (*(volatile unsigned int *)0x60000008)
#define SD_SECTOR_HI (*(volatile unsigned int *)0x6000000C)
#define SD_DATA      (*(volatile unsigned int *)0x60000014)
#define SD_IRQ_CTRL  (*(volatile unsigned int *)0x60000018)

/* SD status bits */
#define SD_BUSY   0x01
#define SD_ERROR  0x02
#define SD_READY  0x04
#define SD_SDHC   0x08

/* SD commands */
#define SD_CMD_NONE  0
#define SD_CMD_READ  1
#define SD_CMD_WRITE 2

/* Max terminal size (1080p: 120x33 = 3960 cells) */
#define MAX_TERM_CELLS 3960

/* RGB565 colors */
#define WHITE_FG   0xFFFF
#define BLACK_BG   0x0000
#define GREEN_FG   0x07E0
#define YELLOW_FG  0xFFE0
#define CYAN_FG    0x07FF

/* Terminal dimensions (set at boot from CSR) */
static int term_cols;
static int term_rows;
static int term_cells;

/* Multiply row by term_cols without hardware multiply.
 * 720p: cols=80  = 64+16       → (r<<6)+(r<<4)
 * 1080p: cols=120 = 128-8      → (r<<7)-(r<<3) */
static int row_to_cell(int r)
{
    if (term_cols == 120)
        return (r << 7) - (r << 3);
    else
        return (r << 6) + (r << 4);
}

/* Terminal state */
static int cursor_col;
static int cursor_row;
static unsigned int term_fg;
static unsigned int term_bg;

/* Shadow framebuffer in RAM for scrolling (terminal is write-only from bus).
 * Layout: shadow_char[cell], shadow_attr[cell]
 * Sized for max mode (3960 cells = 3960 + 15840 = 19800 bytes) */
static unsigned char  shadow_char[MAX_TERM_CELLS];
static unsigned int   shadow_attr[MAX_TERM_CELLS];

static void term_flush_cell(int cell)
{
    TERM[cell * 2]     = shadow_attr[cell];
    TERM[cell * 2 + 1] = shadow_char[cell];
}

static void term_write_cell(int col, int row, unsigned int ch, unsigned int fg, unsigned int bg)
{
    int cell = row_to_cell(row) + col;
    shadow_char[cell] = (unsigned char)ch;
    shadow_attr[cell] = (bg << 16) | fg;
    term_flush_cell(cell);
}

static void term_clear(void)
{
    int i;
    unsigned int attr = (term_bg << 16) | term_fg;
    for (i = 0; i < term_cells; i++) {
        shadow_char[i] = ' ';
        shadow_attr[i] = attr;
        TERM[i * 2]     = attr;
        TERM[i * 2 + 1] = ' ';
    }
    cursor_col = 0;
    cursor_row = 0;
}

static void term_scroll(void)
{
    int r, c, src, dst;
    /* Move rows 1..N-1 up to 0..N-2 using shadow buffer */
    for (r = 0; r < term_rows - 1; r++) {
        for (c = 0; c < term_cols; c++) {
            src = row_to_cell(r + 1) + c;
            dst = row_to_cell(r) + c;
            shadow_char[dst] = shadow_char[src];
            shadow_attr[dst] = shadow_attr[src];
            term_flush_cell(dst);
        }
    }
    /* Clear last row */
    unsigned int attr = (term_bg << 16) | term_fg;
    for (c = 0; c < term_cols; c++) {
        dst = row_to_cell(term_rows - 1) + c;
        shadow_char[dst] = ' ';
        shadow_attr[dst] = attr;
        term_flush_cell(dst);
    }
}

static void term_putchar(int c)
{
    if (c == '\n') {
        cursor_col = 0;
        cursor_row++;
    } else if (c == '\r') {
        cursor_col = 0;
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

static void set_video_mode(unsigned int mode)
{
    /* Write CSR 0xBC1 */
    __asm__ volatile("csrw 0xBC1, %0" :: "r"(mode));

    /* Update terminal dimensions */
    if (mode & 1) {
        term_cols = 120;
        term_rows = 33;
    } else {
        term_cols = 80;
        term_rows = 22;
    }
    term_cells = row_to_cell(term_rows);

    /* Clear and redraw banner */
    term_fg = WHITE_FG;
    term_bg = BLACK_BG;
    term_clear();

    term_set_color(CYAN_FG, BLACK_BG);
    term_puts("JZ-HDL SOC - RV32I @ 54MHz\n");
    term_set_color(GREEN_FG, BLACK_BG);
    if (mode & 1) {
        term_puts("UART echo active | HDMI 1920x1080 | 120x33 terminal\n");
    } else {
        term_puts("UART echo active | HDMI 1280x720 | 80x22 terminal\n");
    }
    term_set_color(WHITE_FG, BLACK_BG);
    term_puts("Press 1=720p, 2=1080p\n");
    term_puts("----------------------------------------\n");
}

/* Blue screen of death: show crash info and halt */
static void bsod(unsigned int mcause, unsigned int mepc)
{
    int i;
    unsigned int attr = (0x001F << 16) | WHITE_FG;  /* blue bg, white fg */

    /* Fill entire terminal with blue background */
    for (i = 0; i < term_cells; i++) {
        shadow_char[i] = ' ';
        shadow_attr[i] = attr;
        TERM[i * 2]     = attr;
        TERM[i * 2 + 1] = ' ';
    }

    /* Print crash info directly via terminal */
    term_fg = WHITE_FG;
    term_bg = 0x001F;
    cursor_col = 0;
    cursor_row = 1;

    term_puts("*** CRASH ***\n\n");
    term_puts("mcause: 0x");
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
    term_puts("\nmepc:   0x");
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

    /* Halt */
    for (;;)
        __asm__ volatile("" ::: "memory");
}

void trap_handler(void)
{
    unsigned int mcause, mepc, pending;
    __asm__ volatile("csrr %0, mcause" : "=r"(mcause));
    __asm__ volatile("csrr %0, mepc" : "=r"(mepc));
    __asm__ volatile("csrr %0, 0xFC0" : "=r"(pending));

    /* Check if this is an external interrupt (bit 31 set, cause 11) */
    if (mcause != 0x8000000Bu) {
        /* Not an external interrupt - real crash */
        bsod(mcause, mepc);
    }

    /* IRQ bits: 0=UART TX ready, 1=UART RX data, 2=SD card */

    if (pending & 0x2) {
        /* UART RX IRQ */
        unsigned int ch = UART_RXDATA;

        if (ch == '1') {
            set_video_mode(0);
        } else if (ch == '2') {
            set_video_mode(1);
        } else {
            /* Echo other characters to UART and terminal */
            while (!(UART & 1))
                ;
            UART = ch;
            term_putchar((int)ch);
        }
    }

    if (pending & 0x4) {
        /* SD card IRQ - acknowledge and clear */
        SD_IRQ_CTRL = 0x2;  /* bit 1 = clear */
    }

    /* UART TX ready (bit 0) - ignore */
}

/* Read a sector into SDRAM[0..255] buffer. Returns 0 on success. */
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
        SDRAM[w] = SD_DATA;
    return 0;
}

/* Read byte from SDRAM-buffered sector at byte offset */
static unsigned int sb(unsigned int offset)
{
    unsigned int word = SDRAM[offset >> 1];
    if (offset & 1)
        return (word >> 8) & 0xFF;
    return word & 0xFF;
}

/* Read 16-bit little-endian from sector buffer */
static unsigned int sb16(unsigned int offset)
{
    return SDRAM[offset >> 1] & 0xFFFF;
}

/* Read 32-bit little-endian from sector buffer */
static unsigned int sb32(unsigned int offset)
{
    return (SDRAM[(offset >> 1) + 1] << 16) | (SDRAM[offset >> 1] & 0xFFFF);
}

/* Print a hex nibble count worth of hex digits */
static void put_hex_short(unsigned int val, int nibbles)
{
    int i;
    char buf[9];
    for (i = nibbles - 1; i >= 0; i--) {
        unsigned int nib = val & 0xF;
        buf[i] = (nib < 10) ? ('0' + nib) : ('A' - 10 + nib);
        val >>= 4;
    }
    buf[nibbles] = '\0';
    puts(buf);
}

/* Print trimmed 8.3 filename from sector buffer at given offset */
static void put_fname83(unsigned int off)
{
    int i, last;
    /* Print name (8 chars, trim trailing spaces) */
    last = 7;
    while (last >= 0 && sb(off + (unsigned int)last) == ' ')
        last--;
    for (i = 0; i <= last; i++)
        putchar((int)sb(off + (unsigned int)i));
    /* Print extension (3 chars, trim trailing spaces) */
    last = 2;
    while (last >= 0 && sb(off + 8 + (unsigned int)last) == ' ')
        last--;
    if (last >= 0) {
        putchar('.');
        for (i = 0; i <= last; i++)
            putchar((int)sb(off + 8 + (unsigned int)i));
    }
}

/* Check if partition type is FAT */
static int is_fat_type(unsigned int t)
{
    return t == 0x01 || t == 0x04 || t == 0x06 ||
           t == 0x0B || t == 0x0C || t == 0x0E;
}

/* SD card volume listing */
static void sd_list_volumes(void)
{
    unsigned int status, part_lba;
    int timeout, i, found;

    puts("SD: Waiting for init...");
    timeout = 0;
    for (;;) {
        status = SD_STATUS;
        if (!(status & SD_BUSY) && (status & (SD_READY | SD_ERROR)))
            break;
        timeout++;
        if (timeout > 500000) {
            puts(" TIMEOUT\n");
            return;
        }
    }
    if (status & SD_ERROR) {
        puts(" FAIL\n");
        return;
    }
    puts(" OK");
    if (status & SD_SDHC)
        puts(" [SDHC]");
    puts("\n");

    /* Read MBR */
    if (sd_read_sector(0)) {
        puts("SD: Read MBR failed\n");
        return;
    }

    /* Verify MBR signature */
    if (sb16(510) != 0xAA55) {
        puts("SD: Bad MBR signature\n");
        return;
    }

    /* Parse partition table */
    puts("SD: Partitions:\n");
    term_set_color(CYAN_FG, BLACK_BG);
    part_lba = 0;
    found = 0;
    for (i = 0; i < 4; i++) {
        unsigned int base = 446 + (unsigned int)(i << 4);
        unsigned int ptype = sb(base + 4);
        unsigned int plba = sb32(base + 8);
        unsigned int psize = sb32(base + 12);
        if (ptype == 0)
            continue;
        putchar(' ');
        putchar('0' + i);
        puts(": type=0x");
        put_hex_short(ptype, 2);
        puts(" LBA=0x");
        put_hex(plba);
        puts(" size=0x");
        put_hex(psize);
        putchar('\n');
        if (!found && is_fat_type(ptype)) {
            part_lba = plba;
            found = 1;
        }
    }
    term_set_color(WHITE_FG, BLACK_BG);

    if (!found) {
        puts("SD: No FAT partition found\n");
        return;
    }

    /* Read FAT boot sector */
    if (sd_read_sector(part_lba)) {
        puts("SD: Read FAT boot sector failed\n");
        return;
    }

    {
        unsigned int reserved, num_fats, fat_size_16, fat_size_32;
        unsigned int spc, fat_total, root_dir_lba, is_fat32;
        unsigned int root_cluster, data_start;
        int j, s;

        spc = sb(13);
        reserved = sb16(14);
        num_fats = sb(16);
        fat_size_16 = sb16(22);

        is_fat32 = (fat_size_16 == 0);
        if (is_fat32) {
            fat_size_32 = sb32(36);
            root_cluster = sb32(44);
        } else {
            fat_size_32 = 0;
            root_cluster = 0;
        }

        /* Print volume label */
        puts("SD: Volume: ");
        term_set_color(YELLOW_FG, BLACK_BG);
        {
            unsigned int label_off = is_fat32 ? 71 : 43;
            for (j = 0; j < 11; j++)
                putchar((int)sb(label_off + (unsigned int)j));
        }
        term_set_color(WHITE_FG, BLACK_BG);
        puts(is_fat32 ? " [FAT32]\n" : " [FAT16]\n");

        /* Calculate fat_total = fat_size * num_fats (num_fats is 1 or 2) */
        {
            unsigned int fs = is_fat32 ? fat_size_32 : fat_size_16;
            fat_total = fs;
            if (num_fats >= 2)
                fat_total += fs;
        }

        /* Calculate root directory LBA */
        if (is_fat32) {
            data_start = part_lba + reserved + fat_total;
            /* (root_cluster - 2) * spc using shift (spc is power of 2) */
            {
                unsigned int shift = 0, tmp = spc;
                while (tmp > 1) { tmp >>= 1; shift++; }
                root_dir_lba = data_start + ((root_cluster - 2) << shift);
            }
        } else {
            root_dir_lba = part_lba + reserved + fat_total;
        }

        /* List root directory (2 sectors = up to 32 entries) */
        puts("SD: Root directory:\n");
        term_set_color(CYAN_FG, BLACK_BG);
        for (s = 0; s < 2; s++) {
            if (sd_read_sector(root_dir_lba + (unsigned int)s)) {
                puts("  Read error\n");
                break;
            }
            for (j = 0; j < 16; j++) {
                unsigned int eoff = (unsigned int)(j << 5); /* j * 32 */
                unsigned int first = sb(eoff);
                unsigned int attr = sb(eoff + 11);
                if (first == 0x00)
                    goto done;      /* end of directory */
                if (first == 0xE5)
                    continue;       /* deleted */
                if (attr == 0x0F)
                    continue;       /* LFN entry */
                puts("  ");
                put_fname83(eoff);
                if (attr & 0x08) {
                    puts(" <VOL>");
                } else if (attr & 0x10) {
                    puts(" <DIR>");
                } else {
                    unsigned int fsize = sb32(eoff + 28);
                    puts(" 0x");
                    put_hex(fsize);
                }
                putchar('\n');
            }
        }
done:
        term_set_color(WHITE_FG, BLACK_BG);
    }
}

#define RAM_COUNT    64
#define SDRAM_COUNT  10

/* Knight rider pattern: single LED bounces across 6 LEDs */
static const unsigned int knight_rider[SDRAM_COUNT] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x10, 0x08, 0x04, 0x02
};

static void delay(void)
{
    int outer, inner;
    for (outer = 23; outer > 0; outer--) {
        for (inner = 0xFFFF; inner > 0; inner--) {
            __asm__ volatile("" ::: "memory");
        }
    }
}

void main(void)
{
    int i, pass, tick;

    /* Read video mode CSR to determine terminal size */
    unsigned int vmode;
    __asm__ volatile("csrr %0, 0xBC1" : "=r"(vmode));
    if (vmode & 1) {
        /* 1080p mode: 120x33 */
        term_cols = 120;
        term_rows = 33;
    } else {
        /* 720p mode: 80x22 */
        term_cols = 80;
        term_rows = 22;
    }
    term_cells = row_to_cell(term_rows);

    /* Init terminal */
    term_fg = WHITE_FG;
    term_bg = BLACK_BG;
    term_clear();

    /* Banner */
    term_set_color(CYAN_FG, BLACK_BG);
    term_puts("JZ-HDL SOC - RV32I @ 54MHz\n");
    term_set_color(GREEN_FG, BLACK_BG);
    if (vmode & 1) {
        term_puts("UART echo active | HDMI 1920x1080 | 120x33 terminal\n");
    } else {
        term_puts("UART echo active | HDMI 1280x720 | 80x22 terminal\n");
    }
    term_set_color(WHITE_FG, BLACK_BG);
    term_puts("Press 1=720p, 2=1080p\n");
    term_puts("----------------------------------------\n");

    /* UART-only init messages */
    uart_putchar('I'); uart_putchar('n'); uart_putchar('i'); uart_putchar('t'); uart_putchar('\n');

    /* Check SD card and list volumes */
    sd_list_volumes();

    /* Fill RAM with counting pattern */
    for (i = 0; i < RAM_COUNT; i++)
        RAM[i] = (unsigned int)i;

    /* Fill SDRAM with knight rider pattern */
    for (i = 0; i < SDRAM_COUNT; i++)
        SDRAM[i] = knight_rider[i];

    puts("Init done\n");

    /* --- Main display loop --- */
    tick = 0;

    for (;;) {
        /* Pattern 1: counting from Block RAM */
        for (i = 0; i < RAM_COUNT; i++) {
            LED = RAM[i];
            delay();
        }

        term_set_color(YELLOW_FG, BLACK_BG);
        puts("Tick ");
        put_hex((unsigned int)tick);
        puts(" - LED pattern complete\n");
        term_set_color(WHITE_FG, BLACK_BG);

        /* Pattern 2: knight rider from SDRAM (play twice) */
        for (pass = 0; pass < 2; pass++) {
            for (i = 0; i < SDRAM_COUNT; i++) {
                LED = SDRAM[i];
                delay();
            }
        }

        puts("Tick ");
        put_hex((unsigned int)tick);
        puts(" - Knight rider done\n");

        tick++;
    }
}
