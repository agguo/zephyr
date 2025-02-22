#include "lsh.h"

/* change this to any other UART peripheral if desired */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
#define MSG_SIZE 128

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];
static int rx_buf_pos;
static int count_arrow;
static char old_msg[MSG_SIZE];
static char copy[MSG_SIZE];
static int cur_pos;

static bool esc = false;

char history[MSG_SIZE];

void print_single(const char *c)
{
    uart_poll_out(uart_dev, *c);
}

/*
* Print a null-terminated string character by character to the UART interface
*/
void print_uart(const char *buf)
{
    int msg_len = strlen(buf);

    /* while (*buf != '\0') {
        print_single(buf);
        buf++;
    } */

    for (int i = 0; i < msg_len; i++) {
        print_single(&buf[i]);
    }
}

void print_line(const char *buf) {
    print_uart(buf);
    print_uart("\n\r");
}

const lshCmd *find_cmds(char *cmd_given) {
    for (int i = 0; i < num_cmds; i++) {
        const lshCmd *cmd_found = &lsh_cmd_list[i];
        if (strcmp(cmd_found->name, cmd_given) == 0) {
            return cmd_found;
        }
    }
    return NULL;
}

void check_cmds() {

    char *argv[16] = {0};
    int argc = 0;
    char *arg_new = NULL;

    for (int i = 0; i <= rx_buf_pos; i++) {
        char *arg = &rx_buf[i];

        if (*arg == ' ' || i == rx_buf_pos) {
            // null terminate current argument
            *arg = '\0';
            
            if (arg_new) {
                argv[argc++] = arg_new;
                arg_new = NULL;
            }

        } else if (!arg_new) {
            // beginning of new argument
            arg_new = arg;
        }
    }

    if (argc > 0) {
        const lshCmd *cmd_provided = find_cmds(argv[0]);

        if (!cmd_provided) {
            print_line("Command not found. Type 'help' to list all commands.");
        } else {
            cmd_provided->handler(argc, argv);
        }
    }

    return;
}

/*
* Read characters from UART until line end is detected. Afterwards push the
* data to the message queue.
*/
void serial_cb(const struct device *dev, void *user_data)
{
    uint8_t c;

    if (!uart_irq_update(uart_dev)) {
        return;
    }

    if (!uart_irq_rx_ready(uart_dev)) {
        return;
    }

    /* read until FIFO empty */
    while (uart_fifo_read(uart_dev, &c, 1) == 1) {
        if (c == '\n' || c == '\r') {

            /* if queue is full, message is silently dropped */
            k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);
            
            print_uart("\r\n");

            if (rx_buf_pos > 0) {
                check_cmds();
            }
            
            print_uart("lsh>> ");

            /* reset the buffer (it was copied to the msgq) */
            rx_buf[0]   = '\0';
            rx_buf_pos  = 0;
            cur_pos     = 0;
            count_arrow = 0;

        } else if (c == '\e') {
            esc = true;

        } else if (esc) {
            if (c == '[') {
                esc = true;
            } else {
                esc = false;
                uint32_t num = k_msgq_num_used_get(&uart_msgq);
                if (c == 'A') {
                    if ((num != 0) && (count_arrow < num)) {
                        if (count_arrow == 0) {
                            strcpy(old_msg, rx_buf); 
                        }
                        k_msgq_peek_at(&uart_msgq, &history, num-(++count_arrow));
                        for (int i = 0; i < rx_buf_pos-cur_pos; i++) {
                            print_uart("\e[C");
                        }
                        for (int i = 0; i < rx_buf_pos; i++) {
                            print_uart("\b \b");
                        }
                        print_uart(history);
                        strcpy(rx_buf, history);
                        rx_buf_pos = strlen(history);
                        cur_pos = strlen(history);
                    }

                } else if (c == 'B') {
                    if ((num != 0) && (count_arrow <= num) && (count_arrow > 0)) {
                        if (count_arrow != 1) {
                            k_msgq_peek_at(&uart_msgq, &history, num-(--count_arrow));
                            for (int i = 0; i < rx_buf_pos-cur_pos; i++) {
                                print_uart("\e[C");
                            }
                            for (int i = 0; i < rx_buf_pos; i++) {
                                print_uart("\b \b");
                            }

                            print_uart(history);
                            strcpy(rx_buf, history);
                            rx_buf_pos = strlen(history);
                            cur_pos = strlen(history);
                        } else {
                            --count_arrow;
                            for (int i = 0; i < rx_buf_pos-cur_pos; i++) {
                                print_uart("\e[C");
                            }
                            for (int i = 0; i < rx_buf_pos; i++) {
                                print_uart("\b \b");
                            }

                            print_uart(old_msg);
                            strcpy(rx_buf, old_msg);
                            rx_buf_pos = strlen(old_msg);
                            cur_pos = strlen(old_msg);
                        }
                    }
                } else if (c == 'C') {
                    if (cur_pos < rx_buf_pos) {
                        print_uart("\e[C");
                        cur_pos++;
                    }
                } else if (c == 'D') {
                    //print_single('\e');
                    if (cur_pos > 0) {
                        print_uart("\e[D");
                        cur_pos--;
                    }
                }
            }
        } else if ((c == 8 || c == 127) && rx_buf_pos > 0) {
            if (rx_buf_pos == cur_pos) {
                rx_buf[rx_buf_pos--] = '\0';
                cur_pos--;
                print_uart("\b \b");
            } else {
                strncpy(copy, rx_buf+cur_pos, rx_buf_pos-cur_pos);
                copy[rx_buf_pos-cur_pos] = '\0';

                print_uart("\e[D");
                print_uart(copy);
                print_uart(" \b");

                for (int i = 0; i < rx_buf_pos-cur_pos; i++) {
                    print_uart("\e[D");
                }
                rx_buf[--cur_pos] = '\0';
                strcat(rx_buf, copy);
                rx_buf_pos--;
            }

        } else if ((c != 8 && c != 127) && (rx_buf_pos < (sizeof(rx_buf) - 1))) {
            if (rx_buf_pos == cur_pos) {
                rx_buf[rx_buf_pos++] = c;
                rx_buf[rx_buf_pos] = '\0';
                cur_pos++;
                print_single(&c);
            } else {
                strncpy(copy, rx_buf+cur_pos, rx_buf_pos-cur_pos);
                copy[rx_buf_pos-cur_pos] = '\0';
                
                print_single(&c);
                print_uart(copy);
                for (int i = 0; i < rx_buf_pos-cur_pos; i++) {
                    print_uart("\e[D");
                }
                rx_buf[cur_pos++] = c;
                rx_buf[cur_pos] = '\0';
                strcat(rx_buf, copy);
                rx_buf_pos++;
            }
        }
        /* else: characters beyond buffer size are dropped */
    }
}

int lsh(void)
{
    // char tx_buf[MSG_SIZE];

    /* configure interrupt and callback to receive data */
    uart_irq_callback_user_data_set(uart_dev, serial_cb, NULL);
    uart_irq_rx_enable(uart_dev);

    print_uart("Welcome to Lua Shell (LSH)!\r\n");
    print_uart("lsh>> ");

    /* indefinitely wait for input from the user */
    /* while (k_msgq_get(&uart_msgq, &tx_buf, K_FOREVER) == 0) {
        print_uart("lsh>> ");
    
    }*/

    return 0;
}