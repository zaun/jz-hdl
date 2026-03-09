/*
 * jsmn.h
 * Minimalistic JSON parser in C.
 * Source: https://github.com/zserge/jsmn (MIT license)
 */
#ifndef JSMN_H
#define JSMN_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JSMN_UNDEFINED = 0,
    JSMN_OBJECT = 1,
    JSMN_ARRAY = 2,
    JSMN_STRING = 3,
    JSMN_PRIMITIVE = 4
} jsmntype_t;

typedef enum {
    JSMN_ERROR_NOMEM = -1,
    JSMN_ERROR_INVAL = -2,
    JSMN_ERROR_PART  = -3
} jsmnerr_t;

typedef struct {
    jsmntype_t type;
    int start;
    int end;
    int size;
} jsmntok_t;

typedef struct {
    unsigned int pos;
    unsigned int toknext;
    int toksuper;
} jsmn_parser;

void jsmn_init(jsmn_parser *parser);
int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
               jsmntok_t *tokens, unsigned int num_tokens);

#ifdef __cplusplus
}
#endif

#endif /* JSMN_H */

#ifdef JSMN_IMPLEMENTATION

static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser,
                                  jsmntok_t *tokens,
                                  size_t num_tokens)
{
    if (parser->toknext >= num_tokens) {
        return NULL;
    }
    jsmntok_t *tok = &tokens[parser->toknext++];
    tok->start = tok->end = -1;
    tok->size = 0;
    tok->type = JSMN_UNDEFINED;
    return tok;
}

static void jsmn_fill_token(jsmntok_t *token, jsmntype_t type,
                            int start, int end)
{
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

static int jsmn_parse_primitive(jsmn_parser *parser, const char *js,
                                size_t len, jsmntok_t *tokens,
                                size_t num_tokens)
{
    int start = (int)parser->pos;
    for (; parser->pos < len; parser->pos++) {
        char c = js[parser->pos];
        if (c == '\t' || c == '\r' || c == '\n' || c == ' ' ||
            c == ','  || c == ']'  || c == '}') {
            break;
        }
        if (c < 32 || c == '\"' || c == '\\') {
            parser->pos = (unsigned int)start;
            return JSMN_ERROR_INVAL;
        }
    }
    if (tokens == NULL) {
        parser->pos--;
        return 0;
    }
    jsmntok_t *token = jsmn_alloc_token(parser, tokens, num_tokens);
    if (token == NULL) {
        parser->pos = (unsigned int)start;
        return JSMN_ERROR_NOMEM;
    }
    jsmn_fill_token(token, JSMN_PRIMITIVE, start, (int)parser->pos);
    parser->pos--;
    return 0;
}

static int jsmn_parse_string(jsmn_parser *parser, const char *js,
                             size_t len, jsmntok_t *tokens,
                             size_t num_tokens)
{
    int start = (int)parser->pos;
    parser->pos++;

    for (; parser->pos < len; parser->pos++) {
        char c = js[parser->pos];
        if (c == '\"') {
            if (tokens == NULL) {
                return 0;
            }
            jsmntok_t *token = jsmn_alloc_token(parser, tokens, num_tokens);
            if (token == NULL) {
                parser->pos = (unsigned int)start;
                return JSMN_ERROR_NOMEM;
            }
            jsmn_fill_token(token, JSMN_STRING, start + 1, (int)parser->pos);
            return 0;
        }
        if (c == '\\' && parser->pos + 1 < len) {
            parser->pos++;
        }
    }
    parser->pos = (unsigned int)start;
    return JSMN_ERROR_PART;
}

void jsmn_init(jsmn_parser *parser)
{
    parser->pos = 0;
    parser->toknext = 0;
    parser->toksuper = -1;
}

int jsmn_parse(jsmn_parser *parser, const char *js, size_t len,
               jsmntok_t *tokens, unsigned int num_tokens)
{
    int r;
    int count = (int)parser->toknext;

    for (; parser->pos < len; parser->pos++) {
        char c = js[parser->pos];
        switch (c) {
        case '{':
        case '[': {
            count++;
            if (tokens == NULL) {
                break;
            }
            jsmntok_t *token = jsmn_alloc_token(parser, tokens, num_tokens);
            if (token == NULL) {
                return JSMN_ERROR_NOMEM;
            }
            token->type = (c == '{' ? JSMN_OBJECT : JSMN_ARRAY);
            token->start = (int)parser->pos;
            token->size = 0;
            if (parser->toksuper != -1) {
                tokens[parser->toksuper].size++;
            }
            parser->toksuper = (int)(parser->toknext - 1);
            break;
        }
        case '}':
        case ']': {
            if (tokens == NULL) {
                break;
            }
            jsmntype_t type = (c == '}' ? JSMN_OBJECT : JSMN_ARRAY);
            for (int i = (int)parser->toknext - 1; i >= 0; i--) {
                jsmntok_t *token = &tokens[i];
                if (token->start != -1 && token->end == -1) {
                    if (token->type != type) {
                        return JSMN_ERROR_INVAL;
                    }
                    token->end = (int)parser->pos + 1;
                    parser->toksuper = -1;
                    for (int j = i - 1; j >= 0; j--) {
                        if (tokens[j].start != -1 && tokens[j].end == -1) {
                            parser->toksuper = j;
                            break;
                        }
                    }
                    break;
                }
            }
            break;
        }
        case '\"':
            r = jsmn_parse_string(parser, js, len, tokens, num_tokens);
            if (r < 0) return r;
            count++;
            if (parser->toksuper != -1 && tokens != NULL) {
                tokens[parser->toksuper].size++;
            }
            break;
        case '\t':
        case '\r':
        case '\n':
        case ' ':
        case ':':
        case ',':
            break;
        default:
            r = jsmn_parse_primitive(parser, js, len, tokens, num_tokens);
            if (r < 0) return r;
            count++;
            if (parser->toksuper != -1 && tokens != NULL) {
                tokens[parser->toksuper].size++;
            }
            break;
        }
    }

    if (tokens != NULL) {
        for (int i = (int)parser->toknext - 1; i >= 0; i--) {
            if (tokens[i].start != -1 && tokens[i].end == -1) {
                return JSMN_ERROR_PART;
            }
        }
    }

    return count;
}

#endif /* JSMN_IMPLEMENTATION */
