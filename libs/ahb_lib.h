/*
* Single header (STB style) library
* 
* This file has some functions that I find useful. 
*
* by Andre Herman F. Bezerra (amelco), 2026
*/

#ifndef AHB_LIB
#define AHB_LIB

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>

/* ---- Macros -------------------------------------------------------------- */
void _ahb_exit(const char *file, int line, const char *thing, const char *format, ...);

#define AHB_TODO(...) _ahb_exit(__FILE__, __LINE__, "TODO", __VA_ARGS__)
#define AHB_ABORT(...) _ahb_exit(__FILE__, __LINE__, "ABORT", __VA_ARGS__)
#define AHB_UNREACHABLE(...) _ahb_exit(__FILE__, __LINE__, "UNREACHABLE", __VA_ARGS__)

#define da_append(p, item)                                                      \
    do {                                                                        \
        if (!(p)->data || (p)->count >= (p)->capacity) {                        \
            (p)->capacity = !(p)->data ? 1 : (p)->capacity * 2;                 \
            (p)->data = realloc((p)->data, (p)->capacity * sizeof(*(p)->data)); \
        }                                                                       \
        (p)->data[(p)->count++] = (item);                                       \
    } while(0)
 
#define da_free(p) if ((p)->data != NULL) { free((p)->data); (p)->data = NULL; }
/* -------------------------------------------------------------------------- */

/* ---- Structs and Enums --------------------------------------------------- */
typedef struct {
    int curr_argn;
    int total_args;
    char **args;
} State;

typedef struct {
    char *data;
    size_t capacity;
    size_t count;
} TempAlloc;

typedef struct {
    char *data;
    size_t count;
} String;

typedef struct {
    char *data;
    size_t capacity;
    size_t count;
} StringBuilder;

typedef struct {
    String *data;
    size_t capacity;
    size_t count;
} StringList;

typedef struct {
    int day;
    int month;
    int year;
} Date;

typedef struct {
    char *output;
    char *error;
} ExecResult;

typedef enum {
    INFO,
    WARN,
    ERROR,
    FATAL
} LogLevel;

// logs shows timestamp by defatul. You always can turn this off
extern bool log_print_timestamp;
/* -------------------------------------------------------------------------- */

/* ******************************************************************************************
 * Test functions - This is to be removed from here soon. Not sure if it is that useful
 * Usage: ahb_test(test_name, command, result, validation_function, expected_result, user_data)
 *   test_name            just a name to identify the test
     command              command to be executed in the command line
     result               the ExecResult struct already allocated in memory. It will be filled in with the result of the execution command
     validation_function  it's a function pointer. You need to provide the validation function. It must match the type of the expected_result
     expected_result      it's the expected result of the validation function
     user_data            optional data to be passed if needed, so it can be used in the validation function

     We provide test functions for the basic C types (int, bool, float). If you need to test a custom type you can 
     create a new test_custom_type function. Note that you might need to make your own print result code instead of
     using the _PRINT_RESULT() macro because it only works for int types for now.
 */

    // DO NOT CHANGE those macros unless you really know what you're doing
    #ifndef AHB_TEST_DEBUG
    #define AHB_TEST_DEBUG false
    #endif
    
    // the expected value determines the return type of the validation function
    #define _TEST_PARAMS(T) char *name, ExecResult *result, char *cmd, T (*validation)(ExecResult*, const void*), T expected, const void* userdata
    
    #define _EXECUTE_TEST()                                                 \
        if (!ahb_exec(result, cmd)) AHB_ABORT("could not execute command"); \
        if (AHB_TEST_DEBUG) printf("%s\n", result->output)
    
    #define _VALIDATE(T)                     \
        T got = validation(result, userdata)
    
    #define _PRINT_RESULT()                                                 \
        if (got == expected) {                                              \
            ahb_color_printf(COLOR_GREEN, "  PASS");                        \
            printf(": %s\n", name);                                         \
        }                                                                   \
        else {                                                              \
            ahb_color_printf(COLOR_RED, "  FAIL");                          \
            printf(": %s\t(Got: %d, Expected: %d)\n", name, got, expected); \
        }                                                                   
    
    // test functions
    void ahb_test_int(_TEST_PARAMS(int));
    void ahb_test_bool(_TEST_PARAMS(bool));
/* ****************************************************************************************** */

/* *** Hash Map ***************************************************************************** */
/*
 * NOTE: naive implementatio of a hash table, using linear search
 *       replace for a faster implementation later.
 *       char* key -> any value type.
 *
 * --- Scalar value ---
 *     Map m = {0};
 *     double ratio = 0.5;
 *     map_insert(&m, "ratio", &ratio, sizeof ratio);
 *
 *     double *r = map_search(&m, "ratio");
 *     if (r) printf("%f\n", *r);
 *     map_free(&m, NULL);
 *
 * --- String value (the value IS the bytes, so pass strlen + 1) ---
 *     map_insert(&m, email, code, strlen(code) + 1);
 *
 *     char *stored_code = map_search(&m, email);
 *     if (stored_code) printf("%s\n", stored_code);
 *
 * --- Struct value (the map shares the struct's nested pointers, so hand over ownership) ---
 *     static void free_flaglist_cb(void *v) { free_flag_list((FlagList *)v); }
 *
 *     Map flags_by_code = {0};
 *     FlagList list = get_user_flag_list(user_code);
 *     map_insert(&flags_by_code, user_code, &list, sizeof list);
 *
 *     FlagList *found = map_search(&flags_by_code, user_code);
 *     if (found) { ... }
 *
 *     // do NOT also call free_flag_list(&list): the map owns it now
 *     map_free(&flags_by_code, free_flaglist_cb);
 */
typedef struct {
    char *key;
    void *value;
} HashItem;

typedef struct {
    HashItem *data;
    size_t count;
    size_t capacity;
} Map;

void *ahb_map_search(Map *map, const char *key);
bool ahb_map_remove(Map *map, const char *key, void (*free_value)(void *));
void ahb_map_free(Map *map, void (*free_value)(void *));

/* ****************************************************************************************** */

// command line arguments management
void ahb_args_init(int argc, char **argv);
char *ahb_args_next(void);

// command execution
bool ahb_exec(ExecResult *result, char *cmd);
void ahb_exec_free(ExecResult *result);

// printf like functions
#define COLOR_RESET     "\033[0m"
#define COLOR_RED       "\033[31m"
#define COLOR_GREEN     "\033[32m"
#define COLOR_YELLOW    "\033[33m"
#define COLOR_BLUE      "\033[34m"
#define COLOR_DARKGRAY  "\033[38;5;238m"
#define COLOR_LIGHTGRAY  "\033[38;5;248m"
void ahb_color_printf(const char *color, const char *format, ...);

// temporary allocation
//   temp_alloc() will store the content in a temporary allocator that can be
//   freed using temp_alloc_free().
char *ahb_temp_alloc(const char *content);
void ahb_temp_alloc_free(void);

// file operations
bool ahb_file_exits(const char *path);
long ahb_get_file_size(const char *f);
char *ahb_read_entire_file(const char *path);

// String operations
String ahb_cstr_to_String(const char *cstr);
char* ahb_String_to_cstr(String str);
String ahb_String_trim(String str);
void ahb_print_String(String str);

void ahb_split(const char *cstr, char sep, StringList *l);
void ahb_split_free(StringList *l);

void ahb_sb_append(StringBuilder *dest, const char *text); 
void ahb_sb_free(StringBuilder *sb);
char *ahb_sb_to_cstr(StringBuilder sb);
char *ahb_str_replace_all(const char *src, const char *needle, const char *replacement);

// Logging functions
void ahb_Log(LogLevel level, const char *format, ...);
void ahb_Log_nonl(LogLevel level, const char *format, ...);

// utility functions
bool ahb_is_valid_date(const char *date, long long *date_lld);
long long date_to_lld(const char *date);

// strip ahb prefix when non conflict names is assured
#ifdef AHB_STRIP_PREFIX
#define ahb_test_int        test_int
#define ahb_test_bool       test_bool
#define args_init           ahb_args_init
#define args_next           ahb_args_next
#define exec                ahb_exec
#define exec_free           ahb_exec_free
#define color_printf        ahb_color_printf
#define temp_alloc          ahb_temp_alloc
#define temp_alloc_free     ahb_temp_alloc_free
#define read_entire_file    ahb_read_entire_file
#define is_valid_date       ahb_is_valid_date
#define file_exits          ahb_file_exits
#define get_file_size       ahb_get_file_size
#define cstr_to_String      ahb_cstr_to_String
#define String_to_cstr      ahb_String_to_cstr
#define String_trim         ahb_String_trim
#define print_String        ahb_print_String
#define sb_append           ahb_sb_append
#define sb_free             ahb_sb_free
#define sb_to_cstr          ahb_sb_to_cstr
#define str_replace_all     ahb_str_replace_all
#define split               ahb_split
#define split_free          ahb_split_free
#define Log                 ahb_Log
#define Log_nonl            ahb_Log_nonl
#define ahb_map_search      map_search
#define ahb_map_remove      map_remove
#define ahb_map_free        map_free
#define TODO                AHB_TODO
#define ABORT               AHB_ABORT
#define UNREACHABLE         AHB_UNREACHABLE
#endif // AHB_STRIP_PREFIX

#ifdef AHB_LIB_IMPLEMENTATION

bool log_print_timestamp = true;

static State st = {0};
static TempAlloc allocator = {0};

void _ahb_exit(const char *file, int line, const char *thing, const char *format, ...)
{
    va_list args;
    printf("%s:%d: %s (", file, line, thing);
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf(")\n");
    exit(1);
}

/* **************************************************** 
 * Test Functions
 */
void ahb_test_int(_TEST_PARAMS(int))
{
    _EXECUTE_TEST();
    _VALIDATE(int);
    _PRINT_RESULT();
}

void ahb_test_bool(_TEST_PARAMS(bool))
{
    _EXECUTE_TEST();
    _VALIDATE(bool);
    _PRINT_RESULT();
}
/* **************************************************** */


void ahb_args_init(int argc, char **argv)
{
    st.curr_argn = 0;
    st.total_args = argc;
    st.args = argv;
}

char *ahb_args_next()
{
    st.curr_argn += 1;
    if (st.curr_argn >= st.total_args) return NULL;
    char *res = st.args[st.curr_argn];
    return res;
}

// TASK(20260820-124019): get also stderr output
bool ahb_exec(ExecResult *result, char *cmd)
{
    char buffer[128] = {0};
    FILE *pipe = popen(cmd, "r");

    if (pipe == NULL) return false;

    StringBuilder sb = {0};
    while (fgets(buffer, sizeof(buffer), pipe) != NULL) {
        ahb_sb_append(&sb, buffer);
    }

    int status = pclose(pipe);

    result->output = strdup(sb.data ? sb.data : "");
    ahb_sb_free(&sb);

    // pclose failed, command killed by a signal or failed
    if (status == -1 || !WIFEXITED(status) || WEXITSTATUS(status) != 0) return false;

    return true;
}

char *ahb_sb_to_cstr(StringBuilder sb)
{
    char *cstr = malloc(sb.count + 1);
    strncpy(cstr, sb.data, sb.count);
    cstr[sb.count] = '\0';
    return cstr;
}

// Returns a heap copy of `src` with every occurrence of `needle` replaced by `replacement`.
// A NULL/empty `needle` returns a plain copy of `src` (so it never loops forever).
char *ahb_str_replace_all(const char *src, const char *needle, const char *replacement)
{
    if (src == NULL) src = "";

    StringBuilder sb = {0};
    if (needle == NULL || needle[0] == '\0') {
        ahb_sb_append(&sb, src);
    }
    else {
        size_t needle_len = strlen(needle);
        const char *p = src;
        const char *hit;
        while ((hit = strstr(p, needle)) != NULL) {
            if (hit > p) {
                char *segment = strndup(p, (size_t)(hit - p));
                ahb_sb_append(&sb, segment);
                free(segment);
            }
            ahb_sb_append(&sb, replacement);
            p = hit + needle_len;
        }
        ahb_sb_append(&sb, p);
    }

    char *out = ahb_sb_to_cstr(sb);
    ahb_sb_free(&sb);
    return out;
}

void ahb_exec_free(ExecResult *res)
{
    free(res->output);
    free(res->error);
    res->output = NULL;
    res->error = NULL;
}

void ahb_color_printf(const char *color, const char *format, ...) {
    va_list args;

    printf("%s", color);

    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("%s", COLOR_RESET);
}

void _make_sure_is_initialized()
{
    if (allocator.data == NULL) {
        allocator.count = 0;
        allocator.capacity = 1024 * 1024; // 1 KB
        allocator.data = malloc(allocator.capacity);
        assert(allocator.data);
    }
}

char *ahb_temp_alloc(const char *content)
{
    if (content == NULL) return NULL;
    _make_sure_is_initialized();
    size_t size = strlen(content);
    if (allocator.count > allocator.capacity) {
        allocator.capacity *= 2;
        allocator.data = realloc(allocator.data, allocator.capacity);
        assert(allocator.data);
    }
    memcpy(allocator.data + allocator.count, content, size);
    allocator.data[allocator.count + size] = '\0';
    char *ptr = allocator.data + allocator.count;
    allocator.count += size + 1;
    return ptr;
}

void ahb_temp_alloc_free()
{
    free(allocator.data);
    allocator.capacity = 0;
    allocator.count = 0;
    allocator.data = NULL;
}

long ahb_get_file_size(const char *filepath)
{
    FILE *f = fopen(filepath, "r");
    // assert(f && "Could not open file");
    if (!f) return 0;
    fseek(f, 0L, SEEK_END);
    size_t size = ftell(f);
    rewind(f);
    fclose(f);
    return size;
}

bool ahb_file_exits(const char *path)
{
    FILE *f = fopen(path, "r");
    return f != NULL;
}

// this allocates memory
char *ahb_read_entire_file(const char *path)
{
    size_t size = ahb_get_file_size(path);
    if (size == 0) return NULL;

    FILE *f = fopen(path, "r");
    char *content = malloc(size + 1);
    assert(content && "Could not allocate memory. Buy more RAM.");
    int count = fread(content, 1, size, f);
    content[size] = '\0';
    assert((size_t)count == size && "Could not read entire file");
    fclose(f);
    return content;
}

// pass date = NULL if only want to validate
bool ahb_is_valid_date(const char *date, long long *date_lld)
{
    // accepts only DD-MM-YYYY
    StringList l = {0};
    ahb_split(date, '-', &l);
    if (l.count < 3) return false;

    char *day   = l.data[0].data;
    char *month = l.data[1].data;
    char *year  = l.data[2].data;
    if (strlen(day) > 2 || strlen(month) > 2 || strlen(year) > 4) return false;

    int dday   = atoi(day);
    int dmonth = atoi(month);
    int dyear  = atoi(year);
    if (dday <= 0 || dmonth <= 0 || dyear <= 0) return false;
    if (dday > 31 || dmonth > 12 || dyear < 1970 || dyear > 9999) return false;

    if (date_lld) {
        char yyyymmdd[9] = {0};
        sprintf(yyyymmdd, "%4d%02d%02d", dyear, dmonth, dday);
        *date_lld = date_to_lld(yyyymmdd);
    }
    return true;
}

// truncate the date into long long
// accepts the date ONLY in the format YYYYMMDD
long long date_to_lld(const char *date) {
    char *tmp = ahb_temp_alloc(date);
    tmp[8] = '\0';

    char date_[15] = {0};
    strncpy(date_, tmp, 8);
    strncpy(date_+8, tmp+9, 6);
    return atoll(date_);
}

String ahb_cstr_to_String(const char *cstr)
{
    return (String){
        .data = (char*)cstr,
        .count = strlen(cstr)
    };
}

void ahb_print_String(String str)
{
    for (size_t i = 0; i < str.count; ++i) {
        printf("%c", str.data[i]);
    }
}

// this function allocates memory
char* ahb_String_to_cstr(String str)
{
    char *res = malloc(str.count + 1);
    strncpy(res, str.data, str.count);
    res[str.count] = '\0';
    assert(strlen(res) == str.count);
    return res;
}

String ahb_String_trim(String str)
{
    if (str.count == 0) return str;
    String new = str;
    for (size_t i = new.count - 1; ; --i) {
        if (new.data[i] == ' ') new.count--;
        if (i == 0) break;
    }
    for (size_t i = 0; i < new.count; ++i) {
        if (new.data[i] == ' ') new.data++;
    }
    return new;
}

// this function allocates memory if sb is empty
void ahb_sb_append(StringBuilder *sb, const char *text)
{
    long len = strlen(text);
    while (!sb->data || sb->count + (size_t)len + 1 > sb->capacity) {
        sb->capacity = !sb->data ? 512 : sb->capacity * 2;
        sb->data = realloc(sb->data, sb->capacity);
        if (sb->data == NULL) AHB_ABORT("Could not allocate memory. Buy more RAM");
    }
    memcpy(sb->data + sb->count, text, strlen(text));
    sb->count += strlen(text);
    sb->data[sb->count] = '\0';
}

void ahb_sb_free(StringBuilder *sb)
{
    if (sb->data != NULL) {
        free(sb->data);
        sb->data = NULL;
    }
}

void ahb_split(const char *cstr, char sep, StringList *l)
{
    size_t ini = 0;
    unsigned long i;
    for (i=0; i < strlen(cstr); i++) {
        if (cstr[i] == sep) {
            String str = {0};
            str.data = strndup(cstr+ini, i-ini);
            str.count = strlen(str.data);
            da_append(l, str);
            i++;
            ini = i;
        }
    }
    String str = {0};
    str.data = strndup(cstr+ini, i-ini);
    str.count = strlen(str.data);
    da_append(l, str);
}

void ahb_split_free(StringList *l)
{
    if (l->data != NULL) {
        for (size_t i = 0; i < l->count; i++) {
            if (l->data[i].data != NULL) {
                free(l->data[i].data);
                l->data[i].data = NULL;
            }
        }
    }
    free(l->data);
    l->data = NULL;
}


void _ahb_Log(bool nl, LogLevel level, const char *format, va_list ap)
{
    flockfile(stdout);
    if (log_print_timestamp) {
        time_t rawtime;
        time(&rawtime);
        struct tm tm_info;
        if (localtime_r(&rawtime, &tm_info)) {
            char buf[64];
            strftime(buf, sizeof buf, "%a %b %e %H:%M:%S %Y", &tm_info);
            ahb_color_printf(COLOR_LIGHTGRAY, "[%s] ", buf);
        }
    }
    switch (level) {
    case INFO:  ahb_color_printf(COLOR_BLUE,   "INFO: ");  break;
    case WARN:  ahb_color_printf(COLOR_YELLOW, "WARN: ");  break;
    case ERROR: ahb_color_printf(COLOR_RED,    "ERROR: "); break;
    case FATAL: ahb_color_printf(COLOR_RED,    "FATAL: "); break;
    default: AHB_UNREACHABLE("LogLevel");
    }
    vprintf(format, ap);
    if (nl) printf("\n");
    funlockfile(stdout);
    if (level == FATAL) exit(1);

}

void ahb_Log(LogLevel level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    _ahb_Log(true, level, format, args);
    va_end(args);
}

// same as ahb_Log but without newLines on the end of the message
void ahb_Log_nonl(LogLevel level, const char *format, ...) {
    va_list args;
    va_start(args, format);
    _ahb_Log(false, level, format, args);
    va_end(args);
}

// Returns a pointer to the stored value (cast it back to your type), or NULL if not found.
void *map_search(Map *map, const char *key) {
    for (size_t i = 0; i < map->count; ++i) {
        if (strcmp(map->data[i].key, key) == 0) {
            return map->data[i].value;
        }
    }
    return NULL;
}

// Copies `value_size` raw bytes of `value`; the map owns its own copy (key is strdup'd).
void map_insert(Map *map, const char *key, const void *value, size_t value_size) {
    void *copy = malloc(value_size);
    if (copy) memcpy(copy, value, value_size);
    HashItem item = { strdup(key), copy };
    da_append(map, item);
}

// Removes `key` if present, freeing its key and value. Pass a callback to also release heap
// nested inside the value, or NULL for plain scalars/strings. Returns true when removed.
bool map_remove(Map *map, const char *key, void (*free_value)(void *)) {
    for (size_t i = 0; i < map->count; ++i) {
        if (strcmp(map->data[i].key, key) == 0) {
            free(map->data[i].key);
            if (free_value) free_value(map->data[i].value);
            free(map->data[i].value);
            map->data[i] = map->data[map->count - 1];
            map->count -= 1;
            return true;
        }
    }
    return false;
}

// Frees every key and value buffer. Pass a callback to also release heap nested inside
// the value, or NULL for plain scalars/strings.
void map_free(Map *map, void (*free_value)(void *)) {
    for (size_t i = 0; i < map->count; ++i) {
        free(map->data[i].key);
        if (free_value) free_value(map->data[i].value);
        free(map->data[i].value);
    }
    free(map->data);
    map->data = NULL;
    map->count = 0;
    map->capacity = 0;
}

#endif // AHB_LIB_IMPLEMENTATION

#endif // AHB_LIB

