#ifndef XLD_OPTIONS_H
#define XLD_OPTIONS_H

#include <stdbool.h>

typedef struct {
    const char *input_file;
    const char *output_file;
    const char *entry_symbol;
    bool verbose;
} xld_options_t;

void options_init(xld_options_t *opt);
void options_parse(int argc, char **argv, xld_options_t *opt);
void options_usage(const char *prog);

#endif

