// NOTE: You only need to execute `gcc -o forge forge.c` once!
//       After that, you just need to execute `./build` to build the program

#define AHB_STRIP_PREFIX
#define AHB_LIB_IMPLEMENTATION
#include "libs/ahb_lib.h"

void build_api_impl(ExecResult *res, bool force_dev)
{
    Log_nonl(INFO, "Compiling C api...\n");
    if (!ahb_exec(res, "mkdir -p api/build")) Log(FATAL, "Could not create `api/build` directory");
    char cmd[512];
    snprintf(cmd, sizeof cmd, "gcc -o api/build/api -g -Wall -Wextra -Icore/src -Ilibs api/api.c -pthread");
    if (!ahb_exec(res, cmd)) Log(FATAL, "Could not compile api");
    printf("Done!\n");
}
void build_api(ExecResult *res)
{
    build_api_impl(res, false);
}

int main(int argc, char **argv) {
    char *default_arg = "all";
    args_init(argc, argv);
    char *arg = args_next();
    if (!arg) arg = default_arg;

    ExecResult res = {0};
    if (strcmp(arg, "help") == 0 || strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0 ){
        printf("Usage: %s <command>\n\n", argv[0]);
        printf("Commands:\n");
        printf("  all       build everything (default)\n");
        printf("  api       build fleabag api\n");
        return 0;
    }
    
    if (strcmp(arg, "all") == 0) {
        build_api(&res);
        return 0;
    }
    
    if(strcmp(arg, "api") == 0) {
        char *action = args_next();
        build_api(&res);
        return 0;
    }

    printf("Undefined command. See `%s help` for more details\n", argv[0]);
    return 1;
}

