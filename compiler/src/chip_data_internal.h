#ifndef JZ_HDL_CHIP_DATA_INTERNAL_H
#define JZ_HDL_CHIP_DATA_INTERNAL_H

#include "third_party/jsmn.h"

const char *jz_chip_builtin_json(const char *chip_id);

/* Shared JSMN helpers (defined in chip_data.c) */
int jz_json_token_eq(const char *json, const jsmntok_t *tok, const char *s);
int jz_json_token_eq_ci(const char *json, const jsmntok_t *tok, const char *s);
int jz_json_skip(const jsmntok_t *toks, int count, int index);
int jz_json_token_to_uint(const char *json, const jsmntok_t *tok, unsigned *out);
char *jz_json_token_strdup(const char *json, const jsmntok_t *tok);

#endif /* JZ_HDL_CHIP_DATA_INTERNAL_H */
