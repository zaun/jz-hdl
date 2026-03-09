/*
 * ls.c - List directory contents
 *
 * Relocatable JZEX program. Uses kernel ECALLs for directory listing
 * and BIOS ECALLs for terminal output.
 *
 * Kernel ECALLs used:
 *   0x20: sd_list_count(path_ptr)             — count directory entries
 *   0x21: sd_list(path_ptr, buf, max_entries) — list directory into buffer
 *
 * Usage: ls [-a] [path]
 *   -a: show entries starting with '.'
 *   Default path: current directory (PWD)
 */

const char help_text[] __attribute__((section(".rodata.help"))) =
    "ls [-a] [path] - list directory contents\n"
    "  -a  show hidden files (starting with '.')\n"
    "  If no path given, lists current directory (PWD)\n"
    "  Attributes: R=readonly H=hidden S=system\n"
    "              V=volume D=directory A=archive\n"
    "  Upper=set, lower=unset\n";

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

/* Kernel: sd_list — fills buf, returns entry count or -1 */
static int sd_list(const char *path, void *buf, int max_entries)
{
    struct ecall_ret r = ecall3(0x21, (unsigned int)path,
                                (unsigned int)buf, (unsigned int)max_entries);
    return (int)r.a0;
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

/* Kernel: env_get — returns allocated string (must mem_return) */
static char *env_get(const char *key)
{
    struct ecall_ret r = ecall1(0x31, (unsigned int)key);
    return (char *)r.a0;
}

/* Kernel: mem_return — free allocated memory */
static void mem_return(void *ptr)
{
    ecall1(0x32, (unsigned int)ptr);
}

void main(const char *args, unsigned int args_len)
{
    const char *path;
    char *pwd_buf = (char *)0;
    int count, i, show_all = 0;
    struct dir_entry entries[32];

    /* Parse -a flag */
    if (args_len >= 2 && args[0] == '-' && args[1] == 'a') {
        show_all = 1;
        args += 2;
        args_len -= 2;
        while (args_len > 0 && *args == ' ') { args++; args_len--; }
    }

    if (args_len > 0) {
        path = args;
    } else {
        pwd_buf = env_get("PWD");
        path = pwd_buf ? pwd_buf : "0:/";
    }

    count = sd_list(path, entries, 32);
    if (count < 0) {
        puts("Cannot open ");
        puts(path);
        putchar('\n');
        return;
    }

    for (i = 0; i < count; i++) {
        if (!show_all && entries[i].name[0] == '.')
            continue;

        /* Size: 7-char right-aligned field */
        {
            char sizebuf[8];
            unsigned int v = entries[i].size;
            int j = 6;
            sizebuf[7] = '\0';
            if (v == 0) {
                for (j = 0; j < 6; j++) sizebuf[j] = ' ';
                sizebuf[6] = '0';
            } else {
                while (j >= 0 && v > 0) {
                    sizebuf[j--] = '0' + (char)(v % 10);
                    v /= 10;
                }
                while (j >= 0)
                    sizebuf[j--] = ' ';
            }
            puts(sizebuf);
        }

        /* Attributes: rhsvda (upper=ON, lower=OFF) */
        putchar(' ');
        {
            unsigned int a = entries[i].attr;
            putchar((a & 0x01) ? 'R' : 'r');
            putchar((a & 0x02) ? 'H' : 'h');
            putchar((a & 0x04) ? 'S' : 's');
            putchar((a & 0x08) ? 'V' : 'v');
            putchar((a & 0x10) ? 'D' : 'd');
            putchar((a & 0x20) ? 'A' : 'a');
        }

        putchar(' ');
        puts(entries[i].name);
        putchar('\n');
    }

    if (pwd_buf)
        mem_return(pwd_buf);
}
