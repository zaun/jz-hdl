/*
 * man.c - Display help text from JZEX programs
 *
 * Relocatable JZEX program. Loads a target JZEX binary and displays
 * its embedded help string from the JZEX v2 header.
 *
 * Kernel ECALLs used:
 *   0x21: sd_list(path_ptr, buf, max)  — list directory entries
 *   0x22: sd_read(cluster, dest, max)  — read file by cluster
 *   0x31: env_get(key_ptr)             — get environment variable
 *   0x32: mem_return(ptr)              — free memory
 *
 * BIOS ECALLs used:
 *   0x03: tty_write(dev, buf, len)     — write to terminal
 *
 * Usage: man <command>
 */

const char help_text[] __attribute__((section(".rodata.help"))) =
    "man <command> - display help for a command\n"
    "  Reads the JZEX header of the named program and\n"
    "  displays its embedded help text.\n";

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

/* ----------------------------------------------------------------
 * Directory entry struct (matches kernel's layout, 76 bytes)
 * ---------------------------------------------------------------- */

struct dir_entry {
    char         name[64];
    unsigned int size;
    unsigned int attr;
    unsigned int cluster;
};

/* File data buffer — use high SDRAM address, well above loaded program */
#define FILE_BUF_ADDR  0x40200000u
#define FILE_BUF_MAX   0x00100000u  /* 1MB max */

/* ----------------------------------------------------------------
 * Search for a file in a directory, load it, return 1 if found
 * ---------------------------------------------------------------- */

static int try_load_in_dir(const char *dir, const char *upper_name)
{
    struct dir_entry entries[32];
    int count, i;

    count = sd_list(dir, entries, 32);
    if (count <= 0)
        return 0;

    for (i = 0; i < count; i++) {
        if (entries[i].attr & 0x10)
            continue;  /* skip directories */
        if (!streqi(entries[i].name, (int)strlen(entries[i].name),
                    upper_name, (int)strlen(upper_name)))
            continue;

        /* Found it — load into buffer */
        if (sd_read(entries[i].cluster, FILE_BUF_ADDR,
                    entries[i].size < FILE_BUF_MAX ? entries[i].size : FILE_BUF_MAX) < 0) {
            puts("Failed to load ");
            puts(upper_name);
            putchar('\n');
            return 1;  /* found but failed */
        }
        return 1;
    }
    return 0;
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

void main(const char *args, unsigned int args_len)
{
    char upper_name[20];
    int i, ni, found;

    if (args_len == 0) {
        puts("Usage: man <command>\n");
        return;
    }

    /* Build uppercase "COMMAND.JZE" filename */
    ni = 0;
    for (i = 0; i < (int)args_len && args[i] != ' ' && ni < 11; i++) {
        char c = args[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        upper_name[ni++] = c;
    }
    /* Append .JZE if no extension */
    {
        int has_dot = 0;
        int j;
        for (j = 0; j < ni; j++) {
            if (upper_name[j] == '.') { has_dot = 1; break; }
        }
        if (!has_dot) {
            upper_name[ni++] = '.';
            upper_name[ni++] = 'J';
            upper_name[ni++] = 'Z';
            upper_name[ni++] = 'E';
        }
    }
    upper_name[ni] = '\0';

    /* Search PWD first, then PATH */
    found = 0;
    {
        char *pwd_buf = env_get("PWD");
        const char *pwd = pwd_buf ? pwd_buf : "0:/";
        found = try_load_in_dir(pwd, upper_name);
        if (pwd_buf)
            mem_return(pwd_buf);
    }

    if (!found) {
        /* Try PATH */
        char *path_env = env_get("PATH");
        if (path_env && *path_env) {
            const char *p = path_env;
            while (*p && !found) {
                char dir[128];
                int di = 0;
                while (*p && *p != ';' && di < 126) {
                    dir[di++] = *p++;
                }
                if (di > 0 && dir[di - 1] != '/')
                    dir[di++] = '/';
                dir[di] = '\0';
                if (*p == ';')
                    p++;
                if (di > 0)
                    found = try_load_in_dir(dir, upper_name);
            }
        }
        if (path_env)
            mem_return(path_env);
    }

    if (!found) {
        puts("Command not found: ");
        tty_write(args, args_len);
        putchar('\n');
        return;
    }

    /* Parse JZEX header from loaded file */
    {
        volatile unsigned char *h = (volatile unsigned char *)FILE_BUF_ADDR;
        unsigned int magic;
        unsigned int help_offset, help_size;

        /* Check JZEX magic */
        magic = h[0] | (h[1] << 8) | (h[2] << 16) | (h[3] << 24);
        if (magic != 0x58455A4Au) {
            puts("Not a JZEX program: ");
            tty_write(args, args_len);
            putchar('\n');
            return;
        }

        /* Read help offset and size from header */
        help_offset = h[6] | (h[7] << 8);
        help_size = h[8] | (h[9] << 8);

        if (help_size == 0 || help_offset == 0) {
            puts("No help available for ");
            tty_write(args, args_len);
            putchar('\n');
            return;
        }

        /* Print help text */
        tty_write((const char *)(FILE_BUF_ADDR + help_offset), help_size);
    }
}
