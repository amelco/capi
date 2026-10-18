/* Simple networking library
 *
 * This is a very naive implementation of network capabilities. It was developed for educational purposes only.
 * The server listens on an IPv4 loopback socket (127.0.0.1).
 * Client connections are IPv4 only, and the address must be numeric (no DNS names).
 * It doesn't have any security concerns.
 *
 * Limitations:
 * - It is very opnionated. Lots of stuff is defined and the user cannot modify them (for instance,
 *     it is not possible to choose from which address the server will be listening to. It always
 *     listens on the loopback address (127.0.0.1). You can only define the port it listens to.
 * - The payload is sent at once and it is expected that a single recv read all of the payload.
 * - The payload is not yet dynamic.

 * by Andre Herman Freire Bezerra (2026)
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>

#define AHB_LIB_IMPLEMENTATION
#include "ahb_lib.h"

// #define DEBUG true
#define CLOSE(sock) close(sock);
#define CLIENT_TIMEOUT_SEC 10

typedef enum {
    VERB_GET,
    VERB_POST,
    VERB_QUANTITY_
} Verb;

typedef struct {
    Verb verb;
    char *resource;
    char *query;
    char *body;
} Response;

// server functions
int server_connect(int port);
int accept_client(int server);
Response read_from_client(int client, char *payload, int payload_size);
void request_header(char *out, const char *payload, int size, const char *name);
void request_cookie(char *out, const char *payload, int size, const char *name);
void random_token(char *out, size_t size);
void send_to_client(int client, char *payload, int payload_size);

// extra_headers is a NUL-terminated block of complete header lines (each ending in CRLF)
// as the caller must trust it (it goes straight onto the wire). Pass NULL for none.
void send_200_to_client_ex(int client, char* payload, int payload_size, const char *content_type, const char *extra_headers);
// binary-safe variant: payload may contain NUL bytes (payload_size is authoritative)
void send_200_to_client_bytes(int client, char* payload, int payload_size, const char *content_type, const char *extra_headers);
// cookie_attributes is the raw attribute list (e.g. "HttpOnly; Secure; SameSite=Strict; Path=/")
void send_200_to_client_cookie(int client, char* payload, int payload_size, const char *content_type,
                               const char *cookie_name, const char *cookie_value, const char *cookie_attributes);
void send_200_to_client_json(int client, char* payload, int payload_size);
void send_200_to_client(int client, char* payload, int payload_size);
void send_200_to_client_js(int client, char* payload, int payload_size);
void send_404_to_client(int client);
void send_500_to_client(int client);

void free_response(Response *res);


int client_connect(const char *addr, int port);
// void send_to_server(int sock, const char *msg);

#endif // NETWORK_H


/* ******************** 
 * Implementation
 * ********************
 */

#ifdef NETWORK_IMPLEMENTATION

#define MAX_RESOURCE_LEN 256

int _connect(int port, char* address)
{
    bool is_client = address != NULL;
    int reuseaddr = 1;

    if (is_client) {
        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in addr = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
        };
        inet_pton(AF_INET, address, &addr.sin_addr);
        connect(sock, (struct sockaddr *)&addr, sizeof(addr));
        return sock;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(reuseaddr));

    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK),
    };

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ahb_Log(FATAL, "Could not bind the server to port %d", port);
    }
    if (listen(sock, SOMAXCONN) < 0) {
        ahb_Log(FATAL, "Could not listen on port %d", port);
    }
    return sock;
}

int server_connect(int port)
{
    signal(SIGPIPE, SIG_IGN);
    return _connect(port, NULL);
}

int accept_client(int server)
{
    int client;
    do {
        client = accept(server, NULL, NULL);
    } while (client < 0 && errno == EINTR);

    if (client < 0) {
        ahb_Log(ERROR, "accept() failed: %s", strerror(errno));
        return -1;
    }

    struct timeval timeout = { .tv_sec = CLIENT_TIMEOUT_SEC, .tv_usec = 0 };
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    return client;
}

int client_connect(const char* address, int port)
{
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {
       .sin_family = AF_INET,
       .sin_port = htons(port),
    };
    inet_pton(AF_INET, address, &addr.sin_addr);
    connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    return sock;
}

static int _hex_value(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// Fills `out` with a random lowercase hex token: size-1 chars (rounded down to an even count)
// plus a NUL. Reads /dev/urandom, falling back to rand() if it cannot be opened.
void random_token(char *out, size_t size)
{
    if (size == 0) return;
    if (size == 1) { out[0] = '\0'; return; }

    size_t hex_len = size - 1;
    if (hex_len % 2 != 0) hex_len -= 1;

    unsigned char bytes[64];
    size_t byte_len = hex_len / 2;
    if (byte_len > sizeof bytes) byte_len = sizeof bytes;

    FILE *urandom = fopen("/dev/urandom", "rb");
    size_t got = 0;
    if (urandom) {
        got = fread(bytes, 1, byte_len, urandom);
        fclose(urandom);
    }
    for (size_t i = got; i < byte_len; ++i) bytes[i] = (unsigned char)rand();

    static const char hex[] = "0123456789abcdef";
    size_t n = 0;
    for (size_t i = 0; i < byte_len; ++i) {
        out[n++] = hex[bytes[i] >> 4];
        out[n++] = hex[bytes[i] & 0x0f];
    }
    out[n] = '\0';
}

// The request target can carry a query string ("GET /?user=bob HTTP/1.1"); only the path is
// the resource, so everything from '?' on is dropped 
void _get_resource(char *resource, const char *payload, int size)
{
    char *space = strchr(payload, ' ');
    if (!space) return;
    char *ini = space + 1;
    int len = 0;
    for (char *tmp = ini; tmp[0] != ' ' && tmp[0] != '?' && tmp[0] != '\0'; tmp++, len++) { }
    if (len > size - 1) len = size - 1;
    strncpy(resource, ini, len);
    resource[len] = '\0';

    // Clients percent-encode the path (ex. "@" becomes "%40"). Decode back to the real characters
    int w = 0;
    for (int r = 0; r < len; ++r) {
        if (resource[r] == '%' && r + 2 < len) {
            int hi = _hex_value(resource[r + 1]);
            int lo = _hex_value(resource[r + 2]);
            if (hi >= 0 && lo >= 0) {
                resource[w++] = (char)((hi << 4) | lo);
                r += 2;
                continue;
            }
        }
        resource[w++] = resource[r];
    }
    resource[w] = '\0';
}

void _get_query(char *query, const char *payload, int size)
{
    char *space = strchr(payload, ' ');
    if (!space) return;
    char *ini = strchr(space + 1, '?');
    if (!ini) return;
    ini += 1;
    int len = 0;
    for (char *tmp = ini; tmp[0] != ' ' && tmp[0] != '\0'; tmp++, len++) { }
    if (len > size - 1) len = size - 1;
    strncpy(query, ini, len);
    query[len] = '\0';

    int w = 0;
    for (int r = 0; r < len; ++r) {
        if (query[r] == '%' && r + 2 < len) {
            int hi = _hex_value(query[r + 1]);
            int lo = _hex_value(query[r + 2]);
            if (hi >= 0 && lo >= 0) {
                query[w++] = (char)((hi << 4) | lo);
                r += 2;
                continue;
            }
        }
        query[w++] = query[r];
    }
    query[w] = '\0';
}

void request_header(char *out, const char *payload, int size, const char *name)
{
    if (size <= 0) return;
    out[0] = '\0';

    size_t name_len = strlen(name);
    if (name_len == 0) return;

    const char *line = strstr(payload, "\r\n");
    while (line) {
        line += 2;
        if (line[0] == '\r' && line[1] == '\n') break;

        size_t i = 0;
        while (i < name_len) {
            char a = line[i];
            char b = name[i];
            if (a >= 'A' && a <= 'Z') a += 'a' - 'A';
            if (b >= 'A' && b <= 'Z') b += 'a' - 'A';
            if (a != b) break;
            i++;
        }
        if (i == name_len && line[i] == ':') {
            const char *value = line + name_len + 1;
            while (*value == ' ' || *value == '\t') value++;
            const char *end = strstr(value, "\r\n");
            int len = end ? (int)(end - value) : (int)strlen(value);
            if (len > size - 1) len = size - 1;
            memcpy(out, value, len);
            out[len] = '\0';
            return;
        }
        line = strstr(line, "\r\n");
    }
}

void request_cookie(char *out, const char *payload, int size, const char *name)
{
    if (size <= 0) return;
    out[0] = '\0';

    size_t name_len = strlen(name);
    if (name_len == 0) return;

    char cookies[1024] = {0};
    request_header(cookies, payload, sizeof cookies, "Cookie");
    if (cookies[0] == '\0') return;

    const char *p = cookies;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ';') p++;
        if (*p == '\0') break;

        const char *eq = strchr(p, '=');
        if (!eq) break;

        size_t key_len = (size_t)(eq - p);
        while (key_len > 0 && (p[key_len - 1] == ' ' || p[key_len - 1] == '\t')) key_len--;

        if (key_len == name_len && strncmp(p, name, name_len) == 0) {
            const char *value = eq + 1;
            const char *value_end = value;
            while (*value_end && *value_end != ';') value_end++;
            int len = (int)(value_end - value);
            if (len > size - 1) len = size - 1;
            memcpy(out, value, (size_t)len);
            out[len] = '\0';
            return;
        }

        const char *semicolon = strchr(p, ';');
        if (!semicolon) break;
        p = semicolon + 1;
    }
}

int _get_content_len(const char *payload)
{
    char *key = "Content-Length: ";
    char *value_ini = strstr(payload, key);
    if (!value_ini) return 0;

    value_ini += strlen(key);
    char *value_end = strchr(value_ini, '\r');
    if (!value_end) return 0;

    size_t len = (size_t)(value_end - value_ini);
    char value[20] = {0};
    if (len > sizeof(value) - 1) len = sizeof(value) - 1;
    memcpy(value, value_ini, len);
    return atoi(value);
}

Response read_from_client(int client, char *payload, int payload_size)
{
    Response res = {
        .verb = VERB_GET,
        .resource = NULL,
        .query = NULL,
        .body = NULL
    };

    int total = 0;
    int header_size = 0;
    int content_length = 0;

    while (total < payload_size - 1) {
        int n = recv(client, payload + total, payload_size - 1 - total, 0);
        if (n <= 0) {
            return res;
        }
        int len = strchr(payload, ' ') - payload;
        if (len > 0) {
            char verb[10] = {0};
            strncpy(verb, payload, len);
            if      (strcmp(verb, "GET") == 0)  res.verb = VERB_GET;
            else if (strcmp(verb, "POST") == 0) res.verb = VERB_POST;
        }

        total += n;
        payload[total] = '\0';

        if (content_length == 0) {
            content_length = _get_content_len(payload);
        }

        char *end_header = strstr(payload, "\r\n\r\n");
        if (end_header == NULL) continue;

        if (header_size == 0) {
            header_size = (int)(end_header + 4 - payload);

            char resource[MAX_RESOURCE_LEN] = {0};
            _get_resource(resource, payload, sizeof(resource));
            res.resource = strdup(resource);

            char query[MAX_RESOURCE_LEN] = {0};
            _get_query(query, payload, sizeof(query));
            if (query[0] != '\0') res.query = strdup(query);
        }

        if (total >= header_size + content_length) {
            if (content_length > 0) {
                res.body = strdup(payload + header_size);
            }
            return res;
        }
    }
    return res;
}

void _append_date_header(StringBuilder *sb)
{
    time_t now = time(NULL);
    struct tm gmt;
    char date[64] = {0};
    if (gmtime_r(&now, &gmt) == NULL) return;
    if (strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", &gmt) == 0) return;
    ahb_sb_append(sb, "Date: ");
    ahb_sb_append(sb, date);
    ahb_sb_append(sb, "\r\n");
}


static bool _send_all(int client, const char *data, size_t size)
{
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = send(client, data + sent, size - sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        if (n == 0) return false;
        sent += (size_t)n;
    }
    return true;
}

void _send_to_client(int client, int code, char *payload, int payload_size, char *content_type, const char *extra_headers)
{
    StringBuilder sb = {0};
    switch (code) {
    case 200: 
        ahb_sb_append(&sb, "HTTP/1.1 200 OK\r\n");
        ahb_sb_append(&sb, "Content-Type: ");
        ahb_sb_append(&sb, content_type);
        ahb_sb_append(&sb, "\r\n");
        ahb_sb_append(&sb, "Content-Length: ");
        char content_length[20] = {0};
        sprintf(content_length, "%d", payload_size);
        ahb_sb_append(&sb, content_length);
        ahb_sb_append(&sb, "\r\n");
        if (extra_headers) ahb_sb_append(&sb, extra_headers);
        ahb_sb_append(&sb, "Connection: close\r\n");
        _append_date_header(&sb);

        ahb_sb_append(&sb, "\r\n");
        ahb_sb_append(&sb, (char*)payload);
        break;
    case 400:
        ahb_sb_append(&sb, "HTTP/1.1 400 Not Found\r\n"); 
        ahb_sb_append(&sb, "Content-Type: text/html\r\n");
        ahb_sb_append(&sb, "Content-Length: 0\r\n");
        ahb_sb_append(&sb, "Connection: close\r\n");
        _append_date_header(&sb);
        ahb_sb_append(&sb, "\r\n");
        break;
    case 404:
        ahb_sb_append(&sb, "HTTP/1.1 404 Not Found\r\n"); 
        ahb_sb_append(&sb, "Content-Type: text/html\r\n");
        ahb_sb_append(&sb, "Content-Length: 0\r\n");
        ahb_sb_append(&sb, "Connection: close\r\n");
        _append_date_header(&sb);
        ahb_sb_append(&sb, "\r\n");
        break;
    case 500:
        ahb_sb_append(&sb, "HTTP/1.1 500 Internal Server Error\r\n");
        ahb_sb_append(&sb, "Content-Type: text/plain\r\n");
        ahb_sb_append(&sb, "Content-Length: 0\r\n");
        ahb_sb_append(&sb, "Connection: close\r\n");
        _append_date_header(&sb);
        ahb_sb_append(&sb, "\r\n");
        break;
    }
    
    _send_all(client, sb.data, strlen(sb.data));
    ahb_sb_free(&sb);
}

void send_200_to_client_ex(int client, char* payload, int payload_size, const char *content_type, const char *extra_headers)
{
    _send_to_client(client, 200, payload, payload_size, (char*)content_type, extra_headers);
}
void send_200_to_client_bytes(int client, char* payload, int payload_size, const char *content_type, const char *extra_headers)
{
    StringBuilder sb = {0};
    ahb_sb_append(&sb, "HTTP/1.1 200 OK\r\n");
    ahb_sb_append(&sb, "Content-Type: ");
    ahb_sb_append(&sb, content_type);
    ahb_sb_append(&sb, "\r\n");
    ahb_sb_append(&sb, "Content-Length: ");
    char content_length[20] = {0};
    sprintf(content_length, "%d", payload_size);
    ahb_sb_append(&sb, content_length);
    ahb_sb_append(&sb, "\r\n");
    if (extra_headers) ahb_sb_append(&sb, extra_headers);
    ahb_sb_append(&sb, "Connection: close\r\n");
    _append_date_header(&sb);
    ahb_sb_append(&sb, "\r\n");

    _send_all(client, sb.data, strlen(sb.data));
    if (payload_size > 0) _send_all(client, payload, (size_t)payload_size);
    ahb_sb_free(&sb);
}
void send_200_to_client_cookie(int client, char* payload, int payload_size, const char *content_type,
                               const char *cookie_name, const char *cookie_value, const char *cookie_attributes)
{
    StringBuilder headers = {0};
    ahb_sb_append(&headers, "Set-Cookie: ");
    ahb_sb_append(&headers, cookie_name);
    ahb_sb_append(&headers, "=");
    ahb_sb_append(&headers, cookie_value);
    ahb_sb_append(&headers, "; ");
    ahb_sb_append(&headers, cookie_attributes);
    ahb_sb_append(&headers, "\r\n");

    _send_to_client(client, 200, payload, payload_size, (char*)content_type, headers.data);
    ahb_sb_free(&headers);
}
void send_200_to_client_json(int client, char* payload, int payload_size)
{
    _send_to_client(client, 200, payload, payload_size, "application/json", NULL);
}
void send_200_to_client(int client, char* payload, int payload_size)
{
    _send_to_client(client, 200, payload, payload_size, "text/html", NULL);
}
void send_200_to_client_js(int client, char* payload, int payload_size)
{
    _send_to_client(client, 200, payload, payload_size, "application/javascript", NULL);
}
void send_404_to_client(int client)
{
    _send_to_client(client, 404, NULL, 0, NULL, NULL);
}
void send_400_to_client(int client)
{
    _send_to_client(client, 400, NULL, 0, NULL, NULL);
}
void send_500_to_client(int client)
{
    _send_to_client(client, 500, NULL, 0, NULL, NULL);
}

void free_response(Response *res)
{
    if (res->resource) free(res->resource);
    if (res->query) free(res->query);
    if (res->body) free(res->body);
}

#endif// NETWORK_IMPLEMENTATION
