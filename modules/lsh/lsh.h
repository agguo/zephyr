#ifndef LSH_H
#define LSH_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <zephyr/posix/unistd.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>

#include <string.h>

typedef struct lshCommand {
    const char *name;
    int (*handler)(int argc, char *argv[]);
    const char *info;
} lshCmd;

extern const lshCmd *const lsh_cmd_list;
extern int const num_cmds;

void print_uart(const char *buf);
void print_line(const char *buf);

int lsh(void);

#endif /* LSH_H */
