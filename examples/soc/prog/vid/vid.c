/*
 * vid.c - Change video display mode and clear screen
 *
 * Relocatable JZEX program. Sets the terminal video mode and clears screen.
 *
 * BIOS ECALLs used:
 *   0x02: tty_size(device)             — get terminal rows/cols
 *   0x03: tty_write(dev, buf, len)     — write to terminal
 *   0x04: tty_clear(device)            — clear terminal
 *   0x07: tty_bg(device, 0x00RRGGBB)   — set background color
 *   0x08: vid_mode(mode)               — set video mode
 *
 * Usage: vid [0|1] [-c [-b #RRGGBB]]
 *   0 = 720p  (80x22)
 *   1 = 1080p (120x33)
 *   -c         clear screen to black
 *   -c -b #HHH clear screen to background color
 *   No arg: show current mode
 */

const char help_text[] __attribute__((section(".rodata.help"))) =
    "vid [0|1] [-c [-b #RRGGBB]] - video mode and clear\n"
    "  0          set 720p  (80x22)\n"
    "  1          set 1080p (120x33)\n"
    "  -c         clear screen to black\n"
    "  -c -b #HHH clear to background color (#RRGGBB)\n"
    "  No arg: show current mode\n";

/* ----------------------------------------------------------------
 * ECALL wrappers
 * ---------------------------------------------------------------- */

struct ecall_ret {
    unsigned int a0;
    unsigned int a1;
};

static struct ecall_ret ecall1(unsigned int num, unsigned int a1)
{
    struct ecall_ret ret;
    register unsigned int r_a0 __asm__("a0") = num;
    register unsigned int r_a1 __asm__("a1") = a1;
    __asm__ volatile("ecall"
        : "+r"(r_a0), "+r"(r_a1)
        :
        : "memory", "a2", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6");
    ret.a0 = r_a0;
    ret.a1 = r_a1;
    return ret;
}

static struct ecall_ret ecall2(unsigned int num, unsigned int a1, unsigned int a2)
{
    struct ecall_ret ret;
    register unsigned int r_a0 __asm__("a0") = num;
    register unsigned int r_a1 __asm__("a1") = a1;
    register unsigned int r_a2 __asm__("a2") = a2;
    __asm__ volatile("ecall"
        : "+r"(r_a0), "+r"(r_a1), "+r"(r_a2)
        :
        : "memory", "a3", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6");
    ret.a0 = r_a0;
    ret.a1 = r_a1;
    return ret;
}

static struct ecall_ret ecall3(unsigned int num, unsigned int a1,
                               unsigned int a2, unsigned int a3)
{
    struct ecall_ret ret;
    register unsigned int r_a0 __asm__("a0") = num;
    register unsigned int r_a1 __asm__("a1") = a1;
    register unsigned int r_a2 __asm__("a2") = a2;
    register unsigned int r_a3 __asm__("a3") = a3;
    __asm__ volatile("ecall"
        : "+r"(r_a0), "+r"(r_a1), "+r"(r_a2), "+r"(r_a3)
        :
        : "memory", "a4", "a5", "a6", "a7",
          "t0", "t1", "t2", "t3", "t4", "t5", "t6");
    ret.a0 = r_a0;
    ret.a1 = r_a1;
    return ret;
}

/* BIOS: tty_write (device=3=both) */
static void tty_write(const char *s, unsigned int len)
{
    ecall3(0x03, 3, (unsigned int)s, len);
}

/* BIOS: tty_clear (device=1=terminal) */
static void tty_clear(void)
{
    ecall1(0x04, 1);
}

/* BIOS: tty_bg (device=1=terminal) */
static void tty_bg(unsigned int color)
{
    ecall2(0x07, 1, color);
}

/* BIOS: tty_size — returns rows in a0, cols in a1 */
static void tty_size(int *rows, int *cols)
{
    struct ecall_ret r = ecall1(0x02, 1);
    *rows = (int)r.a0;
    *cols = (int)r.a1;
}

/* BIOS: vid_mode — set video mode, returns prev mode in a1 */
static unsigned int vid_mode(unsigned int mode)
{
    struct ecall_ret r = ecall1(0x08, mode);
    return r.a1;
}

/* ----------------------------------------------------------------
 * String / output helpers
 * ---------------------------------------------------------------- */

static unsigned int strlen(const char *s)
{
    unsigned int n = 0;
    while (*s++) n++;
    return n;
}

static void puts(const char *s)
{
    tty_write(s, strlen(s));
}

static void put_dec(unsigned int v)
{
    char buf[6];
    int i = 5;
    buf[5] = '\0';
    if (v == 0) {
        buf[--i] = '0';
    } else {
        while (v > 0 && i > 0) {
            buf[--i] = '0' + (char)(v % 10);
            v /= 10;
        }
    }
    puts(&buf[i]);
}

/* Parse a hex digit, return -1 on invalid */
static int hex_digit(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Parse #RRGGBB hex color string, return 0x00RRGGBB or -1 on error */
static int parse_color(const char *s, int len)
{
    unsigned int color = 0;
    int i, start = 0;

    if (len < 1)
        return -1;

    /* Skip leading '#' */
    if (s[0] == '#') {
        start = 1;
    }

    if (len - start != 6)
        return -1;

    for (i = start; i < len; i++) {
        int d = hex_digit(s[i]);
        if (d < 0)
            return -1;
        color = (color << 4) | (unsigned int)d;
    }
    return (int)color;
}

/* ----------------------------------------------------------------
 * Argument parsing helpers
 * ---------------------------------------------------------------- */

/* Skip whitespace, return new position */
static int skip_spaces(const char *args, int pos, int len)
{
    while (pos < len && args[pos] == ' ')
        pos++;
    return pos;
}

/* Return length of current token (non-space chars) */
static int token_len(const char *args, int pos, int len)
{
    int start = pos;
    while (pos < len && args[pos] != ' ')
        pos++;
    return pos - start;
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

void main(const char *args, unsigned int args_len)
{
    int pos = 0;
    int len = (int)args_len;
    int do_clear = 0;
    int has_bg = 0;
    unsigned int bg_color = 0x00000000;  /* black */

    if (len == 0) {
        /* No arg: show current mode */
        int rows, cols;
        tty_size(&rows, &cols);
        puts("Current mode: ");
        if (cols >= 120)
            puts("1 (1080p ");
        else
            puts("0 (720p ");
        put_dec((unsigned int)cols);
        puts("x");
        put_dec((unsigned int)rows);
        puts(")\n");
        return;
    }

    /* Parse arguments */
    while (pos < len) {
        int tlen;
        pos = skip_spaces(args, pos, len);
        if (pos >= len)
            break;
        tlen = token_len(args, pos, len);

        if (tlen == 1 && args[pos] == '0') {
            vid_mode(0);
            pos += tlen;
        } else if (tlen == 1 && args[pos] == '1') {
            vid_mode(1);
            pos += tlen;
        } else if (tlen == 2 && args[pos] == '-' && args[pos + 1] == 'c') {
            do_clear = 1;
            pos += tlen;
        } else if (tlen == 2 && args[pos] == '-' && args[pos + 1] == 'b') {
            pos += tlen;
            /* Next token should be the color */
            pos = skip_spaces(args, pos, len);
            if (pos < len) {
                int clen = token_len(args, pos, len);
                int color = parse_color(args + pos, clen);
                if (color >= 0) {
                    has_bg = 1;
                    bg_color = (unsigned int)color;
                } else {
                    puts("Invalid color. Use #RRGGBB format.\n");
                    return;
                }
                pos += clen;
            } else {
                puts("Missing color after -b. Use #RRGGBB format.\n");
                return;
            }
        } else {
            puts("Usage: vid [0|1] [-c [-b #RRGGBB]]\n");
            puts("  0          set 720p  (80x22)\n");
            puts("  1          set 1080p (120x33)\n");
            puts("  -c         clear screen to black\n");
            puts("  -c -b #HHH clear to background color\n");
            return;
        }
    }

    if (do_clear) {
        if (has_bg)
            tty_bg(bg_color);
        else
            tty_bg(0x00000000);
        tty_clear();
    }
}
