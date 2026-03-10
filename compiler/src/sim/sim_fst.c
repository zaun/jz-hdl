/**
 * @file sim_fst.c
 * @brief FST (Fast Signal Trace) waveform writer for simulation.
 *
 * Implements the GTKWave FST binary format. All data is buffered in memory
 * during simulation and written to disk at fst_close() time.
 *
 * Block ordering: HDR → VCDATA → GEOM → HIER
 *
 * The HIER block uses gzip compression; GEOM and VCDATA use zlib.
 * This implementation includes minimal gzip/zlib "stored" encoders
 * to avoid an external dependency.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "sim_fst.h"

#define FST_MAX_SIGNALS 4096

/* ---- FST block types ---- */
#define FST_BL_HDR               0
#define FST_BL_VCDATA            1
#define FST_BL_GEOM              3
#define FST_BL_HIER              4

/* ---- FST hierarchy entry tags ---- */
#define FST_HT_SCOPE            254
#define FST_HT_UPSCOPE          255
/* Variables use their VarType directly as the tag byte */

/* ---- FST scope/var types ---- */
#define FST_ST_VCD_MODULE         0
#define FST_VT_VCD_WIRE          28
#define FST_VD_IMPLICIT           0

/* ---- FST endianness test double ---- */
static const double FST_DOUBLE_ENDTEST = 2.7182818284590452354;

/* ---- Signal definition ---- */
typedef struct FSTSignal {
    char *scope;
    char *name;
    int   width;
} FSTSignal;

/* ---- Value change record ---- */
typedef struct FSTChange {
    uint64_t time;
    int      sig_id;
    uint64_t value;
} FSTChange;

/* ---- Dynamic byte buffer ---- */
typedef struct FSTBuffer {
    uint8_t *data;
    size_t   len;
    size_t   cap;
} FSTBuffer;

struct FSTWriter {
    FILE       *fp;
    uint64_t    timescale_ps;
    int8_t      timescale_exp;

    FSTSignal   signals[FST_MAX_SIGNALS];
    int         num_signals;
    int         defs_ended;

    /* Hierarchy data (built during add_signal, compressed at close) */
    FSTBuffer   hier;
    int         num_scopes;
    char       *current_scope;

    /* Value changes (buffered during simulation) */
    FSTChange  *changes;
    size_t      num_changes;
    size_t      changes_cap;

    uint64_t    current_time;
    uint64_t    start_time;
    uint64_t    end_time;
    int         has_time;
};

/* ---- Buffer helpers ---- */

static void buf_init(FSTBuffer *b)
{
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

static void buf_ensure(FSTBuffer *b, size_t extra)
{
    size_t needed = b->len + extra;
    if (needed <= b->cap) return;
    size_t newcap = b->cap ? b->cap * 2 : 4096;
    while (newcap < needed) newcap *= 2;
    b->data = (uint8_t *)realloc(b->data, newcap);
    b->cap = newcap;
}

static void buf_u8(FSTBuffer *b, uint8_t v)
{
    buf_ensure(b, 1);
    b->data[b->len++] = v;
}

static void buf_bytes(FSTBuffer *b, const void *data, size_t len)
{
    buf_ensure(b, len);
    memcpy(b->data + b->len, data, len);
    b->len += len;
}

static void buf_str(FSTBuffer *b, const char *s)
{
    size_t len = s ? strlen(s) : 0;
    buf_bytes(b, s ? s : "", len);
    buf_u8(b, 0); /* null terminator */
}

static void buf_free(FSTBuffer *b)
{
    free(b->data);
    b->data = NULL;
    b->len = 0;
    b->cap = 0;
}

/* ---- Varint encoding (unsigned LEB128) ---- */

static void buf_varint(FSTBuffer *b, uint64_t v)
{
    do {
        uint8_t byte = (uint8_t)(v & 0x7F);
        v >>= 7;
        if (v) byte |= 0x80;
        buf_u8(b, byte);
    } while (v);
}

/* ---- Big-endian encoding helpers ---- */

static void write_u8(FILE *fp, uint8_t v)
{
    fputc(v, fp);
}

static void write_u64(FILE *fp, uint64_t v)
{
    uint8_t buf[8];
    for (int i = 7; i >= 0; i--) {
        buf[i] = (uint8_t)(v & 0xFF);
        v >>= 8;
    }
    fwrite(buf, 1, 8, fp);
}

static void write_varint(FILE *fp, uint64_t v)
{
    do {
        uint8_t byte = (uint8_t)(v & 0x7F);
        v >>= 7;
        if (v) byte |= 0x80;
        fputc(byte, fp);
    } while (v);
}

/* ---- Adler-32 checksum (for zlib) ---- */

static uint32_t adler32(const uint8_t *data, size_t len)
{
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; i++) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

/* ---- CRC32 (for gzip) ---- */

static uint32_t crc32_table[256];
static int crc32_table_ready = 0;

static void crc32_init(void)
{
    if (crc32_table_ready) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            if (c & 1) c = 0xEDB88320u ^ (c >> 1);
            else c >>= 1;
        }
        crc32_table[i] = c;
    }
    crc32_table_ready = 1;
}

static uint32_t compute_crc32(const uint8_t *data, size_t len)
{
    crc32_init();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFu;
}

/* ---- Deflate "stored" blocks (shared by zlib and gzip wrappers) ---- */

static size_t deflate_stored_size(size_t len)
{
    size_t nblocks = (len + 65534) / 65535;
    if (nblocks == 0) nblocks = 1;
    return nblocks * 5 + len;
}

static size_t deflate_stored(uint8_t *out, const uint8_t *in, size_t len)
{
    size_t pos = 0;
    size_t remaining = len;
    const uint8_t *ptr = in;

    while (remaining > 0 || pos == 0) {
        size_t block_len = remaining > 65535 ? 65535 : remaining;
        int is_final = (remaining <= 65535) ? 1 : 0;
        out[pos++] = (uint8_t)is_final;
        out[pos++] = (uint8_t)(block_len & 0xFF);
        out[pos++] = (uint8_t)((block_len >> 8) & 0xFF);
        out[pos++] = (uint8_t)(~block_len & 0xFF);
        out[pos++] = (uint8_t)((~block_len >> 8) & 0xFF);
        if (block_len > 0) {
            memcpy(out + pos, ptr, block_len);
            pos += block_len;
            ptr += block_len;
        }
        remaining -= block_len;
        if (block_len == 0) break;
    }
    return pos;
}

/* ---- Zlib "stored" encoder ---- */

static size_t zlib_store_bound(size_t len)
{
    return 2 + deflate_stored_size(len) + 4;
}

static size_t zlib_store(uint8_t *out, const uint8_t *in, size_t len)
{
    size_t pos = 0;
    /* Zlib header: CM=8, CINFO=7, FCHECK so (0x78xx % 31)==0 */
    out[pos++] = 0x78;
    out[pos++] = 0x01;

    pos += deflate_stored(out + pos, in, len);

    /* Adler-32 (big-endian) */
    uint32_t cksum = adler32(in, len);
    out[pos++] = (uint8_t)((cksum >> 24) & 0xFF);
    out[pos++] = (uint8_t)((cksum >> 16) & 0xFF);
    out[pos++] = (uint8_t)((cksum >> 8) & 0xFF);
    out[pos++] = (uint8_t)(cksum & 0xFF);
    return pos;
}

/* ---- Gzip "stored" encoder (for HIER block) ---- */

static size_t gzip_store_bound(size_t len)
{
    return 10 + deflate_stored_size(len) + 8;
}

static size_t gzip_store(uint8_t *out, const uint8_t *in, size_t len)
{
    size_t pos = 0;
    /* Gzip header (10 bytes) */
    out[pos++] = 0x1f; /* ID1 */
    out[pos++] = 0x8b; /* ID2 */
    out[pos++] = 0x08; /* CM = deflate */
    out[pos++] = 0x00; /* FLG */
    out[pos++] = 0x00; out[pos++] = 0x00;
    out[pos++] = 0x00; out[pos++] = 0x00; /* MTIME */
    out[pos++] = 0x00; /* XFL */
    out[pos++] = 0xff; /* OS = unknown */

    pos += deflate_stored(out + pos, in, len);

    /* CRC32 (little-endian) */
    uint32_t crc = compute_crc32(in, len);
    out[pos++] = (uint8_t)(crc & 0xFF);
    out[pos++] = (uint8_t)((crc >> 8) & 0xFF);
    out[pos++] = (uint8_t)((crc >> 16) & 0xFF);
    out[pos++] = (uint8_t)((crc >> 24) & 0xFF);

    /* ISIZE (little-endian) */
    uint32_t isize = (uint32_t)(len & 0xFFFFFFFFu);
    out[pos++] = (uint8_t)(isize & 0xFF);
    out[pos++] = (uint8_t)((isize >> 8) & 0xFF);
    out[pos++] = (uint8_t)((isize >> 16) & 0xFF);
    out[pos++] = (uint8_t)((isize >> 24) & 0xFF);

    return pos;
}


/* ---- Timescale conversion ---- */

static int8_t timescale_to_exponent(uint64_t ps)
{
    if (ps >= 1000000000000ULL) return 0;
    if (ps >= 100000000000ULL)  return -1;
    if (ps >= 10000000000ULL)   return -2;
    if (ps >= 1000000000ULL)    return -3;
    if (ps >= 100000000ULL)     return -4;
    if (ps >= 10000000ULL)      return -5;
    if (ps >= 1000000ULL)       return -6;
    if (ps >= 100000ULL)        return -7;
    if (ps >= 10000ULL)         return -8;
    if (ps >= 1000ULL)          return -9;
    if (ps >= 100ULL)           return -10;
    if (ps >= 10ULL)            return -11;
    return -12;
}

/* ---- Hierarchy building (correct FST tags) ---- */

static void hier_open_scope(FSTWriter *w, const char *name)
{
    buf_u8(&w->hier, FST_HT_SCOPE);      /* tag 254 */
    buf_u8(&w->hier, FST_ST_VCD_MODULE);  /* scope type */
    buf_str(&w->hier, name);              /* name\0 */
    buf_str(&w->hier, "");                /* component\0 */
    w->num_scopes++;
}

static void hier_close_scope(FSTWriter *w)
{
    buf_u8(&w->hier, FST_HT_UPSCOPE);    /* tag 255 */
}

static void hier_add_var(FSTWriter *w, const char *name, int width)
{
    buf_u8(&w->hier, FST_VT_VCD_WIRE);   /* VarType IS the tag (28) */
    buf_u8(&w->hier, FST_VD_IMPLICIT);    /* direction */
    buf_str(&w->hier, name);              /* name\0 */
    buf_varint(&w->hier, (uint64_t)width); /* width (varint) */
    buf_varint(&w->hier, 0);              /* alias = 0 (new signal) */
}

/* ---- Public API ---- */

FSTWriter *fst_open(const char *filename, uint64_t timescale_ps)
{
    FILE *fp = fopen(filename, "wb");
    if (!fp) return NULL;

    FSTWriter *w = (FSTWriter *)calloc(1, sizeof(FSTWriter));
    if (!w) {
        fclose(fp);
        return NULL;
    }

    w->fp = fp;
    w->timescale_ps = timescale_ps;
    w->timescale_exp = timescale_to_exponent(timescale_ps);

    buf_init(&w->hier);

    return w;
}

int fst_add_signal(FSTWriter *w, const char *scope, const char *name, int width)
{
    if (!w || w->defs_ended || w->num_signals >= FST_MAX_SIGNALS) return -1;

    const char *s = scope ? scope : "top";

    /* Open/switch scope if needed */
    if (!w->current_scope || strcmp(w->current_scope, s) != 0) {
        if (w->current_scope) {
            hier_close_scope(w);
            free(w->current_scope);
        }
        w->current_scope = strdup(s);
        hier_open_scope(w, s);
    }

    int idx = w->num_signals;
    FSTSignal *sig = &w->signals[idx];
    sig->scope = strdup(s);
    sig->name = strdup(name);
    sig->width = width;

    hier_add_var(w, name, width);

    w->num_signals++;
    return idx;
}

void fst_end_definitions(FSTWriter *w)
{
    if (!w || w->defs_ended) return;

    if (w->current_scope) {
        hier_close_scope(w);
        free(w->current_scope);
        w->current_scope = NULL;
    }

    w->defs_ended = 1;
}

void fst_set_time(FSTWriter *w, uint64_t time_ps)
{
    if (!w || !w->defs_ended) return;

    uint64_t time_units = time_ps / w->timescale_ps;

    if (!w->has_time) {
        w->start_time = time_units;
        w->end_time = time_units;
        w->has_time = 1;
    }
    w->current_time = time_units;
    if (time_units > w->end_time) {
        w->end_time = time_units;
    }
}

void fst_dump_value(FSTWriter *w, int sig_id, uint64_t value, int width)
{
    if (!w || !w->defs_ended || sig_id < 0 || sig_id >= w->num_signals) return;
    (void)width;

    if (w->num_changes >= w->changes_cap) {
        size_t newcap = w->changes_cap ? w->changes_cap * 2 : 8192;
        w->changes = (FSTChange *)realloc(w->changes, newcap * sizeof(FSTChange));
        w->changes_cap = newcap;
    }

    FSTChange *vc = &w->changes[w->num_changes++];
    vc->time = w->current_time;
    vc->sig_id = sig_id;
    vc->value = value;
}

/* ---- FST file writing (called from fst_close) ---- */

/**
 * @brief Build the unique sorted time table from all value changes.
 */
static uint64_t *build_time_table(FSTWriter *w, size_t *out_count)
{
    if (w->num_changes == 0) {
        *out_count = 0;
        return NULL;
    }

    size_t cap = 1024;
    uint64_t *times = (uint64_t *)malloc(cap * sizeof(uint64_t));
    size_t count = 0;

    for (size_t i = 0; i < w->num_changes; i++) {
        uint64_t t = w->changes[i].time;
        int found = 0;
        for (size_t j = 0; j < count; j++) {
            if (times[j] == t) { found = 1; break; }
        }
        if (!found) {
            if (count >= cap) {
                cap *= 2;
                times = (uint64_t *)realloc(times, cap * sizeof(uint64_t));
            }
            times[count++] = t;
        }
    }

    /* Sort */
    for (size_t i = 0; i < count - 1; i++) {
        for (size_t j = i + 1; j < count; j++) {
            if (times[j] < times[i]) {
                uint64_t tmp = times[i];
                times[i] = times[j];
                times[j] = tmp;
            }
        }
    }

    *out_count = count;
    return times;
}

static size_t time_to_index(const uint64_t *times, size_t count, uint64_t t)
{
    for (size_t i = 0; i < count; i++) {
        if (times[i] == t) return i;
    }
    return 0;
}

/* ---- HDR block ----
 *
 * Fixed 330 bytes total. section_length = 329.
 *
 * Layout:
 *   type(1) + section_length(8) + start_time(8) + end_time(8) +
 *   endian_double(8) + memory_used(8) + num_scopes(8) +
 *   num_hier_vars(8) + num_vars(8) + num_vc_blocks(8) +
 *   timescale(1) + version(128) + date(119) + filetype(1) + timezero(8)
 */
#define FST_HDR_VERSION_SIZE  128
#define FST_HDR_DATE_SIZE     119  /* 26 date + 93 reserved */
#define FST_HDR_SECTION_LEN   329

static void write_hdr_block(FSTWriter *w)
{
    fseek(w->fp, 0, SEEK_SET);

    write_u8(w->fp, FST_BL_HDR);
    write_u64(w->fp, FST_HDR_SECTION_LEN);

    write_u64(w->fp, w->start_time);
    write_u64(w->fp, w->end_time);

    fwrite(&FST_DOUBLE_ENDTEST, sizeof(double), 1, w->fp);

    write_u64(w->fp, 0);                              /* memory_used */
    write_u64(w->fp, (uint64_t)w->num_scopes);        /* num_scopes */
    write_u64(w->fp, (uint64_t)w->num_signals);       /* num_hier_vars */
    write_u64(w->fp, (uint64_t)w->num_signals);       /* num_vars */
    write_u64(w->fp, 1);                               /* num_vc_blocks */

    write_u8(w->fp, (uint8_t)w->timescale_exp);

    /* Version (128 bytes, zero-padded) */
    {
        char buf[FST_HDR_VERSION_SIZE];
        memset(buf, 0, sizeof(buf));
        snprintf(buf, sizeof(buf), "JZ-HDL Simulator 1.0");
        fwrite(buf, 1, FST_HDR_VERSION_SIZE, w->fp);
    }

    /* Date (119 bytes: 26 date + 93 reserved, zero-padded) */
    {
        char buf[FST_HDR_DATE_SIZE];
        memset(buf, 0, sizeof(buf));
        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        strftime(buf, 26, "%a %b %d %H:%M:%S %Y", tm_info);
        fwrite(buf, 1, FST_HDR_DATE_SIZE, w->fp);
    }

    write_u8(w->fp, 0); /* filetype = Verilog */
    write_u64(w->fp, 0); /* timezero */
}

/* ---- VCDATA block (type 1) ----
 *
 * Layout (forward):
 *   type(1) + section_length(8) + start_time(8) + end_time(8) +
 *   memory_required(8) +
 *   frame_uclen(varint) + frame_clen(varint) + frame_maxhandle(varint) +
 *   frame_data(frame_clen bytes) +
 *   vc_maxhandle(varint) + packtype(1) +
 *   vc_data_stream
 *
 * Layout (from end of block, reading backwards):
 *   ... tsec_uclen(8) + tsec_clen(8) + tsec_nitems(8)  [last 24 bytes]
 *   ... time_data(tsec_clen bytes) before that
 *   ... chain_clen(8) before time_data
 *   ... chain_table(chain_clen bytes) before chain_clen
 *
 * VC data stream: per-signal value changes, concatenated.
 *   A padding byte (0x00) at offset 0 ensures all valid offsets > 0.
 *
 * 1-bit value change: varint = (time_index_delta << 2) | enc
 *   enc: 0=value0, 2=value1
 *
 * Multi-bit value change: varint = (time_index_delta << 1) | 1
 *   followed by ceil(width/8) bytes of packed bits (MSB first)
 *
 * Chain table: varint sequence mapping signal handles to vc_data offsets.
 *   val odd:  offset = prev + (val >> 1), signal has data
 *   val even nonzero: run of (val >> 1) signals with no data
 *   val == 0: alias (not used here)
 */
static void write_vcdata_block(FSTWriter *w)
{
    size_t time_count = 0;
    uint64_t *times = build_time_table(w, &time_count);
    int maxhandle = w->num_signals;

    long block_start = ftell(w->fp);
    write_u8(w->fp, FST_BL_VCDATA);
    long seclen_pos = ftell(w->fp);
    write_u64(w->fp, 0); /* section_length placeholder */

    write_u64(w->fp, w->start_time);
    write_u64(w->fp, w->end_time);
    long mem_req_pos = ftell(w->fp);
    write_u64(w->fp, 0); /* memory_required placeholder */

    /* ---- Frame data (initial values at start_time) ---- */
    {
        int total_bits = 0;
        for (int i = 0; i < maxhandle; i++)
            total_bits += w->signals[i].width;

        /* Uncompressed frame: all '0' initial values */
        write_varint(w->fp, (uint64_t)total_bits); /* frame_uclen */
        write_varint(w->fp, (uint64_t)total_bits); /* frame_clen (== uclen → raw) */
        write_varint(w->fp, (uint64_t)maxhandle);  /* frame_maxhandle */
        for (int i = 0; i < total_bits; i++)
            fputc('0', w->fp);
    }

    /* vc_maxhandle + packtype */
    write_varint(w->fp, (uint64_t)maxhandle);
    fputc('!', w->fp); /* uncompressed */

    /* ---- Build per-signal VC data ---- */
    FSTBuffer *sig_vc = (FSTBuffer *)calloc((size_t)maxhandle, sizeof(FSTBuffer));
    int *prev_tidx = (int *)calloc((size_t)maxhandle, sizeof(int));
    for (int s = 0; s < maxhandle; s++)
        buf_init(&sig_vc[s]);

    for (size_t i = 0; i < w->num_changes; i++) {
        FSTChange *vc = &w->changes[i];
        int s = vc->sig_id;
        if (s < 0 || s >= maxhandle) continue;

        int width = w->signals[s].width;
        size_t tidx = time_to_index(times, time_count, vc->time);
        int delta = (int)tidx - prev_tidx[s];
        prev_tidx[s] = (int)tidx;

        if (width == 1) {
            /* 1-bit 2-state: (delta << 2) | enc; enc: 0→'0', 2→'1' */
            uint64_t val = vc->value & 1;
            uint64_t enc = ((uint64_t)delta << 2) | (val ? 2 : 0);
            buf_varint(&sig_vc[s], enc);
        } else {
            /* Multi-bit 2-state: (delta << 1) | 0, then ceil(width/8)
             * bytes of MSB-aligned packed bits.
             *
             * The reader unpacks bit j (0..width-1) from:
             *   byte = j/8, bit = 7 - (j & 7)
             * So signal MSB goes into bit 7 of byte 0.
             * Effectively: value << (ceil(width/8)*8 - width).
             */
            buf_varint(&sig_vc[s], ((uint64_t)delta << 1) | 0);
            int nbytes = (width + 7) / 8;
            int shift = nbytes * 8 - width;
            uint64_t packed = vc->value << shift;
            for (int b = nbytes - 1; b >= 0; b--) {
                buf_u8(&sig_vc[s],
                       (uint8_t)((packed >> (b * 8)) & 0xFF));
            }
        }
    }

    /* ---- Write VC data stream ---- */
    /*
     * Each signal's entry starts with a varint header:
     *   varint > 0: compressed data follows (varint = uncompressed length)
     *   varint == 0: raw data follows (length from chain_table_lengths)
     *
     * chain_table offsets are from vc_start (the packtype byte position).
     * Offset 1 = first byte after packtype.
     */
    uint32_t *offsets = (uint32_t *)calloc((size_t)maxhandle, sizeof(uint32_t));
    uint32_t cumulative = 1; /* offset 1 = first byte after packtype */
    uint64_t total_vc_mem = 0;
    for (int s = 0; s < maxhandle; s++) {
        if (sig_vc[s].len > 0) {
            offsets[s] = cumulative;
            fputc(0, w->fp);  /* varint(0) = raw/uncompressed data follows */
            fwrite(sig_vc[s].data, 1, sig_vc[s].len, w->fp);
            cumulative += 1 + (uint32_t)sig_vc[s].len; /* 1 for varint(0) header */
            total_vc_mem += sig_vc[s].len;
        }
        /* offsets[s] stays 0 if no data */
    }

    /* ---- Chain table (raw varint sequence, NOT compressed) ---- */
    {
        FSTBuffer chain;
        buf_init(&chain);
        uint32_t pval = 0;
        int s = 0;
        while (s < maxhandle) {
            if (offsets[s] > 0) {
                /* Signal has data: encode delta */
                uint32_t delta = offsets[s] - pval;
                buf_varint(&chain, ((uint64_t)delta << 1) | 1);
                pval = offsets[s];
                s++;
            } else {
                /* Count consecutive signals with no data */
                int run = 0;
                while (s + run < maxhandle && offsets[s + run] == 0)
                    run++;
                buf_varint(&chain, (uint64_t)run << 1);
                s += run;
            }
        }

        /* Write chain table raw (not compressed) */
        fwrite(chain.data, 1, chain.len, w->fp);
        write_u64(w->fp, (uint64_t)chain.len);

        buf_free(&chain);
    }

    /* ---- Time table (compressed varint deltas, at end of block) ---- */
    {
        FSTBuffer tb;
        buf_init(&tb);
        uint64_t prev = 0;
        for (size_t i = 0; i < time_count; i++) {
            buf_varint(&tb, times[i] - prev);
            prev = times[i];
        }

        size_t bound = zlib_store_bound(tb.len);
        uint8_t *compressed = (uint8_t *)malloc(bound);
        size_t tsec_clen = zlib_store(compressed, tb.data, tb.len);

        fwrite(compressed, 1, tsec_clen, w->fp);
        write_u64(w->fp, (uint64_t)tb.len);        /* tsec_uclen */
        write_u64(w->fp, (uint64_t)tsec_clen);      /* tsec_clen */
        write_u64(w->fp, (uint64_t)time_count);      /* tsec_nitems */

        free(compressed);
        buf_free(&tb);
    }

    /* Patch section_length and mem_required */
    long block_end = ftell(w->fp);
    uint64_t section_len = (uint64_t)(block_end - block_start) - 1;
    fseek(w->fp, seclen_pos, SEEK_SET);
    write_u64(w->fp, section_len);
    fseek(w->fp, mem_req_pos, SEEK_SET);
    write_u64(w->fp, total_vc_mem);
    fseek(w->fp, block_end, SEEK_SET);

    /* Cleanup */
    for (int s = 0; s < maxhandle; s++)
        buf_free(&sig_vc[s]);
    free(sig_vc);
    free(offsets);
    free(prev_tidx);
    free(times);
}

/* ---- GEOM block ----
 *
 * Layout:
 *   type(1) + section_length(8) + uncomp_len(8) + geom_count(8) +
 *   zlib_compressed(varint widths)
 */
static void write_geom_block(FSTWriter *w)
{
    /* Build uncompressed data: num_signals varints of widths */
    FSTBuffer geom;
    buf_init(&geom);
    for (int i = 0; i < w->num_signals; i++) {
        buf_varint(&geom, (uint64_t)w->signals[i].width);
    }

    size_t bound = zlib_store_bound(geom.len);
    uint8_t *compressed = (uint8_t *)malloc(bound);
    size_t clen = zlib_store(compressed, geom.data, geom.len);

    /* total = 1(type) + 8(seclen) + 8(uncomp) + 8(count) + clen */
    size_t total = 1 + 8 + 8 + 8 + clen;

    write_u8(w->fp, FST_BL_GEOM);
    write_u64(w->fp, (uint64_t)(total - 1)); /* section_length */
    write_u64(w->fp, (uint64_t)geom.len);    /* uncompressed_length */
    write_u64(w->fp, (uint64_t)w->num_signals); /* geom_count */
    fwrite(compressed, 1, clen, w->fp);

    free(compressed);
    buf_free(&geom);
}

/* ---- HIER block ----
 *
 * Layout:
 *   type(1) + section_length(8) + uncomp_len(8) + gzip_compressed_data
 */
static void write_hier_block(FSTWriter *w)
{
    size_t bound = gzip_store_bound(w->hier.len);
    uint8_t *compressed = (uint8_t *)malloc(bound);
    size_t clen = gzip_store(compressed, w->hier.data, w->hier.len);

    /* total = 1(type) + 8(seclen) + 8(uncomp) + clen */
    size_t total = 1 + 8 + 8 + clen;

    write_u8(w->fp, FST_BL_HIER);
    write_u64(w->fp, (uint64_t)(total - 1));       /* section_length */
    write_u64(w->fp, (uint64_t)w->hier.len);        /* uncompressed_length */
    fwrite(compressed, 1, clen, w->fp);

    free(compressed);
}

/* ---- Close and finalize ---- */

void fst_close(FSTWriter *w)
{
    if (!w) return;

    if (w->fp && w->defs_ended) {
        /* Write HDR placeholder (330 bytes) */
        write_hdr_block(w);

        /* Block order: HDR → VCDATA → GEOM → HIER */
        write_vcdata_block(w);
        write_geom_block(w);
        write_hier_block(w);

        /* Re-write HDR with final start/end times */
        write_hdr_block(w);
    }

    if (w->fp) {
        fclose(w->fp);
    }

    for (int i = 0; i < w->num_signals; i++) {
        free(w->signals[i].scope);
        free(w->signals[i].name);
    }

    buf_free(&w->hier);
    free(w->changes);
    free(w->current_scope);

    free(w);
}
