#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TAPE_CONST_VAL 65536

void flush_op(FILE *out, int *op, int *count) {
    if (*op) {
        if (*op == '+') fprintf(out, "    tape[ptr] += %d;\n", *count);
        else if (*op == '-') fprintf(out, "    tape[ptr] -= %d;\n", *count);
        else if (*op == '>') fprintf(out, "    ptr += %d;\n", *count);
        else if (*op == '<') fprintf(out, "    ptr -= %d;\n", *count);
        *op = 0;
        *count = 0;
    }
}

void transpile(FILE *in, FILE *out, int optimize) {
    fprintf(out, "#include <stdio.h>\n");
    fprintf(out, "#define TAPE %d\n", TAPE_CONST_VAL); 
    fprintf(out, "unsigned char tape[TAPE];\n");
    fprintf(out, "unsigned char vstack[TAPE];\n");
    fprintf(out, "int pstack[TAPE];\n");
    fprintf(out, "int ptr = 0;\n");
    fprintf(out, "int vsp = 0;\n");
    fprintf(out, "int psp = 0;\n");
    fprintf(out, "int main() {\n");

    int c;
    int last_op = 0;
    int count = 0;

    while ((c = fgetc(in)) != EOF) {
        if (c == '/') {
            int next = fgetc(in);
            if (next == '*') {
                if (optimize) flush_op(out, &last_op, &count);
                fprintf(out, "    /*");
                while (1) {
                    int nc = fgetc(in);
                    if (nc == EOF) break;
                    fputc(nc, out);
                    if (nc == '*') {
                        int nnc = fgetc(in);
                        if (nnc == '/') {
                            fputc('/', out);
                            break;
                        }
                        ungetc(nnc, in);
                    }
                }
                fprintf(out, "*/\n");
                continue;
            } else {
                ungetc(next, in);
            }
        }

        if (c == '_') {
            int next = fgetc(in);
            if (next == '>') {
                if (optimize) flush_op(out, &last_op, &count);
                fprintf(out, "    ptr = TAPE - 1;\n");
                continue;
            } else if (next == '<') {
                if (optimize) flush_op(out, &last_op, &count);
                fprintf(out, "    ptr = 0;\n");
                continue;
            } else if (next == '^') {
                if (optimize) flush_op(out, &last_op, &count);
                fprintf(out, "    if (vsp < TAPE) vstack[vsp++] = tape[ptr];\n");
                continue;
            } else if (next == '&') {
                if (optimize) flush_op(out, &last_op, &count);
                fprintf(out, "    if (vsp > 0) tape[ptr] = vstack[--vsp];\n");
                continue;
            } else if (next == '#') {
                if (optimize) flush_op(out, &last_op, &count);
                fprintf(out, "    if (psp < TAPE) pstack[psp++] = ptr;\n");
                continue;
            } else if (next == '$') {
                if (optimize) flush_op(out, &last_op, &count);
                fprintf(out, "    if (psp > 0) ptr = pstack[--psp];\n");
                continue;
            } else if (next == '?') {
                int third = fgetc(in);
                if (third == '>') {
                    if (optimize) flush_op(out, &last_op, &count);
                    fprintf(out, "    if (ptr == TAPE - 1) tape[ptr] = 0;\n");
                    continue;
                } else if (third == '<') {
                    if (optimize) flush_op(out, &last_op, &count);
                    fprintf(out, "    if (ptr == 0) tape[ptr] = 0;\n");
                    continue;
                } else {
                    ungetc(third, in);
                    ungetc(next, in);
                }
            } else {
                ungetc(next, in);
            }
        }
        else if (strchr("+-<>.,[]", c)) {
            if (optimize) {
                if (strchr("+-<>", c)) {
                    if (last_op == c) {
                        count++;
                    } else {
                        flush_op(out, &last_op, &count);
                        last_op = c;
                        count = 1;
                    }
                } else {
                    flush_op(out, &last_op, &count);
                    if (c == '.') fprintf(out, "    putchar(tape[ptr]);\n");
                    else if (c == ',') fprintf(out, "    tape[ptr] = getchar();\n");
                    else if (c == '[') fprintf(out, "    while(tape[ptr]) {\n");
                    else if (c == ']') fprintf(out, "    }\n");
                }
            } else {
                if (c == '+') fprintf(out, "    tape[ptr]++;\n");
                else if (c == '-') fprintf(out, "    tape[ptr]--;\n");
                else if (c == '>') fprintf(out, "    ptr++;\n");
                else if (c == '<') fprintf(out, "    ptr--;\n");
                else if (c == '.') fprintf(out, "    putchar(tape[ptr]);\n");
                else if (c == ',') fprintf(out, "    tape[ptr] = getchar();\n");
                else if (c == '[') fprintf(out, "    while(tape[ptr]) {\n");
                else if (c == ']') fprintf(out, "    }\n");
            }
        }
    }
    if (optimize) flush_op(out, &last_op, &count);

    fprintf(out, "    return 0;\n}\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s [options] <input_file>\n", argv[0]);
        fprintf(stderr, "Options:\n");
        fprintf(stderr, "  -o <output_file>  Specify output file name (default: a.out)\n");
        fprintf(stderr, "  -O                Enable optimization\n");
        fprintf(stderr, "  -E                Preprocess only\n");
        return 1;
    }

    int flag_O = 0;
    int flag_E = 0;
    char *input_file = NULL;
    char *output_file = "a.out";

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-O") == 0) flag_O = 1;
        else if (strcmp(argv[i], "-E") == 0) flag_E = 1;
        else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) output_file = argv[++i];
            else {
                fprintf(stderr, "Error: -o requires an argument\n");
                return 1;
            }
        } else {
            if (input_file != NULL) {
                 fprintf(stderr, "Error: Multiple input files not supported\n");
                 return 1;
            }
            input_file = argv[i];
        }
    }

    if (!input_file) {
        fprintf(stderr, "Error: No input file specified\n");
        return 1;
    }

    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "bfpp \"%s\"", input_file);

    if (flag_E) {
        int res = system(cmd);
        return res == 0 ? 0 : 1;
    }

    FILE *pp_in = popen(cmd, "r");
    if (!pp_in) {
        perror("popen failed");
        return 1;
    }

    int is_c_output = 0;
    size_t len = strlen(output_file);
    if (len > 2 && strcmp(output_file + len - 2, ".c") == 0) {
        is_c_output = 1;
    }

    if (is_c_output) {
        FILE *c_out = fopen(output_file, "w");
        if (!c_out) {
            perror("fopen failed");
            pclose(pp_in);
            return 1;
        }
        transpile(pp_in, c_out, flag_O);
        fclose(c_out);
        pclose(pp_in);
    } else {
        char tmp_c[] = "/tmp/bfc_temp_XXXXXX.c";
        int fd = mkstemp(tmp_c);
        if (fd == -1) {
            perror("mkstemp failed");
            pclose(pp_in);
            return 1;
        }
        FILE *c_out = fdopen(fd, "w");
        if (!c_out) {
            perror("fdopen failed");
            close(fd);
            unlink(tmp_c);
            pclose(pp_in);
            return 1;
        }
        transpile(pp_in, c_out, flag_O);
        fclose(c_out);
        pclose(pp_in);

        snprintf(cmd, sizeof(cmd), "cc %s \"%s\" -o \"%s\"", flag_O ? "-O2" : "", tmp_c, output_file);
        int res = system(cmd);
        unlink(tmp_c);
        if (res != 0) {
            fprintf(stderr, "Compilation failed\n");
            return 1;
        }
    }

    return 0;
}
