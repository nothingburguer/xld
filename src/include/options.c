#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "options.h"

void options_init(xld_options_t *opt) {
    opt->input_file   = NULL;
    opt->output_file  = "a.out";
    opt->entry_symbol = "_start";
    opt->silent       = false;
}

void options_usage(const char *prog) {
    printf(
        "usage: %s [options] input.o\n\n"
        "options:\n"
        "  -o <file>       output file (default: a.out)\n"
        "  -e <symbol>     entry symbol (default: _start)\n"
        "  -s              silent output\n"
        "  --help          show this help\n",
        prog
    );
}

void options_parse(int argc, char **argv, xld_options_t *opt) {
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o")) {
            if (++i >= argc) {
                fprintf(stderr, "xld: -o requires an argument\n");
                exit(1);
            }
            opt->output_file = argv[i];
        }
        else if (!strcmp(argv[i], "-e")) {
            if (++i >= argc) {
                fprintf(stderr, "xld: -e requires an argument\n");
                exit(1);
            }
            opt->entry_symbol = argv[i];
        }
        else if (!strcmp(argv[i], "-s")) {
            opt->silent = true;
        }
        else if (!strcmp(argv[i], "--help")) {
            options_usage(argv[0]);
            exit(0);
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "xld: unknown option '%s'\n", argv[i]);
            exit(1);
        }
        else {
            opt->input_file = argv[i];
        }
    }

    if (!opt->input_file) {
        fprintf(stderr, "xld: no input file provided\n");
        options_usage(argv[0]);
        exit(1);
    }
}

