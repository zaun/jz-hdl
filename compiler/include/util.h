/**
 * @file util.h
 * @brief General-purpose utilities for string handling and dynamic buffers.
 *
 * Provides functions for string duplication, file I/O, and a growable
 * byte buffer implementation.
 */

#ifndef JZ_HDL_UTIL_H
#define JZ_HDL_UTIL_H

#include <stddef.h>

/**
 * @brief Duplicate a string using malloc.
 * @param s String to duplicate.
 * @return Newly allocated copy, or NULL on failure.
 * @note Caller must free the result with free().
 */
char *jz_strdup(const char *s);

/**
 * @brief Read an entire file into memory.
 * @param filename Path to the file.
 * @param out_size Optional pointer to receive file size.
 * @return File contents as null-terminated string, or NULL on failure.
 * @note Caller must free the result with free().
 */
char *jz_read_entire_file(const char *filename, size_t *out_size);

/**
 * @struct JZBuffer
 * @brief A growable dynamic byte buffer.
 */
typedef struct JZBuffer {
    unsigned char *data;  /* Pointer to buffer data. */
    size_t         len;   /* Current length in bytes. */
    size_t         cap;   /* Allocated capacity in bytes. */
} JZBuffer;

/**
 * @brief Reserve capacity in a buffer.
 * @param buf Pointer to the buffer.
 * @param new_cap Minimum capacity required.
 * @return 0 on success, -1 on allocation failure.
 */
int   jz_buf_reserve(JZBuffer *buf, size_t new_cap);

/**
 * @brief Append data to a buffer.
 * @param buf Pointer to the buffer.
 * @param data Pointer to data to append.
 * @param len Number of bytes to append.
 * @return 0 on success, -1 on allocation failure.
 */
int   jz_buf_append(JZBuffer *buf, const void *data, size_t len);

/**
 * @brief Free a buffer and reset its state.
 * @param buf Pointer to the buffer.
 */
void  jz_buf_free(JZBuffer *buf);

#endif /* JZ_HDL_UTIL_H */
