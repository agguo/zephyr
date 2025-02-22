# include "lsh.h"

int cmd_hello(int argc, char *argv[]) {
    print_line("Hello World!");
    return 0;
}

int cmd_help(int argc, char *argv[]) {
    for (int i = 0; i < num_cmds; i++) {
        const lshCmd *cmd_found = &lsh_cmd_list[i];
        print_uart(cmd_found->name);
        print_uart(" : ");
        print_line(cmd_found->info);
    }
    return 0;
}

static const lshCmd lsh_cmds_list_internal[] = {
    {"hello", cmd_hello, "Says hello"},
    {"help", cmd_help, "List all commands"},
};

const lshCmd *const lsh_cmd_list = lsh_cmds_list_internal;

const int num_cmds = (sizeof(lsh_cmds_list_internal) / sizeof(lsh_cmds_list_internal[0]));