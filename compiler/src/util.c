#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../include/util.h"

char *jz_strdup(const char *s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, s, len + 1);
    return copy;
}

char *jz_read_entire_file(const char *filename, size_t *out_size) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    long len = ftell(f);
    if (len < 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);

    char *buf = (char *)malloc((size_t)len + 1);
    if (!buf) {
        fclose(f);
        return NULL;
    }

    size_t read = fread(buf, 1, (size_t)len, f);
    fclose(f);
    if (read != (size_t)len) {
        free(buf);
        return NULL;
    }

    buf[len] = '\0';
    if (out_size) *out_size = (size_t)len;
    return buf;
}

int jz_buf_reserve(JZBuffer *buf, size_t new_cap)
{
    if (!buf) return -1;
    if (new_cap <= buf->cap) return 0;
    size_t cap = buf->cap ? buf->cap : 16;
    while (cap < new_cap) {
        cap *= 2;
    }
    unsigned char *data = (unsigned char *)realloc(buf->data, cap);
    if (!data) return -1;
    buf->data = data;
    buf->cap = cap;
    return 0;
}

int jz_buf_append(JZBuffer *buf, const void *data, size_t len)
{
    if (!buf || (!data && len != 0)) return -1;
    if (len == 0) return 0;
    if (jz_buf_reserve(buf, buf->len + len) != 0) return -1;
    memcpy(buf->data + buf->len, data, len);
    buf->len += len;
    return 0;
}

void jz_buf_free(JZBuffer *buf)
{
    if (!buf) return;
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}
