#include <stdio.h>
#include <stdbool.h>
#include <pthread.h>
#include <sys/stat.h>

#define AHB_LIB_IMPLEMENTATION
#include "ahb_lib.h"

#define NETWORK_IMPLEMENTATION
#include "network.h"

#define JSON_IMPLEMENTATION
#include "json.h"

#define PAYLOAD_SIZE 8 * 1024 // 8 kb as defined by Apache (https://httpd.apache.org/docs/2.2/mod/core.html#limitrequestfieldsize)

char *json_has_string_property(Arena *arena, const char *json, const char *property_name)
{
    JsonNode *root = json_parse(json, NULL);
    if (!root) return NULL;

    const char *value = json_string(json_object_get(root, property_name));
    char *result = value ? arena_strdup(arena, value) : NULL;
    json_free(root);
    return result;
}

static char *query_param(Arena *arena, const char *query, const char *key)
{
    size_t key_len = strlen(key);
    for (const char *p = query; p && *p; ) {
        const char *end = strchr(p, '&');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (len > key_len && strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            return arena_strndup(arena, p + key_len + 1, len - key_len - 1);
        }
        p = end ? end + 1 : NULL;
    }
    return NULL;
}


typedef struct {
    int client;
    char *example;
} ClientJob;

// static pthread_mutex_t your_lock = PTHREAD_MUTEX_INITIALIZER;

void *handle_client(void *arg)
{
    ClientJob *job = arg;
    int client = job->client;
    char *example = job->example;
    free(job);

    // by default, 4MB arena size
    Arena arena = {0};
    arena_init(&arena);

    char payload[PAYLOAD_SIZE] = {0};
    Response res = read_from_client_arena(&arena, client, payload, PAYLOAD_SIZE);
    if (res.resource == NULL) {
        goto defer;
    }
    ahb_Log(INFO, "client %d: resource %s", client, res.resource);

    static_assert(VERB_QUANTITY_ == 2, "Incomplete switch-case");
    switch (res.verb) {
        case VERB_GET:
            if (strcmp(res.resource, "/") == 0) {
                char *content = ahb_read_entire_file_arena(&arena, "./frontend/index.html");
                if (!content) {
                    ahb_Log(ERROR, "Could not load the file index.html");
                    send_404_to_client(client);
                }
                else {
                    send_200_to_client(client, content, (int)strlen(content));
                }
            }
            else if (strcmp(res.resource, "/capi.svg") == 0) {
                char *content = ahb_read_entire_file_arena(&arena, "./frontend/capi.svg");
                if (!content) {
                    ahb_Log(ERROR, "Could not load the file capi.svg");
                    send_404_to_client(client);
                }
                else {
                    send_200_to_client_ex(client, content, (int)strlen(content), "image/svg+xml", NULL);
                }
            }
            // example: /hasUrlParam/paramValue
            else if (strstr(res.resource, "/hasUrlParam/")) {
                char *paramName = strstr(res.resource, "/hasUrlParam/") + strlen("/hasUrlParam/");
                if (*paramName == '\0') {
                    send_404_to_client(client);
                    break;
                }

                char *paramValue = res.query ? query_param(&arena, res.query, "paramValue") : NULL;
                if (!paramValue) {
                    if (!res.body) {
                        send_404_to_client(client);
                        break;
                    }
                    paramValue = json_has_string_property(&arena, res.body, "paramValue");
                }

                if (!paramValue)  {
                    ahb_Log(INFO, "This route have to have the 'paramValue' to work");
                    send_400_to_client(client);
                    break;
                }

                // Client variables are accessibel at this scope level
                ahb_Log(INFO, "Client's variable example = %s", example);

                char *response = "{\"value:\"\"a json response\"}";

                send_200_to_client_json(client, response, strlen(response));
            }
            else {
                ahb_Log(INFO, "GET resource '%s' not found", res.resource);
                send_404_to_client(client);
            }
            break;
        case VERB_POST:
            if (strcmp(res.resource, "/hasBodyParam") == 0) {
                if (!res.body) {
                    break;
                }

                const char *bodyParam = json_has_string_property(&arena, res.body, "bodyParam");
                if (!bodyParam) {
                    send_400_to_client(client);
                    break;
                }
                
                // do something with bodyParam, like create a db row

                // if IO is needed, must block it for thread safety
                // pthread_mutex_lock(&your_lock);
                // Thing thing = update_thing(...);
                // pthread_mutex_unlock(&your_lock);

                bool created = true;

                if (created) {
                    send_200_to_client(client, (char*)bodyParam, strlen(bodyParam));
                }
                else {
                    send_400_to_client(client);
                }
                break;
            }
            else {
                ahb_Log(INFO, "POST resource '%s' not found", res.resource);
                send_404_to_client(client);
            }
            break;
        default:
            AHB_UNREACHABLE("handle_client()");
    }

    defer:
    arena_free(&arena);
    CLOSE(client);
    return NULL;
}

int main() {
    // line-buffer stdout: when redirected to a file libc block-buffers it, so logs would never flush
    setvbuf(stdout, NULL, _IOLBF, 0);
    srand(time(NULL));

    ahb_Log(INFO, "Listening on http://localhost:6969...");

    int server = server_connect(6969);

    // do any initialization preparation here

    while (true) {
        int client = accept_client(server);
        if (client < 0) continue;

        // this is freed inside handle_client. It's fine for now, but we can think in a better way of doing this
        ClientJob *job = malloc(sizeof *job);
        if (!job) {
            ahb_Log(ERROR, "Could not allocate a client job");
            CLOSE(client);
            continue;
        }
        *job = (ClientJob){ .client = client, .example = "an example variable" };

        pthread_t thread;
        if (pthread_create(&thread, NULL, handle_client, job) != 0) {
            ahb_Log(ERROR, "Could not create a thread for client %d", client);
            free(job);
            CLOSE(client);
            continue;
        }
        pthread_detach(thread);
    }
    
    CLOSE(server);
    return 0;
}
