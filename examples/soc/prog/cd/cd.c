/*
 * cd.c - Change working directory
 *
 * Relocatable JZEX program. Changes the PWD environment variable.
 *
 * Kernel ECALLs used:
 *   0x25: fs_dir_exists(path_ptr)  — check if directory exists (returns 1/0)
 *   0x30: env_set(key_ptr, val_ptr) — set environment variable
 *   0x31: env_get(key_ptr)          — get value (caller must mem_return)
 *   0x32: mem_return(ptr)           — free memory
 *
 * BIOS ECALLs used:
 *   0x03: tty_write(dev, buf, len)  — write to terminal
 *
 * Usage: cd [path]
 *   No arg:   print current directory
 *   ..        go up one directory
 *   0:/PATH   full path
 *   SUBDIR    relative to PWD
 */

const char help_text[] __attribute__((section(".rodata.help"))) =
    "cd [path] - change working directory\n"
    "  No arg:   print current directory\n"
    "  ..        go up one level\n"
    "  0:/PATH   absolute path\n"
    "  SUBDIR    relative to current directory\n";

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

/* Kernel: fs_dir_exists — returns 1 if directory exists, 0 if not */
static int fs_dir_exists(const char *path)
{
    struct ecall_ret r = ecall1(0x25, (unsigned int)path);
    return (int)r.a0;
}

/* Kernel: env_set */
static void env_set(const char *key, const char *val)
{
    ecall2(0x30, (unsigned int)key, (unsigned int)val);
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
 * String helpers
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

static void strcpy(char *dst, const char *src, int maxlen)
{
    int i = 0;
    while (src[i] && i < maxlen - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

void main(const char *args, unsigned int args_len)
{
    char newpath[128];
    char *pwd_buf;
    const char *pwd;

    if (args_len == 0) {
        /* No arg: print current directory */
        pwd_buf = env_get("PWD");
        if (pwd_buf) {
            puts(pwd_buf);
            mem_return(pwd_buf);
        }
        putchar('\n');
        return;
    }

    /* Get current PWD */
    pwd_buf = env_get("PWD");
    pwd = pwd_buf ? pwd_buf : "0:/";

    if (args_len >= 2 && args[1] == ':') {
        /* Full path (contains ':') — use directly */
        {
            int i;
            for (i = 0; i < (int)args_len && i < 126; i++)
                newpath[i] = args[i];
            newpath[i] = '\0';
        }
    } else if (args_len >= 2 && args[0] == '.' && args[1] == '.') {
        /* Go up one directory */
        int len = (int)strlen(pwd);
        strcpy(newpath, pwd, 128);
        /* Strip trailing slash if present */
        if (len > 0 && newpath[len - 1] == '/') {
            newpath[--len] = '\0';
        }
        /* Strip last component */
        while (len > 0 && newpath[len - 1] != '/' && newpath[len - 1] != ':') {
            len--;
        }
        /* Keep the slash after ':' */
        if (len > 0 && newpath[len - 1] == ':') {
            newpath[len] = '/';
            newpath[len + 1] = '\0';
        } else {
            newpath[len] = '\0';
        }
    } else {
        /* Relative path — append to current PWD */
        int len = (int)strlen(pwd);
        int i;
        strcpy(newpath, pwd, 128);
        /* Ensure trailing slash */
        if (len > 0 && newpath[len - 1] != '/') {
            newpath[len] = '/';
            newpath[len + 1] = '\0';
            len++;
        }
        /* Append relative path */
        for (i = 0; i < (int)args_len && len + i < 126; i++)
            newpath[len + i] = args[i];
        newpath[len + i] = '\0';
    }

    if (pwd_buf)
        mem_return(pwd_buf);

    /* Ensure trailing slash */
    {
        int len = (int)strlen(newpath);
        if (len > 0 && newpath[len - 1] != '/' && len < 126) {
            newpath[len] = '/';
            newpath[len + 1] = '\0';
        }
    }

    /* Validate path exists */
    if (!fs_dir_exists(newpath)) {
        puts("Directory not found: ");
        puts(newpath);
        putchar('\n');
        return;
    }

    env_set("PWD", newpath);
}
