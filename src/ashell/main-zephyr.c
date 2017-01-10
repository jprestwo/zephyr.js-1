// Copyright (c) 2016, Intel Corporation.

#include <zephyr.h>
#include <string.h>
#include <shell/shell.h>
#include <misc/printk.h>

#include <uart.h>
#include <drivers/console/uart_console.h>

#include "jerry-api.h"
#include "comms-uart.h"

#include "shell-state.h"

static int shell_cmd_version(int argc, char *argv[])
{
    uint32_t version = sys_kernel_version_get();
    printk("Jerryscript API %d.%d\n", JERRY_API_MAJOR_VERSION, JERRY_API_MINOR_VERSION);
    printk("Zephyr version %d.%d.%d\n", (int)SYS_KERNEL_VER_MAJOR(version),
        (int)SYS_KERNEL_VER_MINOR(version),
        (int)SYS_KERNEL_VER_PATCHLEVEL(version));
    return 0;
}

static int shell_clear(int argc, char *argv[])
{
    printk(ANSI_CLEAR);
    comms_clear();
    return 0;
}

static int shell_status(int argc, char *argv[])
{
    comms_print_status();
    return 0;
}

#ifdef REDIRECT_ASHELL

// Hook to put back the stdout from the ACM to the A101 uart
extern void __stdout_hook_install(int(*hook)(int));

/* While app processes one input line, Zephyr will have another line
buffer to accumulate more console input. */
static struct uart_console_input line_bufs[2];

static struct nano_fifo free_queue;
static struct nano_fifo used_queue;

char *zephyr_getline(void)
{
    static struct uart_console_input *cmd;

    /* Recycle cmd buffer returned previous time */
    if (cmd != NULL) {
        nano_fifo_put(&free_queue, cmd);
    }

    cmd = nano_fifo_get(&used_queue, TICKS_UNLIMITED);
    return cmd->line;
}

static int std_out(int c)
{
    printk("%c", c);
    return 1;
}

static int shell_ashell_activate(int argc, char *argv[])
{
    int i;

    __stdout_hook_install(std_out);

    printk("Redirecting input to ashell\n");
    nano_fifo_init(&used_queue);
    nano_fifo_init(&free_queue);
    for (i = 0; i < sizeof(line_bufs) / sizeof(*line_bufs); i++) {
        nano_fifo_put(&free_queue, &line_bufs[i]);
    }

    uart_register_input(&free_queue, &used_queue, NULL);
    while (1) {
        char *s;
        fflush(stdout);
        s = zephyr_getline();
        if (*s) {
            uint32_t len = strnlen(s, MAX_LINE_LEN);
            ashell_main_state(s, len);
        }
    }
    return 0;
}
#endif

#define SHELL_COMMAND(name,cmd) { name, cmd }

static const struct shell_cmd commands[] =
{
  SHELL_COMMAND("version", shell_cmd_version),
  SHELL_COMMAND("clear", shell_clear),
  SHELL_COMMAND("status", shell_status),
#ifdef REDIRECT_ASHELL
  SHELL_COMMAND("ashell", shell_ashell_activate),
#endif
  SHELL_COMMAND(NULL, NULL)
};

#ifdef ASHELL_MAIN
void main(void)
#else
void main_development_shell(void)
#endif
{
    SHELL_REGISTER(system_get_prompt(), commands);
}
