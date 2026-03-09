/*
 * kernel.c - Kernel for the JZ-HDL SOC
 *
 * Loaded by BIOS from 0:/KERNEL.BIN into SDRAM at 0x40000000.
 * Uses BIOS ECALLs for hardware I/O and raw sector access.
 * Provides filesystem services to user programs via registered ECALL handler.
 *
 * SDRAM program space: 0x40000000 - 0x402FFFFF (3 MB)
 *
 *   0x40000000  +---------------------------+
 *               | .text  — kernel code      |  ~6 KB
 *               | .rodata — string literals |
 *   0x40001670  +---------------------------+
 *               | .bss   — kernel globals   |  ~6 KB
 *               |   env, partition cache,   |
 *               |   sd_buf, allocator state |
 *   __bss_end   +---------------------------+
 *               | Heap (mem_get/mem_return) |  ~3 MB - kernel
 *               |   grows upward            |
 *               |   dir_entry buffers,      |
 *               |   loaded programs, etc.   |
 *   0x402FFFFF  +---------------------------+
 *
 * The kernel stack lives in BIOS RAM (0x10000000 region), not here.
 * All heap memory is managed by mem_get/mem_return (first-fit, coalescing).
 *
 * BIOS ECALLs used:
 *   0x00: register_handler — register kernel ECALL dispatch
 *   0x01: tty_read         — non-blocking UART read
 *   0x03: tty_write        — write to terminal/UART
 *   0x04: tty_clear        — clear terminal
 *   0x06: tty_fg           — set foreground color
 *   0x07: tty_bg           — set background color
 *   0x08: vid_mode         — set video mode (0=720p, 1=1080p)
 *   0x10: sd_status        — SD card status register
 *   0x11: sd_read_sector   — read one 512-byte sector (raw)
 *   0x12: sd_write_sector  — write one 512-byte sector (raw)
 *
 * Kernel ECALLs provided:
 *   0x20: sd_list_count(path_ptr)               — count directory entries
 *   0x21: sd_list(path_ptr, buf, max_entries)    — list directory into buffer
 *   0x22: sd_read(cluster, addr_start, max_len)  — load file by cluster
 *   0x23: fs_write(path_ptr, data_ptr, length)    — write file (create/overwrite)
 *   0x24: fs_remove(path_ptr)                     — delete file
 *   0x25: fs_dir_exists(path_ptr)                 — check if directory exists
 *   0x30: env_set(key_ptr, value_ptr)             — set environment variable
 *   0x31: env_get(key_ptr)                        — get value (caller must mem_return)
 *   0x32: mem_return(ptr)                          — free memory from mem_get
 */

/* ----------------------------------------------------------------
 * ECALL wrappers (to BIOS)
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

/* Device bits: 1=terminal, 2=UART, 3=both */
#define DEV_ALL 3

static void tty_write(const char *s, unsigned int len)
{
    ecall3(0x03, DEV_ALL, (unsigned int)s, len);
}

static void tty_clear(void)
{
    ecall2(0x04, DEV_ALL, 0);
}

static void tty_fg(unsigned int color)
{
    ecall2(0x06, DEV_ALL, color);
}

static void tty_bg(unsigned int color)
{
    ecall2(0x07, DEV_ALL, color);
}

static unsigned int tty_read(char *buf, unsigned int len)
{
    struct ecall_ret r = ecall2(0x01, (unsigned int)buf, len);
    return r.a0;
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

/* ----------------------------------------------------------------
 * Non-blocking getchar using ECALL tty_read
 * ---------------------------------------------------------------- */

static int getchar_nb(void)
{
    char ch;
    if (tty_read(&ch, 1) > 0)
        return (int)(unsigned char)ch;
    return -1;
}

/* Blocking getchar — brief spin with a NOP barrier gives the UART RX
 * IRQ handler a chance to fill the ring buffer between ECALLs. */
static int getchar(void)
{
    int ch;
    for (;;) {
        ch = getchar_nb();
        if (ch >= 0)
            return ch;
        /* Small delay so we're not constantly in ECALL (interrupts disabled).
         * Each NOP is one cycle; this keeps the CPU in user mode long enough
         * for pending IRQs to fire. */
        __asm__ volatile("nop; nop; nop; nop; nop; nop; nop; nop");
    }
}

/* ----------------------------------------------------------------
 * Shell line editor
 * ---------------------------------------------------------------- */

static int readline(char *buf, int maxlen)
{
    int len = 0;
    for (;;) {
        int ch = getchar();
        if (ch == '\r' || ch == '\n') {
            buf[len] = '\0';
            putchar('\n');
            return len;
        }
        if (ch == 0x08 || ch == 0x7F) {
            if (len > 0) {
                len--;
                puts("\b \b");
            }
            continue;
        }
        if (len < maxlen - 1 && ch >= 0x20) {
            buf[len++] = (char)ch;
            putchar(ch);
        }
    }
}

/* ----------------------------------------------------------------
 * Environment (key/value store)
 * ---------------------------------------------------------------- */

#define ENV_MAX_VARS  32
#define ENV_KEY_LEN   32
#define ENV_VAL_LEN   128

static char env_keys[ENV_MAX_VARS][ENV_KEY_LEN];
static char env_vals[ENV_MAX_VARS][ENV_VAL_LEN];
static int  env_count;

static int streq(const char *a, const char *b)
{
    while (*a && *b) {
        if (*a != *b)
            return 0;
        a++;
        b++;
    }
    return *a == *b;
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

/* Find env var by key, returns index or -1 */
static int env_find(const char *key)
{
    int i;
    for (i = 0; i < env_count; i++) {
        if (streq(env_keys[i], key))
            return i;
    }
    return -1;
}

/* Get env var value, returns NULL if not found */
static const char *env_get(const char *key)
{
    int i = env_find(key);
    if (i < 0)
        return (const char *)0;
    return env_vals[i];
}

/* Set env var */
static void env_set(const char *key, const char *val)
{
    int i = env_find(key);
    if (i >= 0) {
        strcpy(env_vals[i], val, ENV_VAL_LEN);
        return;
    }
    if (env_count >= ENV_MAX_VARS) {
        puts("Environment full\n");
        return;
    }
    strcpy(env_keys[env_count], key, ENV_KEY_LEN);
    strcpy(env_vals[env_count], val, ENV_VAL_LEN);
    env_count++;
}

/* ----------------------------------------------------------------
 * Command: SET key = value
 * ---------------------------------------------------------------- */

static const char *skip_spaces(const char *s)
{
    while (*s == ' ') s++;
    return s;
}

static void cmd_set(const char *arg)
{
    char key[ENV_KEY_LEN];
    char val[ENV_VAL_LEN];
    const char *p;
    int i;

    p = skip_spaces(arg);
    if (!*p) {
        /* No args: list all variables */
        for (i = 0; i < env_count; i++) {
            puts(env_keys[i]);
            puts(" = ");
            puts(env_vals[i]);
            putchar('\n');
        }
        return;
    }

    /* Parse key */
    i = 0;
    while (*p && *p != ' ' && *p != '=' && i < ENV_KEY_LEN - 1)
        key[i++] = *p++;
    key[i] = '\0';

    /* Skip to '=' */
    p = skip_spaces(p);
    if (*p != '=') {
        puts("Usage: SET <key> = <value>\n");
        return;
    }
    p++;
    p = skip_spaces(p);

    /* Parse value — quoted or unquoted */
    if (*p == '"') {
        p++;
        i = 0;
        while (*p && *p != '"' && i < ENV_VAL_LEN - 1)
            val[i++] = *p++;
        val[i] = '\0';
    } else {
        i = 0;
        while (*p && i < ENV_VAL_LEN - 1)
            val[i++] = *p++;
        val[i] = '\0';
        /* Trim trailing spaces */
        while (i > 0 && val[i - 1] == ' ')
            val[--i] = '\0';
    }

    env_set(key, val);
}

/* ----------------------------------------------------------------
 * Command: ECHO "string with {$VAR} expansion"
 * ---------------------------------------------------------------- */

static void cmd_echo(const char *arg)
{
    const char *p;

    p = skip_spaces(arg);

    /* Must be a quoted string */
    if (*p != '"') {
        puts("Usage: ECHO \"<string>\"\n");
        return;
    }
    p++;

    while (*p && *p != '"') {
        if (p[0] == '{' && p[1] == '$') {
            /* Variable expansion: {$KEY} */
            char key[ENV_KEY_LEN];
            const char *val;
            int i;

            p += 2;  /* skip {$ */
            i = 0;
            while (*p && *p != '}' && i < ENV_KEY_LEN - 1)
                key[i++] = *p++;
            key[i] = '\0';
            if (*p == '}')
                p++;

            val = env_get(key);
            if (val)
                puts(val);
        } else {
            putchar(*p);
            p++;
        }
    }
    putchar('\n');
}

/* ----------------------------------------------------------------
 * Memory allocator
 *
 * Heap runs from end of kernel BSS to 0x402FFFFF.
 * First-fit free list with coalescing. mem_get zeroes memory.
 * ---------------------------------------------------------------- */

extern char __bss_end[];

struct mem_block {
    unsigned int     size;  /* data area size (excludes header) */
    struct mem_block *next; /* next free block (valid when free) */
};

#define MEM_BLOCK_HDR  sizeof(struct mem_block)
#define MEM_HEAP_END   0x40300000u

static struct mem_block *free_list;
static int heap_ready;

static void mem_init(void)
{
    free_list = (struct mem_block *)(void *)__bss_end;
    free_list->size = MEM_HEAP_END - (unsigned int)(void *)__bss_end
                      - MEM_BLOCK_HDR;
    free_list->next = (struct mem_block *)0;
    heap_ready = 1;
}

static void *mem_get(unsigned int size)
{
    struct mem_block *prev, *cur;
    unsigned int aligned;

    if (!heap_ready)
        mem_init();

    /* Align to 4 bytes */
    aligned = (size + 3u) & ~3u;

    prev = (struct mem_block *)0;
    cur = free_list;
    while (cur) {
        if (cur->size >= aligned) {
            if (cur->size >= aligned + MEM_BLOCK_HDR + 4) {
                /* Split — create new free block after this allocation */
                struct mem_block *nb = (struct mem_block *)
                    ((unsigned char *)cur + MEM_BLOCK_HDR + aligned);
                nb->size = cur->size - aligned - MEM_BLOCK_HDR;
                nb->next = cur->next;
                cur->size = aligned;
                if (prev)
                    prev->next = nb;
                else
                    free_list = nb;
            } else {
                /* Use whole block */
                if (prev)
                    prev->next = cur->next;
                else
                    free_list = cur->next;
            }
            /* Zero the memory */
            {
                unsigned int *p = (unsigned int *)
                    ((unsigned char *)cur + MEM_BLOCK_HDR);
                unsigned int words = cur->size >> 2;
                unsigned int i;
                for (i = 0; i < words; i++)
                    p[i] = 0;
            }
            return (void *)((unsigned char *)cur + MEM_BLOCK_HDR);
        }
        prev = cur;
        cur = cur->next;
    }
    return (void *)0;  /* out of memory */
}

static void mem_return(void *ptr)
{
    struct mem_block *block, *prev, *cur;

    if (!ptr)
        return;

    block = (struct mem_block *)((unsigned char *)ptr - MEM_BLOCK_HDR);

    /* Insert into free list in address order */
    prev = (struct mem_block *)0;
    cur = free_list;
    while (cur && cur < block) {
        prev = cur;
        cur = cur->next;
    }

    block->next = cur;
    if (prev)
        prev->next = block;
    else
        free_list = block;

    /* Coalesce with next */
    if (cur && (unsigned char *)block + MEM_BLOCK_HDR + block->size
            == (unsigned char *)cur) {
        block->size += MEM_BLOCK_HDR + cur->size;
        block->next = cur->next;
    }

    /* Coalesce with prev */
    if (prev && (unsigned char *)prev + MEM_BLOCK_HDR + prev->size
             == (unsigned char *)block) {
        prev->size += MEM_BLOCK_HDR + block->size;
        prev->next = block->next;
    }
}

/* ----------------------------------------------------------------
 * FAT Filesystem (sectors via BIOS ECALLs 0x11/0x12)
 * ---------------------------------------------------------------- */

/* Sector buffer — 256 x 16-bit words = 512 bytes */
static unsigned int sd_buf[256];

/* Read one sector via BIOS ECALL 0x11 */
static int sd_read_sector(unsigned int lba)
{
    struct ecall_ret r = ecall2(0x11, lba, (unsigned int)sd_buf);
    return (int)r.a0;
}

/* Byte accessors into sd_buf */
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

/* Write one sector via BIOS ECALL 0x12 */
static int sd_write_sector(unsigned int lba)
{
    struct ecall_ret r = ecall2(0x12, lba, (unsigned int)sd_buf);
    return (int)r.a0;
}

/* Byte-write helpers for sd_buf */
static void sb_set(unsigned int offset, unsigned int val)
{
    unsigned int idx = offset >> 1;
    if (offset & 1)
        sd_buf[idx] = (sd_buf[idx] & 0x00FF) | ((val & 0xFF) << 8);
    else
        sd_buf[idx] = (sd_buf[idx] & 0xFF00) | (val & 0xFF);
}

static void sb_set16(unsigned int offset, unsigned int val)
{
    sd_buf[offset >> 1] = val & 0xFFFF;
}

static void sb_set32(unsigned int offset, unsigned int val)
{
    sd_buf[offset >> 1]       = val & 0xFFFF;
    sd_buf[(offset >> 1) + 1] = (val >> 16) & 0xFFFF;
}

/* Partition table cache */
struct part_info {
    unsigned int type;
    unsigned int lba;
    unsigned int size;
    unsigned int fat_start;
    unsigned int data_start;
    unsigned int root_cluster;
    unsigned int spc_shift;
    int          is_fat32;
    int          fat_valid;
};

static struct part_info parts[4];
static int mbr_valid;

/* Working filesystem globals */
static unsigned int fs_fat_start;
static unsigned int fs_data_start;
static unsigned int fs_spc_shift;
static unsigned int fs_root_cluster;
static unsigned int fs_is_fat32;

static int is_fat_type(unsigned int t)
{
    return t == 0x01 || t == 0x04 || t == 0x06 || t == 0x0B || t == 0x0C || t == 0x0E;
}

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

static int fs_use_partition(unsigned int idx)
{
    if (idx > 3 || !mbr_valid)
        return -1;
    if (!is_fat_type(parts[idx].type))
        return -1;

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

        tmp = spc;
        parts[idx].spc_shift = 0;
        while (tmp > 1) {
            tmp >>= 1;
            parts[idx].spc_shift++;
        }

        parts[idx].fat_valid = 1;
    }

    fs_fat_start    = parts[idx].fat_start;
    fs_data_start   = parts[idx].data_start;
    fs_spc_shift    = parts[idx].spc_shift;
    fs_root_cluster = parts[idx].root_cluster;
    fs_is_fat32     = (unsigned int)parts[idx].is_fat32;

    return 0;
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
            return 0x0FFFFFF8u;
        val = sb32(ent_offset) & 0x0FFFFFFFu;
    } else {
        /* FAT16 */
        fat_offset = cluster << 1;
        fat_sector = fs_fat_start + (fat_offset >> 9);
        ent_offset = fat_offset & 0x1FF;
        if (sd_read_sector(fat_sector))
            return 0xFFF8u;
        val = sb16(ent_offset);
        if (val >= 0xFFF8u)
            val = 0x0FFFFFF8u;
    }
    return val;
}

/* Match 8.3 directory entry name against search name */
static int match_dir_entry(unsigned int off, const char *name, int namelen)
{
    char fat_name[12];
    int i, ei;

    /* Read 8.3 name from entry */
    for (i = 0; i < 11; i++)
        fat_name[i] = (char)sb(off + (unsigned int)i);
    fat_name[11] = '\0';

    /* Build 8.3 from input: name part */
    ei = -1;
    for (i = 0; i < namelen; i++) {
        if (name[i] == '.') {
            ei = i;
            break;
        }
    }

    /* Compare name portion (up to 8 chars, space-padded) */
    {
        int name_part_len = (ei >= 0) ? ei : namelen;
        for (i = 0; i < 8; i++) {
            char c;
            if (i < name_part_len) {
                c = name[i];
                if (c >= 'a' && c <= 'z') c -= 32;
            } else {
                c = ' ';
            }
            if (fat_name[i] != c)
                return 0;
        }
    }

    /* Compare extension portion (up to 3 chars, space-padded) */
    {
        const char *ext = (ei >= 0) ? name + ei + 1 : "";
        int ext_len = (ei >= 0) ? namelen - ei - 1 : 0;
        for (i = 0; i < 3; i++) {
            char c;
            if (i < ext_len) {
                c = ext[i];
                if (c >= 'a' && c <= 'z') c -= 32;
            } else {
                c = ' ';
            }
            if (fat_name[8 + i] != c)
                return 0;
        }
    }

    return 1;
}

/* Find entry in directory, return cluster (0 = not found) */
static unsigned int find_in_dir(unsigned int cluster, const char *name,
                                int namelen, int *is_dir)
{
    while (cluster < 0x0FFFFFF8u) {
        unsigned int lba = cluster_to_lba(cluster);
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
                    unsigned int cl_hi = sb16(eoff + 20);
                    unsigned int cl_lo = sb16(eoff + 26);
                    *is_dir = (attr & 0x10) ? 1 : 0;
                    return (cl_hi << 16) | cl_lo;
                }
            }
        }
        cluster = fat_next_cluster(cluster);
    }
    return 0;
}

/* ----------------------------------------------------------------
 * FAT write helpers
 * ---------------------------------------------------------------- */

/* Set a FAT entry value and write the sector back */
static int fat_set_entry(unsigned int cluster, unsigned int value)
{
    unsigned int fat_offset, fat_sector, ent_offset;
    if (fs_is_fat32) {
        fat_offset = cluster << 2;
        fat_sector = fs_fat_start + (fat_offset >> 9);
        ent_offset = fat_offset & 0x1FF;
        if (sd_read_sector(fat_sector))
            return -1;
        /* Preserve top 4 bits */
        {
            unsigned int old = sb32(ent_offset);
            value = (old & 0xF0000000u) | (value & 0x0FFFFFFFu);
        }
        sb_set32(ent_offset, value);
    } else {
        fat_offset = cluster << 1;
        fat_sector = fs_fat_start + (fat_offset >> 9);
        ent_offset = fat_offset & 0x1FF;
        if (sd_read_sector(fat_sector))
            return -1;
        sb_set16(ent_offset, value & 0xFFFF);
    }
    return sd_write_sector(fat_sector);
}

/* Find first free FAT entry, mark as EOC, return cluster number (0 on failure) */
static unsigned int fat_alloc_cluster(void)
{
    unsigned int cluster;
    unsigned int max_cluster = 0xFFFF;  /* FAT16 default */
    unsigned int eoc = 0xFFF8u;

    if (fs_is_fat32) {
        max_cluster = 0x0FFFFFF0u;
        eoc = 0x0FFFFFF8u;
    }

    for (cluster = 2; cluster < max_cluster; cluster++) {
        unsigned int fat_offset, fat_sector, ent_offset, val;
        if (fs_is_fat32) {
            fat_offset = cluster << 2;
            fat_sector = fs_fat_start + (fat_offset >> 9);
            ent_offset = fat_offset & 0x1FF;
            if (sd_read_sector(fat_sector))
                return 0;
            val = sb32(ent_offset) & 0x0FFFFFFFu;
        } else {
            fat_offset = cluster << 1;
            fat_sector = fs_fat_start + (fat_offset >> 9);
            ent_offset = fat_offset & 0x1FF;
            if (sd_read_sector(fat_sector))
                return 0;
            val = sb16(ent_offset);
        }
        if (val == 0) {
            if (fat_set_entry(cluster, eoc))
                return 0;
            return cluster;
        }
    }
    return 0;
}

/* Free a cluster chain starting at cluster */
static void fat_free_chain(unsigned int cluster)
{
    while (cluster >= 2 && cluster < 0x0FFFFFF8u) {
        unsigned int next = fat_next_cluster(cluster);
        fat_set_entry(cluster, 0);
        cluster = next;
    }
}

/* Parse path into directory cluster + filename component.
 * Returns 1 on success, 0 on failure. */
static int parse_path_components(const char *path,
                                  unsigned int *dir_cluster,
                                  char *filename, int *filename_len)
{
    unsigned int idx;
    const char *p;
    unsigned int clust;
    const char *last_component;
    int last_len;

    if (path[0] < '0' || path[0] > '3' || path[1] != ':')
        return 0;
    idx = (unsigned int)(path[0] - '0');
    if (fs_use_partition(idx))
        return 0;

    p = path + 2;
    if (*p == '/')
        p++;

    clust = fs_root_cluster;
    last_component = p;
    last_len = 0;

    /* Find the last path component (the filename) */
    {
        const char *scan = p;

        while (*scan) {
            const char *start = scan;
            int len = 0;
            while (*scan && *scan != '/') {
                scan++;
                len++;
            }
            if (len == 0) {
                if (*scan == '/') scan++;
                continue;
            }
            if (*scan == '/' && scan[1]) {
                /* Not the last component — navigate into directory */
                int is_dir = 0;
                unsigned int found = find_in_dir(clust, start, len, &is_dir);
                if (found == 0 || !is_dir)
                    return 0;
                clust = found;
                scan++;
            } else {
                /* Last component = filename */
                last_component = start;
                last_len = len;
                if (*scan == '/')
                    scan++;
            }
        }
    }

    if (last_len == 0)
        return 0;

    *dir_cluster = clust;
    {
        int i;
        for (i = 0; i < last_len && i < 12; i++)
            filename[i] = last_component[i];
        filename[i] = '\0';
    }
    *filename_len = last_len;
    return 1;
}

/* Build an 8.3 FAT directory entry name (11 bytes, space-padded) from filename */
static void build_83_name(const char *name, int namelen, char *fat_name)
{
    int i, ei;

    /* Find dot */
    ei = -1;
    for (i = 0; i < namelen; i++) {
        if (name[i] == '.') {
            ei = i;
            break;
        }
    }

    /* Name part (8 chars, space-padded) */
    {
        int name_part_len = (ei >= 0) ? ei : namelen;
        for (i = 0; i < 8; i++) {
            if (i < name_part_len) {
                char c = name[i];
                if (c >= 'a' && c <= 'z') c -= 32;
                fat_name[i] = c;
            } else {
                fat_name[i] = ' ';
            }
        }
    }

    /* Extension part (3 chars, space-padded) */
    {
        const char *ext = (ei >= 0) ? name + ei + 1 : "";
        int ext_len = (ei >= 0) ? namelen - ei - 1 : 0;
        for (i = 0; i < 3; i++) {
            if (i < ext_len) {
                char c = ext[i];
                if (c >= 'a' && c <= 'z') c -= 32;
                fat_name[8 + i] = c;
            } else {
                fat_name[8 + i] = ' ';
            }
        }
    }
}

/* Find a directory entry by name. Returns entry offset within sector,
 * sector LBA in *out_lba, and entry details. Returns -1 if not found.
 * sd_buf contains the sector with the entry on success. */
static int find_dir_entry(unsigned int dir_cluster, const char *name,
                           int namelen, unsigned int *out_lba,
                           unsigned int *out_start_cluster,
                           unsigned int *out_size)
{
    unsigned int clust = dir_cluster;
    while (clust < 0x0FFFFFF8u) {
        unsigned int lba = cluster_to_lba(clust);
        unsigned int spc = 1u << fs_spc_shift;
        unsigned int s;
        for (s = 0; s < spc; s++) {
            int j;
            if (sd_read_sector(lba + s))
                return -1;
            for (j = 0; j < 16; j++) {
                unsigned int eoff = (unsigned int)(j << 5);
                unsigned int first = sb(eoff);
                unsigned int attr = sb(eoff + 11);
                if (first == 0x00)
                    return -1;
                if (first == 0xE5 || attr == 0x0F || (attr & 0x08))
                    continue;
                if (match_dir_entry(eoff, name, namelen)) {
                    *out_lba = lba + s;
                    *out_start_cluster = (sb16(eoff + 20) << 16) | sb16(eoff + 26);
                    *out_size = sb32(eoff + 28);
                    return (int)eoff;
                }
            }
        }
        clust = fat_next_cluster(clust);
    }
    return -1;
}

/* Find an empty or deleted directory entry slot. Returns offset within sector,
 * sector LBA in *out_lba. Returns -1 if directory is full. */
static int find_free_dir_slot(unsigned int dir_cluster, unsigned int *out_lba)
{
    unsigned int clust = dir_cluster;
    while (clust < 0x0FFFFFF8u) {
        unsigned int lba = cluster_to_lba(clust);
        unsigned int spc = 1u << fs_spc_shift;
        unsigned int s;
        for (s = 0; s < spc; s++) {
            int j;
            if (sd_read_sector(lba + s))
                return -1;
            for (j = 0; j < 16; j++) {
                unsigned int eoff = (unsigned int)(j << 5);
                unsigned int first = sb(eoff);
                if (first == 0x00 || first == 0xE5) {
                    *out_lba = lba + s;
                    return (int)eoff;
                }
            }
        }
        clust = fat_next_cluster(clust);
    }
    return -1;
}

/* 0x23: fs_write — write data to file at path.
 * Creates file if it doesn't exist, overwrites if it does.
 * Returns bytes written, or -1 on error. */
static int fs_write(const char *path, const unsigned char *data,
                     unsigned int length)
{
    unsigned int dir_cluster;
    char filename[13];
    int filename_len;
    char fat_name[11];
    unsigned int old_cluster, old_size;
    unsigned int entry_lba;
    int entry_off;
    unsigned int first_cluster = 0;
    unsigned int prev_cluster = 0;
    unsigned int bytes_written = 0;
    unsigned int i;

    if (!parse_path_components(path, &dir_cluster, filename, &filename_len))
        return -1;

    build_83_name(filename, filename_len, fat_name);

    /* Check if file already exists */
    entry_off = find_dir_entry(dir_cluster, filename, filename_len,
                                &entry_lba, &old_cluster, &old_size);
    if (entry_off >= 0) {
        /* File exists — free old cluster chain */
        if (old_cluster >= 2)
            fat_free_chain(old_cluster);
    }

    /* Allocate clusters and write data */
    if (length > 0) {
        unsigned int remaining = length;
        const unsigned char *src = data;

        while (remaining > 0) {
            unsigned int new_cluster = fat_alloc_cluster();
            unsigned int spc, s;
            if (new_cluster == 0)
                return -1;

            if (first_cluster == 0)
                first_cluster = new_cluster;

            /* Link to previous cluster */
            if (prev_cluster != 0)
                fat_set_entry(prev_cluster, new_cluster);

            /* Write data sectors for this cluster */
            spc = 1u << fs_spc_shift;
            for (s = 0; s < spc && remaining > 0; s++) {
                unsigned int lba = cluster_to_lba(new_cluster) + s;
                unsigned int chunk = 512;
                if (chunk > remaining)
                    chunk = remaining;

                /* Fill sd_buf with data */
                for (i = 0; i < 256; i++)
                    sd_buf[i] = 0;
                for (i = 0; i < chunk; i++) {
                    unsigned int idx = i >> 1;
                    if (i & 1)
                        sd_buf[idx] = (sd_buf[idx] & 0x00FF) | ((unsigned int)src[i] << 8);
                    else
                        sd_buf[idx] = (sd_buf[idx] & 0xFF00) | (unsigned int)src[i];
                }

                if (sd_write_sector(lba))
                    return -1;

                src += chunk;
                remaining -= chunk;
                bytes_written += chunk;
            }

            prev_cluster = new_cluster;
        }
    }

    /* Create/update directory entry */
    if (entry_off >= 0) {
        /* Re-read the directory sector */
        if (sd_read_sector(entry_lba))
            return -1;
        /* Update cluster and size */
        sb_set16((unsigned int)entry_off + 20, (first_cluster >> 16) & 0xFFFF);
        sb_set16((unsigned int)entry_off + 26, first_cluster & 0xFFFF);
        sb_set32((unsigned int)entry_off + 28, length);
    } else {
        /* New file — find empty slot */
        entry_off = find_free_dir_slot(dir_cluster, &entry_lba);
        if (entry_off < 0)
            return -1;
        /* sd_buf already contains the sector from find_free_dir_slot */

        /* Write 8.3 name */
        for (i = 0; i < 11; i++)
            sb_set((unsigned int)entry_off + i, (unsigned int)(unsigned char)fat_name[i]);
        /* Attributes: archive */
        sb_set((unsigned int)entry_off + 11, 0x20);
        /* Clear reserved/time/date fields */
        for (i = 12; i < 20; i++)
            sb_set((unsigned int)entry_off + i, 0);
        /* Cluster high */
        sb_set16((unsigned int)entry_off + 20, (first_cluster >> 16) & 0xFFFF);
        /* Clear time/date */
        sb_set16((unsigned int)entry_off + 22, 0);
        sb_set16((unsigned int)entry_off + 24, 0);
        /* Cluster low */
        sb_set16((unsigned int)entry_off + 26, first_cluster & 0xFFFF);
        /* Size */
        sb_set32((unsigned int)entry_off + 28, length);
    }

    if (sd_write_sector(entry_lba))
        return -1;

    return (int)bytes_written;
}

/* 0x24: fs_remove — delete file at path.
 * Returns 0 on success, -1 on error. */
static int fs_remove(const char *path)
{
    unsigned int dir_cluster;
    char filename[13];
    int filename_len;
    unsigned int entry_lba, file_cluster, file_size;
    int entry_off;

    if (!parse_path_components(path, &dir_cluster, filename, &filename_len))
        return -1;

    entry_off = find_dir_entry(dir_cluster, filename, filename_len,
                                &entry_lba, &file_cluster, &file_size);
    if (entry_off < 0)
        return -1;

    /* Check it's not a directory */
    {
        unsigned int attr = sb(entry_off + 11);
        if (attr & 0x10)
            return -1;
    }

    /* Mark entry as deleted */
    if (sd_read_sector(entry_lba))
        return -1;
    sb_set((unsigned int)entry_off, 0xE5);
    if (sd_write_sector(entry_lba))
        return -1;

    /* Free cluster chain */
    if (file_cluster >= 2)
        fat_free_chain(file_cluster);

    return 0;
}

/* ----------------------------------------------------------------
 * Stateless filesystem operations (kernel ECALLs)
 * ---------------------------------------------------------------- */

/* Directory entry struct — 76 bytes, includes cluster for sd_read */
struct dir_entry {
    char         name[64];   /* long filename or "FILENAME.EXT\0" */
    unsigned int size;       /* file size in bytes */
    unsigned int attr;       /* FAT attributes */
    unsigned int cluster;    /* starting cluster — pass to sd_read */
};

/* Navigate path to directory cluster. Returns 1 on success, 0 on failure. */
static int resolve_dir_path(const char *path, unsigned int *out_cluster)
{
    unsigned int idx;
    const char *p;
    unsigned int clust;

    if (path[0] < '0' || path[0] > '3' || path[1] != ':')
        return 0;
    idx = (unsigned int)(path[0] - '0');

    if (fs_use_partition(idx))
        return 0;

    p = path + 2;
    if (*p == '/')
        p++;

    clust = fs_root_cluster;
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
        if (found == 0 || !is_dir)
            return 0;

        clust = found;
        if (*p == '/')
            p++;
    }

    *out_cluster = clust;
    return 1;
}

/* Helper: fill one dir_entry from FAT entry at offset eoff in sd_buf */
static void fill_dir_entry(struct dir_entry *ent, unsigned int eoff)
{
    int i, pos;
    unsigned int attr = sb(eoff + 11);
    unsigned int cl_hi = sb16(eoff + 20);
    unsigned int cl_lo = sb16(eoff + 26);

    /* Build human-readable name from 8.3 */
    pos = 0;
    for (i = 0; i < 8; i++) {
        char c = (char)sb(eoff + (unsigned int)i);
        if (c != ' ')
            ent->name[pos++] = c;
    }
    {
        char ext[3];
        int has_ext = 0;
        for (i = 0; i < 3; i++) {
            ext[i] = (char)sb(eoff + 8 + (unsigned int)i);
            if (ext[i] != ' ')
                has_ext = 1;
        }
        if (has_ext) {
            ent->name[pos++] = '.';
            for (i = 0; i < 3; i++) {
                if (ext[i] != ' ')
                    ent->name[pos++] = ext[i];
            }
        }
    }
    ent->name[pos] = '\0';

    ent->size    = sb32(eoff + 28);
    ent->attr    = attr;
    ent->cluster = (cl_hi << 16) | cl_lo;
}

/* 0x20: sd_list_count — count entries in directory at path.
 * Returns entry count, or -1 on error. No internal state. */
static int fs_list_count(const char *path)
{
    unsigned int clust;
    int count = 0;

    if (!resolve_dir_path(path, &clust))
        return -1;

    while (clust < 0x0FFFFFF8u) {
        unsigned int lba = cluster_to_lba(clust);
        unsigned int spc = 1u << fs_spc_shift;
        unsigned int s;
        for (s = 0; s < spc; s++) {
            int j;
            if (sd_read_sector(lba + s))
                return count;
            for (j = 0; j < 16; j++) {
                unsigned int eoff = (unsigned int)(j << 5);
                unsigned int first = sb(eoff);
                unsigned int attr = sb(eoff + 11);
                if (first == 0x00)
                    return count;
                if (first == 0xE5 || attr == 0x0F || (attr & 0x08))
                    continue;
                count++;
            }
        }
        clust = fat_next_cluster(clust);
    }
    return count;
}

/* Extract up to 13 UCS-2 characters from an LFN directory entry into buf.
 * Characters are placed at offset (seq-1)*13. Returns number of chars placed.
 * UCS-2 high bytes are dropped (ASCII only). 0x0000 and 0xFFFF are terminators. */
static int lfn_extract(unsigned int eoff, int seq, char *buf, int bufsize)
{
    /* Character offsets within a 32-byte LFN entry (UCS-2 low byte positions) */
    static const unsigned int lfn_offsets[13] = {
        1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30
    };
    int base = (seq - 1) * 13;
    int i;

    for (i = 0; i < 13; i++) {
        int pos = base + i;
        unsigned int lo, hi;
        if (pos >= bufsize - 1)
            break;
        lo = sb(eoff + lfn_offsets[i]);
        hi = sb(eoff + lfn_offsets[i] + 1);
        if (lo == 0xFF && hi == 0xFF)
            break;
        if (lo == 0x00 && hi == 0x00) {
            buf[pos] = '\0';
            return pos;
        }
        buf[pos] = (hi == 0) ? (char)lo : '?';
    }
    return base + i;
}

/* 0x21: sd_list — list directory entries into caller buffer.
 * Collects LFN entries and uses them when available.
 * Returns entries filled, or -1 on error. No internal state. */
static int fs_list(const char *path, struct dir_entry *buf, int max_entries)
{
    unsigned int clust;
    int count = 0;
    char lfn_buf[64];
    int lfn_len = 0;

    if (!resolve_dir_path(path, &clust))
        return -1;

    lfn_buf[0] = '\0';

    while (clust < 0x0FFFFFF8u && count < max_entries) {
        unsigned int lba = cluster_to_lba(clust);
        unsigned int spc = 1u << fs_spc_shift;
        unsigned int s;
        for (s = 0; s < spc && count < max_entries; s++) {
            int j;
            if (sd_read_sector(lba + s))
                return count;
            for (j = 0; j < 16 && count < max_entries; j++) {
                unsigned int eoff = (unsigned int)(j << 5);
                unsigned int first = sb(eoff);
                unsigned int attr = sb(eoff + 11);

                if (first == 0x00)
                    return count;
                if (first == 0xE5) {
                    lfn_len = 0;
                    continue;
                }

                if (attr == 0x0F) {
                    /* LFN entry */
                    int seq = (int)(first & 0x3F);
                    if (first & 0x40) {
                        /* First LFN entry (highest seq) — clear buffer */
                        int k;
                        for (k = 0; k < 64; k++)
                            lfn_buf[k] = '\0';
                    }
                    if (seq >= 1 && seq <= 4) {
                        int end = lfn_extract(eoff, seq, lfn_buf, 64);
                        if (end > lfn_len)
                            lfn_len = end;
                    }
                    continue;
                }

                if (attr & 0x08) {
                    /* Volume label */
                    lfn_len = 0;
                    continue;
                }

                /* Regular entry — fill it */
                fill_dir_entry(&buf[count], eoff);

                /* Override name with LFN if available */
                if (lfn_len > 0) {
                    int k;
                    for (k = 0; k < lfn_len && k < 63; k++)
                        buf[count].name[k] = lfn_buf[k];
                    buf[count].name[k] = '\0';
                    lfn_len = 0;
                }

                count++;
            }
        }
        clust = fat_next_cluster(clust);
    }
    return count;
}

/* 0x22: sd_read — load file data by starting cluster.
 * Follows cluster chain, copies up to max_len bytes to dest.
 * Returns bytes read, or -1 on error. No internal state. */
static int fs_read(unsigned int file_cluster, unsigned int dest,
                   unsigned int max_len)
{
    unsigned int pos = 0;
    unsigned int cur_cl = file_cluster;

    while (pos < max_len && cur_cl < 0x0FFFFFF8u) {
        unsigned int spc = 1u << fs_spc_shift;
        unsigned int s;
        for (s = 0; s < spc && pos < max_len; s++) {
            unsigned int lba = cluster_to_lba(cur_cl) + s;
            unsigned int chunk, i;
            volatile unsigned char *d = (volatile unsigned char *)(dest + pos);

            if (sd_read_sector(lba))
                return -1;

            chunk = 512;
            if (chunk > max_len - pos)
                chunk = max_len - pos;

            for (i = 0; i < chunk; i++) {
                unsigned int word = sd_buf[i >> 1];
                d[i] = (i & 1) ? (unsigned char)(word >> 8)
                                : (unsigned char)(word & 0xFF);
            }
            pos += chunk;
        }
        cur_cl = fat_next_cluster(cur_cl);
    }
    return (int)pos;
}

/* ----------------------------------------------------------------
 * Kernel ECALL dispatch (called by BIOS for unhandled ECALLs)
 * ---------------------------------------------------------------- */

struct ecall_ret kernel_dispatch(unsigned int num, unsigned int a1,
                                 unsigned int a2, unsigned int a3)
{
    struct ecall_ret ret;
    ret.a0 = (unsigned int)-1;
    ret.a1 = 0;

    switch (num) {
    case 0x20:
        ret.a0 = (unsigned int)fs_list_count((const char *)a1);
        return ret;
    case 0x21:
        ret.a0 = (unsigned int)fs_list((const char *)a1,
                                        (struct dir_entry *)a2, (int)a3);
        return ret;
    case 0x22:
        ret.a0 = (unsigned int)fs_read(a1, a2, a3);
        return ret;
    case 0x23:
        ret.a0 = (unsigned int)fs_write((const char *)a1,
                                         (const unsigned char *)a2, a3);
        return ret;
    case 0x24:
        ret.a0 = (unsigned int)fs_remove((const char *)a1);
        return ret;
    case 0x25: {
        unsigned int clust;
        ret.a0 = (unsigned int)resolve_dir_path((const char *)a1, &clust);
        return ret;
    }
    case 0x30:
        env_set((const char *)a1, (const char *)a2);
        ret.a0 = 0;
        return ret;
    case 0x31: {
        const char *val = env_get((const char *)a1);
        if (val) {
            unsigned int len = strlen(val) + 1;
            char *buf = (char *)mem_get(len);
            if (buf) {
                unsigned int i;
                for (i = 0; i < len; i++)
                    buf[i] = val[i];
                ret.a0 = (unsigned int)buf;
            } else {
                ret.a0 = 0;
            }
        } else {
            ret.a0 = 0;
        }
        return ret;
    }
    case 0x32:
        mem_return((void *)a1);
        ret.a0 = 0;
        return ret;
    default:
        return ret;
    }
}

/* ----------------------------------------------------------------
 * Command dispatch
 * ---------------------------------------------------------------- */

static int cmd_match(const char *input, const char *cmd)
{
    while (*cmd) {
        char a = *input;
        char b = *cmd;
        /* Case-insensitive */
        if (a >= 'a' && a <= 'z') a -= 32;
        if (b >= 'a' && b <= 'z') b -= 32;
        if (a != b)
            return 0;
        input++;
        cmd++;
    }
    return (*input == '\0' || *input == ' ');
}

static const char *cmd_arg(const char *input)
{
    while (*input && *input != ' ')
        input++;
    while (*input == ' ')
        input++;
    return input;
}

/* ----------------------------------------------------------------
 * Command: ENV — print all environment variables
 * ---------------------------------------------------------------- */

static void cmd_env(void)
{
    int i;
    if (env_count == 0) {
        puts("(no variables set)\n");
        return;
    }
    for (i = 0; i < env_count; i++) {
        puts(env_keys[i]);
        puts(" = ");
        puts(env_vals[i]);
        putchar('\n');
    }
}

/* ----------------------------------------------------------------
 * Load and run program from filesystem (uses fs_list + fs_read)
 * ---------------------------------------------------------------- */

/* Try to find and run a program in a specific directory.
 * Returns 1 if found and run, 0 if not found. */
static int try_run_in_dir(const char *dir, const char *upper_name,
                           const char *args, unsigned int args_len)
{
    struct dir_entry entries[32];
    int count, i;
    unsigned int dest = 0x40100000u;  /* user program space (above kernel) */
    unsigned int entry;

    count = fs_list(dir, entries, 32);
    if (count <= 0)
        return 0;

    for (i = 0; i < count; i++) {
        if (entries[i].attr & 0x10)
            continue;  /* skip directories */
        if (!streq(entries[i].name, upper_name))
            continue;

        /* Found it — load into memory */
        if (fs_read(entries[i].cluster, dest, entries[i].size) < 0) {
            puts("Failed to load ");
            puts(upper_name);
            putchar('\n');
            return 1;
        }

        /* Check for JZEX header — read entry point offset from v2 header */
        entry = dest;
        {
            volatile unsigned int *p = (volatile unsigned int *)dest;
            if (*p == 0x58455A4Au) {
                unsigned char *h = (unsigned char *)dest;
                entry = dest + (h[4] | (h[5] << 8));
            }
        }

        /* Run it — pass args */
        ((void (*)(const char *, unsigned int))entry)(args, args_len);
        return 1;
    }
    return 0;
}

static int try_run_command(const char *cmd_name)
{
    const char *pwd;
    char upper_name[16];
    int i, ni;
    const char *args;
    unsigned int args_len;

    /* Build uppercase "COMMAND.JZE" to match against */
    ni = 0;
    for (i = 0; cmd_name[i] && cmd_name[i] != ' ' && ni < 11; i++) {
        char c = cmd_name[i];
        if (c >= 'a' && c <= 'z') c -= 32;
        upper_name[ni++] = c;
    }
    upper_name[ni++] = '.';
    upper_name[ni++] = 'J';
    upper_name[ni++] = 'Z';
    upper_name[ni++] = 'E';
    upper_name[ni] = '\0';

    /* Extract args (everything after command name, trimmed) */
    args = cmd_arg(cmd_name);
    args_len = strlen(args);

    /* 1. Try PWD first */
    pwd = env_get("PWD");
    if (!pwd)
        pwd = "0:/";

    if (try_run_in_dir(pwd, upper_name, args, args_len))
        return 1;

    /* 2. Try each directory in PATH (semicolon-separated) */
    {
        const char *path_env = env_get("PATH");
        if (path_env && *path_env) {
            const char *p = path_env;
            while (*p) {
                char dir[ENV_VAL_LEN];
                int di = 0;
                /* Extract next path component (delimited by ';') */
                while (*p && *p != ';' && di < ENV_VAL_LEN - 2) {
                    dir[di++] = *p++;
                }
                /* Ensure trailing slash */
                if (di > 0 && dir[di - 1] != '/')
                    dir[di++] = '/';
                dir[di] = '\0';
                if (*p == ';')
                    p++;
                /* Skip empty entries */
                if (di == 0)
                    continue;
                if (try_run_in_dir(dir, upper_name, args, args_len))
                    return 1;
            }
        }
    }

    return 0;
}

/* ----------------------------------------------------------------
 * Main
 * ---------------------------------------------------------------- */

void main(void)
{
    char line[128];

    tty_fg(0x00FFFFFF);  /* white */
    tty_bg(0x00000000);  /* black */
    tty_clear();

    puts("JZ-HDL Kernel v0.2\n");

    /* Initialize filesystem */
    fs_cache_mbr();
    if (mbr_valid)
        puts("SD card: MBR OK\n");
    else
        puts("SD card: no MBR found\n");

    /* Set default environment */
    env_set("PWD", "0:/");
    env_set("PATH", "");

    /* Register kernel ECALL handler with BIOS */
    ecall1(0x00, (unsigned int)kernel_dispatch);
    puts("ECALL handler registered\n");

    putchar('\n');

    for (;;) {
        puts("> ");
        readline(line, sizeof(line));

        if (!line[0])
            continue;

        if (cmd_match(line, "SET")) {
            cmd_set(cmd_arg(line));
        } else if (cmd_match(line, "ECHO")) {
            cmd_echo(cmd_arg(line));
        } else if (cmd_match(line, "ENV")) {
            cmd_env();
        } else if (!try_run_command(line)) {
            puts("Unknown command: ");
            puts(line);
            puts("\nCommands: SET, ECHO, ENV\n");
        }
    }
}
