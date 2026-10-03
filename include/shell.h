#ifndef REINHARD_SHELL_H
#define REINHARD_SHELL_H

#include <stddef.h>

typedef int (*command_fn)(int argc, char **argv);

typedef struct {
    const char *name;
    const char *usage;
    const char *help;
    command_fn  run;
} command_t;

extern const command_t shell_commands[];
extern const size_t    shell_command_count;

void shell_run(void) __attribute__((noreturn));

#endif
