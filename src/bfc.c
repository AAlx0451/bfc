#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libgen.h>

void print_help(const char *prog_name) {
    printf("Usage: %s [OPTIONS] <input_file>\n\n", prog_name);
    printf("A highly optimized Brainfuck compiler frontend.\n\n");
    printf("Options:\n");
    printf("  -h, --help    Show this help message and exit.\n");
    printf("  -E            Preprocess only (run bfpp and print to stdout).\n");
    printf("  -c            Compile to C source code instead of binary.\n");
    printf("  -O            Enable IR optimizations and CC -O3 compilation.\n");
    printf("  -o <file>     Place the output into <file>.\n");
}

int main(int argc, char **argv) {
    int flag_O = 0, flag_E = 0, flag_c = 0;
    char *input_file = NULL;
    char *output_file = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_help(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-O") == 0) {
            flag_O = 1;
        } else if (strcmp(argv[i], "-E") == 0) {
            flag_E = 1;
        } else if (strcmp(argv[i], "-c") == 0) {
            flag_c = 1;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            output_file = argv[++i];
        } else {
            input_file = argv[i];
        }
    }

    if (!input_file) {
        fprintf(stderr, "Error: No input file specified.\n");
        fprintf(stderr, "Run '%s --help' for more information.\n", argv[0]);
        return 1;
    }

    char cmd[4096];

    if (flag_E) {
        snprintf(cmd, sizeof(cmd), "bfpp \"%s\"", input_file);
        return system(cmd) == 0 ? 0 : 1;
    }

    char default_out[1024];
    if (!output_file) {
        if (flag_c) {
            char *dot = strrchr(input_file, '.');
            if (dot) {
                snprintf(default_out, sizeof(default_out), "%.*s.c", (int)(dot - input_file), input_file);
            } else {
                snprintf(default_out, sizeof(default_out), "%s.c", input_file);
            }
            output_file = default_out;
        } else {
            output_file = "a.out";
        }
    }

    if (flag_c) {
        snprintf(cmd, sizeof(cmd), "bfpp \"%s\" | bfcc %s > \"%s\"", 
                 input_file, flag_O ? "-O" : "", output_file);
        return system(cmd) == 0 ? 0 : 1;
    }

    char tmp_c[] = "/tmp/bfc_temp_XXXXXX.c";
    int fd = mkstemp(tmp_c);
    if (fd == -1) {
        perror("Failed to create temporary file");
        return 1;
    }
    close(fd);

    snprintf(cmd, sizeof(cmd), "bfpp \"%s\" | bfcc %s > \"%s\"", 
             input_file, flag_O ? "-O" : "", tmp_c);
    if (system(cmd) != 0) {
        fprintf(stderr, "Error: Compilation failed during preprocessing/translation.\n");
        unlink(tmp_c);
        return 1;
    }

    snprintf(cmd, sizeof(cmd), "cc %s \"%s\" -o \"%s\"", 
             flag_O ? "-O3" : "", tmp_c, output_file);
    int res = system(cmd);
    unlink(tmp_c);

    return res == 0 ? 0 : 1;
}
