/*
* Single header (STB style) library
* 
* This file has an basic json parser library. 
*
* by Andre Herman F. Bezerra (amelco), 2026
*/

#ifndef JSON_H
#define JSON_H

#include <stdbool.h>
#include <stddef.h>

#ifndef JSON_MAX_DEPTH
#define JSON_MAX_DEPTH 64
#endif

#ifndef JSON_MALLOC
#define JSON_MALLOC(size) malloc(size)
#endif

#ifndef JSON_FREE
#define JSON_FREE(ptr) free(ptr)
#endif

typedef enum {
    JSON_NULL,
    JSON_BOOL,
    JSON_NUMBER,
    JSON_STRING,
    JSON_ARRAY,
    JSON_OBJECT
} JsonType;

typedef struct JsonNode JsonNode;

typedef struct {
    char     *key;
    JsonNode *value;
} JsonMember;

struct JsonNode {
    JsonType type;
    union {
        bool   boolean;
        double number;
        char  *string;
        struct { JsonNode   **items;   size_t count; } array;
        struct { JsonMember  *members; size_t count; } object;
    } as;
};

typedef struct {
    size_t      offset;
    const char *message;
} JsonError;

JsonNode *json_parse(const char *text, JsonError *error);
JsonNode *json_parse_n(const char *text, size_t len, JsonError *error);
void      json_free(JsonNode *node);

bool        json_is_null(const JsonNode *node);
bool        json_is_bool(const JsonNode *node);
bool        json_is_number(const JsonNode *node);
bool        json_is_string(const JsonNode *node);
bool        json_is_array(const JsonNode *node);
bool        json_is_object(const JsonNode *node);

bool        json_bool(const JsonNode *node, bool fallback);
double      json_number(const JsonNode *node, double fallback);
const char *json_string(const JsonNode *node);

size_t      json_array_count(const JsonNode *node);
JsonNode   *json_array_get(const JsonNode *node, size_t index);

size_t      json_object_count(const JsonNode *node);
JsonMember *json_object_member(const JsonNode *node, size_t index);
JsonNode   *json_object_get(const JsonNode *node, const char *key);

#ifdef JSON_IMPLEMENTATION

#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *text;
    size_t      len;
    size_t      pos;
    JsonError  *error;
} JsonParser;

static void _json_error(JsonParser *p, const char *message)
{
    if (p->error) {
        p->error->offset = p->pos;
        p->error->message = message;
    }
}

static bool _json_is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static void _json_skip_ws(JsonParser *p)
{
    while (p->pos < p->len) {
        char c = p->text[p->pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') p->pos++;
        else break;
    }
}

static JsonNode *_json_new_node(JsonType type)
{
    JsonNode *node = JSON_MALLOC(sizeof *node);
    if (!node) return NULL;
    memset(node, 0, sizeof *node);
    node->type = type;
    return node;
}

static bool _json_grow(void **data, size_t *capacity, size_t element_size)
{
    size_t next_capacity = *capacity ? *capacity * 2 : 4;
    void *next = JSON_MALLOC(next_capacity * element_size);
    if (!next) return false;
    if (*data) {
        memcpy(next, *data, *capacity * element_size);
        JSON_FREE(*data);
    }
    *data = next;
    *capacity = next_capacity;
    return true;
}

static int _json_unescape(char c)
{
    switch (c) {
        case '"':  return '"';
        case '\\': return '\\';
        case '/':  return '/';
        case 'b':  return '\b';
        case 'f':  return '\f';
        case 'n':  return '\n';
        case 'r':  return '\r';
        case 't':  return '\t';
        default:   return -1;
    }
}

static bool _json_parse_string(JsonParser *p, char **out)
{
    size_t start = p->pos + 1;
    size_t i = start;
    size_t length = 0;

    while (i < p->len) {
        unsigned char c = (unsigned char)p->text[i];
        if (c == '"') break;
        if (c < 0x20) {
            p->pos = i;
            _json_error(p, "control character in string");
            return false;
        }
        if (c == '\\') {
            i++;
            if (i >= p->len) break;
            if (_json_unescape(p->text[i]) < 0) {
                p->pos = i - 1;
                _json_error(p, "invalid escape sequence");
                return false;
            }
        }
        length++;
        i++;
    }

    if (i >= p->len || p->text[i] != '"') {
        p->pos = i;
        _json_error(p, "unterminated string");
        return false;
    }

    char *buffer = JSON_MALLOC(length + 1);
    if (!buffer) {
        _json_error(p, "out of memory");
        return false;
    }

    size_t src = start;
    size_t dst = 0;
    while (src < i) {
        char c = p->text[src++];
        if (c == '\\') buffer[dst++] = (char)_json_unescape(p->text[src++]);
        else buffer[dst++] = c;
    }
    buffer[dst] = '\0';

    p->pos = i + 1;
    *out = buffer;
    return true;
}

static bool _json_parse_number(JsonParser *p, double *out)
{
    size_t start = p->pos;
    size_t i = start;

    if (i < p->len && p->text[i] == '-') i++;
    if (i >= p->len) { _json_error(p, "invalid number"); return false; }

    if (p->text[i] == '0') {
        i++;
    } else if (_json_is_digit(p->text[i])) {
        while (i < p->len && _json_is_digit(p->text[i])) i++;
    } else {
        _json_error(p, "invalid number");
        return false;
    }

    if (i < p->len && p->text[i] == '.') {
        i++;
        if (i >= p->len || !_json_is_digit(p->text[i])) { _json_error(p, "invalid number"); return false; }
        while (i < p->len && _json_is_digit(p->text[i])) i++;
    }

    if (i < p->len && (p->text[i] == 'e' || p->text[i] == 'E')) {
        i++;
        if (i < p->len && (p->text[i] == '+' || p->text[i] == '-')) i++;
        if (i >= p->len || !_json_is_digit(p->text[i])) { _json_error(p, "invalid number"); return false; }
        while (i < p->len && _json_is_digit(p->text[i])) i++;
    }

    size_t token_len = i - start;
    char *buffer = JSON_MALLOC(token_len + 1);
    if (!buffer) { _json_error(p, "out of memory"); return false; }
    memcpy(buffer, p->text + start, token_len);
    buffer[token_len] = '\0';

    char *end = NULL;
    double value = strtod(buffer, &end);
    bool ok = (end == buffer + token_len);
    JSON_FREE(buffer);

    if (!ok) { _json_error(p, "invalid number"); return false; }
    p->pos = i;
    *out = value;
    return true;
}

static bool _json_parse_literal(JsonParser *p, const char *literal)
{
    size_t n = strlen(literal);
    if (p->pos + n > p->len || memcmp(p->text + p->pos, literal, n) != 0) return false;
    p->pos += n;
    return true;
}

static JsonNode *_json_new_literal(JsonParser *p, JsonType type, bool value)
{
    JsonNode *node = _json_new_node(type);
    if (!node) { _json_error(p, "out of memory"); return NULL; }
    node->as.boolean = value;
    return node;
}

static JsonNode *_json_parse_value(JsonParser *p, int depth);

static JsonNode *_json_parse_array(JsonParser *p, int depth)
{
    if (depth >= JSON_MAX_DEPTH) { _json_error(p, "maximum nesting depth exceeded"); return NULL; }

    JsonNode *node = _json_new_node(JSON_ARRAY);
    if (!node) { _json_error(p, "out of memory"); return NULL; }

    p->pos++;
    _json_skip_ws(p);
    if (p->pos < p->len && p->text[p->pos] == ']') { p->pos++; return node; }

    size_t capacity = 0;
    for (;;) {
        JsonNode *item = _json_parse_value(p, depth + 1);
        if (!item) { json_free(node); return NULL; }

        if (node->as.array.count >= capacity) {
            if (!_json_grow((void **)&node->as.array.items, &capacity, sizeof *node->as.array.items)) {
                json_free(item);
                json_free(node);
                _json_error(p, "out of memory");
                return NULL;
            }
        }
        node->as.array.items[node->as.array.count++] = item;

        _json_skip_ws(p);
        if (p->pos >= p->len) { json_free(node); _json_error(p, "unterminated array"); return NULL; }

        char c = p->text[p->pos];
        if (c == ',') { p->pos++; _json_skip_ws(p); continue; }
        if (c == ']') { p->pos++; return node; }

        json_free(node);
        _json_error(p, "expected ',' or ']' in array");
        return NULL;
    }
}

static JsonNode *_json_parse_object(JsonParser *p, int depth)
{
    if (depth >= JSON_MAX_DEPTH) { _json_error(p, "maximum nesting depth exceeded"); return NULL; }

    JsonNode *node = _json_new_node(JSON_OBJECT);
    if (!node) { _json_error(p, "out of memory"); return NULL; }

    p->pos++;
    _json_skip_ws(p);
    if (p->pos < p->len && p->text[p->pos] == '}') { p->pos++; return node; }

    size_t capacity = 0;
    for (;;) {
        _json_skip_ws(p);
        if (p->pos >= p->len || p->text[p->pos] != '"') {
            json_free(node);
            _json_error(p, "expected string key in object");
            return NULL;
        }

        char *key = NULL;
        if (!_json_parse_string(p, &key)) { json_free(node); return NULL; }

        _json_skip_ws(p);
        if (p->pos >= p->len || p->text[p->pos] != ':') {
            JSON_FREE(key);
            json_free(node);
            _json_error(p, "expected ':' after object key");
            return NULL;
        }
        p->pos++;

        JsonNode *value = _json_parse_value(p, depth + 1);
        if (!value) { JSON_FREE(key); json_free(node); return NULL; }

        if (node->as.object.count >= capacity) {
            if (!_json_grow((void **)&node->as.object.members, &capacity, sizeof *node->as.object.members)) {
                JSON_FREE(key);
                json_free(value);
                json_free(node);
                _json_error(p, "out of memory");
                return NULL;
            }
        }
        node->as.object.members[node->as.object.count].key = key;
        node->as.object.members[node->as.object.count].value = value;
        node->as.object.count++;

        _json_skip_ws(p);
        if (p->pos >= p->len) { json_free(node); _json_error(p, "unterminated object"); return NULL; }

        char c = p->text[p->pos];
        if (c == ',') { p->pos++; continue; }
        if (c == '}') { p->pos++; return node; }

        json_free(node);
        _json_error(p, "expected ',' or '}' in object");
        return NULL;
    }
}

static JsonNode *_json_parse_value(JsonParser *p, int depth)
{
    _json_skip_ws(p);
    if (p->pos >= p->len) { _json_error(p, "unexpected end of input"); return NULL; }

    char c = p->text[p->pos];

    if (c == '{') return _json_parse_object(p, depth);
    if (c == '[') return _json_parse_array(p, depth);

    if (c == '"') {
        char *string = NULL;
        if (!_json_parse_string(p, &string)) return NULL;
        JsonNode *node = _json_new_node(JSON_STRING);
        if (!node) { JSON_FREE(string); _json_error(p, "out of memory"); return NULL; }
        node->as.string = string;
        return node;
    }

    if (c == 't') {
        if (!_json_parse_literal(p, "true")) { _json_error(p, "invalid literal"); return NULL; }
        return _json_new_literal(p, JSON_BOOL, true);
    }
    if (c == 'f') {
        if (!_json_parse_literal(p, "false")) { _json_error(p, "invalid literal"); return NULL; }
        return _json_new_literal(p, JSON_BOOL, false);
    }
    if (c == 'n') {
        if (!_json_parse_literal(p, "null")) { _json_error(p, "invalid literal"); return NULL; }
        return _json_new_literal(p, JSON_NULL, false);
    }
    if (c == '-' || _json_is_digit(c)) {
        double number = 0.0;
        if (!_json_parse_number(p, &number)) return NULL;
        JsonNode *node = _json_new_node(JSON_NUMBER);
        if (!node) { _json_error(p, "out of memory"); return NULL; }
        node->as.number = number;
        return node;
    }

    _json_error(p, "unexpected character");
    return NULL;
}

void json_free(JsonNode *node)
{
    if (!node) return;

    switch (node->type) {
        case JSON_STRING:
            JSON_FREE(node->as.string);
            break;
        case JSON_ARRAY:
            for (size_t i = 0; i < node->as.array.count; i++) json_free(node->as.array.items[i]);
            JSON_FREE(node->as.array.items);
            break;
        case JSON_OBJECT:
            for (size_t i = 0; i < node->as.object.count; i++) {
                JSON_FREE(node->as.object.members[i].key);
                json_free(node->as.object.members[i].value);
            }
            JSON_FREE(node->as.object.members);
            break;
        default:
            break;
    }

    JSON_FREE(node);
}

bool json_is_null(const JsonNode *node)   { return node && node->type == JSON_NULL; }
bool json_is_bool(const JsonNode *node)   { return node && node->type == JSON_BOOL; }
bool json_is_number(const JsonNode *node) { return node && node->type == JSON_NUMBER; }
bool json_is_string(const JsonNode *node) { return node && node->type == JSON_STRING; }
bool json_is_array(const JsonNode *node)  { return node && node->type == JSON_ARRAY; }
bool json_is_object(const JsonNode *node) { return node && node->type == JSON_OBJECT; }

bool json_bool(const JsonNode *node, bool fallback)
{
    return (node && node->type == JSON_BOOL) ? node->as.boolean : fallback;
}

double json_number(const JsonNode *node, double fallback)
{
    return (node && node->type == JSON_NUMBER) ? node->as.number : fallback;
}

const char *json_string(const JsonNode *node)
{
    return (node && node->type == JSON_STRING) ? node->as.string : NULL;
}

size_t json_array_count(const JsonNode *node)
{
    return (node && node->type == JSON_ARRAY) ? node->as.array.count : 0;
}

JsonNode *json_array_get(const JsonNode *node, size_t index)
{
    if (!node || node->type != JSON_ARRAY || index >= node->as.array.count) return NULL;
    return node->as.array.items[index];
}

size_t json_object_count(const JsonNode *node)
{
    return (node && node->type == JSON_OBJECT) ? node->as.object.count : 0;
}

JsonMember *json_object_member(const JsonNode *node, size_t index)
{
    if (!node || node->type != JSON_OBJECT || index >= node->as.object.count) return NULL;
    return &node->as.object.members[index];
}

JsonNode *json_object_get(const JsonNode *node, const char *key)
{
    if (!node || node->type != JSON_OBJECT || !key) return NULL;
    for (size_t i = 0; i < node->as.object.count; i++) {
        if (strcmp(node->as.object.members[i].key, key) == 0) return node->as.object.members[i].value;
    }
    return NULL;
}

JsonNode *json_parse_n(const char *text, size_t len, JsonError *error)
{
    if (error) { error->offset = 0; error->message = NULL; }
    if (!text) {
        if (error) error->message = "null input";
        return NULL;
    }

    JsonParser parser = { text, len, 0, error };
    JsonNode *root = _json_parse_value(&parser, 0);
    if (!root) return NULL;

    _json_skip_ws(&parser);
    if (parser.pos != parser.len) {
        _json_error(&parser, "trailing characters after value");
        json_free(root);
        return NULL;
    }
    return root;
}

JsonNode *json_parse(const char *text, JsonError *error)
{
    if (!text) return json_parse_n(NULL, 0, error);
    return json_parse_n(text, strlen(text), error);
}

#endif // JSON_IMPLEMENTATION

#endif // JSON_H
