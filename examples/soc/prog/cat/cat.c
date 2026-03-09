/*
 * cat.c - Display file contents with "more" pagination
 *
 * Relocatable JZEX program. Reads a file and displays its contents.
 * When the screen is full, pauses with "--More--" prompt.
 *
 * Kernel ECALLs used:
 *   0x21: sd_list(path_ptr, buf, max)  — list directory entries
 *   0x22: sd_read(cluster, dest, max)  — read file by cluster
 *   0x31: env_get(key_ptr)             — get environment variable
 *   0x32: mem_return(ptr)              — free memory
 *
 * BIOS ECALLs used:
 *   0x01: tty_read(buf, len)           — read from terminal
 *   0x02: tty_size(device)             — get terminal rows/cols
 *   0x03: tty_write(dev, buf, len)     — write to terminal
 *
 * Usage: cat <filename>
 */

const char help_text[] __attribute__((section(".rodata.help"))) =
    "cat <filename> - display file contents\n"
    "  Shows file with 'more' pagination.\n"
    "  Space=next page, Enter=next line, q=quit\n";

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

/* BIOS: tty_read */
static unsigned int tty_read(char *buf, unsigned int len)
{
    struct ecall_ret r = ecall2(0x01, (unsigned int)buf, len);
    return r.a0;
}

/* BIOS: tty_size — returns rows in a0, cols in a1 */
static void tty_size(int *rows, int *cols)
{
    struct ecall_ret r = ecall1(0x02, 1);  /* device=1 (terminal) */
    *rows = (int)r.a0;
    *cols = (int)r.a1;
}

/* Kernel: sd_list */
static int sd_list(const char *path, void *buf, int max_entries)
{
    struct ecall_ret r = ecall3(0x21, (unsigned int)path,
                                (unsigned int)buf, (unsigned int)max_entries);
    return (int)r.a0;
}

/* Kernel: sd_read — read file by cluster into dest, up to max_len bytes */
static int sd_read(unsigned int cluster, unsigned int dest, unsigned int max_len)
{
    struct ecall_ret r = ecall3(0x22, cluster, dest, max_len);
    return (int)r.a0;
}

/* Kernel: env_get — returns allocated string (must mem_return) */
static char *env_get(const char *key)
{
    struct ecall_ret r = ecall1(0x31, (unsigned int)key);
    return (char *)r.a0;
}

/* Kernel: mem_return */
static void mem_return(void *ptr)
{
    ecall1(0x32, (unsigned int)ptr);
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

static void putchar(int c)
{
    char ch = (char)c;
    tty_write(&ch, 1);
}

/* Case-insensitive string compare */
static int streqi(const char *a, int alen, const char *b, int blen)
{
    int i;
    if (alen != blen)
        return 0;
    for (i = 0; i < alen; i++) {
        char ca = a[i], cb = b[i];
        if (ca >= 'a' && ca <= 'z') ca -= 32;
        if (cb >= 'a' && cb <= 'z') cb -= 32;
        if (ca != cb)
            return 0;
    }
    return 1;
}

/* Blocking getchar */
static int getchar(void)
{
    char ch;
    for (;;) {
        if (tty_read(&ch, 1) > 0)
            return (int)(unsigned char)ch;
        __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop");
    }
}

/* ----------------------------------------------------------------
 * Directory entry struct (matches kernel's layout, 76 bytes)
 * ---------------------------------------------------------------- */

struct dir_entry {
    char         name[64];
    unsigned int size;
    unsigned int attr;
    unsigned int cluster;
};

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

/* File data buffer — use high SDRAM address, well above loaded program */
#define FILE_BUF_ADDR  0x40200000u
#define FILE_BUF_MAX   0x00100000u  /* 1MB max file size */

void main(const char *args, unsigned int args_len)
{
    char *pwd_buf;
    const char *pwd;
    struct dir_entry entries[32];
    int count, i, found;
    unsigned int file_cluster, file_size;
    int bytes_read;
    volatile unsigned char *data;
    int screen_rows, screen_cols;
    int lines_printed;

    if (args_len == 0) {
        puts("Usage: cat <filename>\n");
        return;
    }

    /* Get terminal size for pagination */
    tty_size(&screen_rows, &screen_cols);

    /* Get current directory */
    pwd_buf = env_get("PWD");
    pwd = pwd_buf ? pwd_buf : "0:/";

    /* List directory to find the file */
    count = sd_list(pwd, entries, 32);
    if (pwd_buf)
        mem_return(pwd_buf);

    if (count < 0) {
        puts("Cannot read directory\n");
        return;
    }

    /* Find matching file (case-insensitive) */
    found = 0;
    for (i = 0; i < count; i++) {
        if (entries[i].attr & 0x10)
            continue;  /* skip directories */
        if (streqi(args, (int)args_len, entries[i].name,
                   (int)strlen(entries[i].name))) {
            file_cluster = entries[i].cluster;
            file_size = entries[i].size;
            found = 1;
            break;
        }
    }

    if (!found) {
        puts("File not found: ");
        tty_write(args, args_len);
        putchar('\n');
        return;
    }

    if (file_size == 0) {
        return;
    }

    /* Read file into buffer */
    {
        unsigned int max = file_size;
        if (max > FILE_BUF_MAX)
            max = FILE_BUF_MAX;
        bytes_read = sd_read(file_cluster, FILE_BUF_ADDR, max);
    }
    if (bytes_read < 0) {
        puts("Read error\n");
        return;
    }

    /* Display file contents with pagination */
    data = (volatile unsigned char *)FILE_BUF_ADDR;
    lines_printed = 0;

    for (i = 0; i < bytes_read; i++) {
        unsigned char ch = data[i];

        if (ch == '\r')
            continue;  /* skip CR */

        if (ch == '\n') {
            putchar('\n');
            lines_printed++;
        } else if (ch >= 0x20 && ch < 0x7F) {
            putchar(ch);
        } else if (ch == '\t') {
            puts("    ");  /* tab = 4 spaces */
        } else {
            putchar('.');  /* non-printable */
        }

        /* Pagination: pause when screen is full (reserve 1 line for prompt) */
        if (lines_printed >= screen_rows - 1) {
            int key;
            puts("--More--");
            key = getchar();

            /* Clear the --More-- prompt */
            puts("\r        \r");

            if (key == 'q' || key == 'Q' || key == 0x1B) {
                putchar('\n');
                return;  /* quit */
            }

            if (key == ' ') {
                lines_printed = 0;  /* next page */
            } else {
                lines_printed = screen_rows - 2;  /* one more line */
            }
        }
    }

    /* Ensure final newline if file doesn't end with one */
    if (bytes_read > 0 && data[bytes_read - 1] != '\n')
        putchar('\n');
}
