/*
 * rm.c - Remove a file
 *
 * Relocatable JZEX program. Prompts for confirmation before deleting.
 *
 * Kernel ECALLs used:
 *   0x24: fs_remove(path_ptr)    — delete file
 *   0x31: env_get(key_ptr)       — get environment variable
 *   0x32: mem_return(ptr)        — free allocated memory
 *
 * BIOS ECALLs used:
 *   0x01: tty_read(buf, len)     — read from terminal
 *   0x03: tty_write(dev, buf, len) — write to terminal
 *
 * Usage: rm <filename>
 */

const char help_text[] __attribute__((section(".rodata.help"))) =
    "rm <filename> - remove a file\n"
    "  Prompts for Y/N confirmation before deleting.\n";

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

/* Kernel: fs_remove */
static int fs_remove(const char *path)
{
    struct ecall_ret r = ecall1(0x24, (unsigned int)path);
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
 * Main
 * ---------------------------------------------------------------- */

void main(const char *args, unsigned int args_len)
{
    char fullpath[128];
    char *pwd_buf;
    const char *pwd;
    int pwd_len, i, result;

    if (args_len == 0) {
        puts("Usage: rm <filename>\n");
        return;
    }

    /* Prompt for confirmation */
    puts("Remove ");
    tty_write(args, args_len);
    puts("? (Y/N) ");

    {
        int ch = getchar();
        putchar(ch);
        putchar('\n');
        if (ch != 'Y' && ch != 'y')
            return;
    }

    /* Build full path: PWD + filename */
    pwd_buf = env_get("PWD");
    pwd = pwd_buf ? pwd_buf : "0:/";
    pwd_len = (int)strlen(pwd);

    /* Check if args is already a full path */
    if (args_len >= 2 && args[1] == ':') {
        for (i = 0; i < (int)args_len && i < 126; i++)
            fullpath[i] = args[i];
        fullpath[i] = '\0';
    } else {
        /* Prepend PWD */
        for (i = 0; i < pwd_len && i < 120; i++)
            fullpath[i] = pwd[i];
        /* Ensure separator */
        if (i > 0 && fullpath[i - 1] != '/')
            fullpath[i++] = '/';
        {
            int j;
            for (j = 0; j < (int)args_len && i < 126; j++)
                fullpath[i++] = args[j];
        }
        fullpath[i] = '\0';
    }

    if (pwd_buf)
        mem_return(pwd_buf);

    result = fs_remove(fullpath);
    if (result == 0) {
        puts("Removed ");
        tty_write(args, args_len);
        putchar('\n');
    } else {
        puts("Failed to remove ");
        tty_write(args, args_len);
        putchar('\n');
    }
}
