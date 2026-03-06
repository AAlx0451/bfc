#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TAPE_CONST_VAL 65536

typedef enum {
    OP_ADD, OP_MOVE, OP_OUT, OP_IN, OP_JZ, OP_JNZ,
    OP_CLEAR, OP_MUL,
    OP_EXT_PTR_MAX, OP_EXT_PTR_ZERO,
    OP_EXT_PUSH_V, OP_EXT_POP_V,
    OP_EXT_PUSH_P, OP_EXT_POP_P,
    OP_EXT_CLR_END, OP_EXT_CLR_BEGIN
} OpType;

typedef struct {
    OpType type;
    int val;
    int val2;
} Instruction;

Instruction *ir = NULL;
int ir_cap = 0;
int ir_len = 0;

void emit(OpType type, int val, int val2) {
    if (ir_len >= ir_cap) {
        ir_cap = ir_cap == 0 ? 1024 : ir_cap * 2;
        ir = realloc(ir, ir_cap * sizeof(Instruction));
    }
    ir[ir_len++] = (Instruction){type, val, val2};
}

void emit_rle(OpType type, int val) {
    if (ir_len > 0 && ir[ir_len - 1].type == type) {
        ir[ir_len - 1].val += val;
        if (ir[ir_len - 1].val == 0) ir_len--;
    } else {
        emit(type, val, 0);
    }
}

void parse_to_ir(FILE *in) {
    int c;
    while ((c = fgetc(in)) != EOF) {
        if (c == '/') {
            int next = fgetc(in);
            if (next == '*') {
                while (1) {
                    int nc = fgetc(in);
                    if (nc == EOF) break;
                    if (nc == '*') {
                        int nnc = fgetc(in);
                        if (nnc == '/') break;
                        ungetc(nnc, in);
                    }
                }
                continue;
            } else ungetc(next, in);
        }
        if (c == '_') {
            int next = fgetc(in);
            if (next == '>') emit(OP_EXT_PTR_MAX, 0, 0);
            else if (next == '<') emit(OP_EXT_PTR_ZERO, 0, 0);
            else if (next == '^') emit(OP_EXT_PUSH_V, 0, 0);
            else if (next == '&') emit(OP_EXT_POP_V, 0, 0);
            else if (next == '#') emit(OP_EXT_PUSH_P, 0, 0);
            else if (next == '$') emit(OP_EXT_POP_P, 0, 0);
            else if (next == '?') {
                int third = fgetc(in);
                if (third == '>') emit(OP_EXT_CLR_END, 0, 0);
                else if (third == '<') emit(OP_EXT_CLR_BEGIN, 0, 0);
                else { ungetc(third, in); ungetc(next, in); }
            } else ungetc(next, in);
        }
        else if (c == '+') emit_rle(OP_ADD, 1);
        else if (c == '-') emit_rle(OP_ADD, -1);
        else if (c == '>') emit_rle(OP_MOVE, 1);
        else if (c == '<') emit_rle(OP_MOVE, -1);
        else if (c == '.') emit(OP_OUT, 0, 0);
        else if (c == ',') emit(OP_IN, 0, 0);
        else if (c == '[') emit(OP_JZ, 0, 0);
        else if (c == ']') emit(OP_JNZ, 0, 0);
    }
}

void optimize_ir() {
    Instruction *new_ir = malloc(ir_cap * sizeof(Instruction));
    int new_len = 0;
    for (int i = 0; i < ir_len; i++) {
        if (ir[i].type == OP_JZ) {
            int j = i + 1, net_move = 0, valid = 1, changes[1024] = {0};
            while (j < ir_len && ir[j].type != OP_JNZ) {
                if (ir[j].type == OP_MOVE) net_move += ir[j].val;
                else if (ir[j].type == OP_ADD) {
                    if (net_move >= -512 && net_move <= 511) changes[net_move + 512] += ir[j].val;
                    else { valid = 0; break; }
                } else { valid = 0; break; }
                j++;
            }
            if (j < ir_len && valid && net_move == 0) {
                int is_only_self = 1;
                for (int k = 0; k < 1024; k++) if (k != 512 && changes[k] != 0) is_only_self = 0;
                
                if (is_only_self && (changes[512] == -1 || changes[512] == 1)) {
                    new_ir[new_len++] = (Instruction){OP_CLEAR, 0, 0};
                    i = j; continue;
                } else if (changes[512] == -1) {
                    for (int k = 0; k < 1024; k++) {
                        if (k != 512 && changes[k] != 0) {
                            new_ir[new_len++] = (Instruction){OP_MUL, k - 512, changes[k]};
                        }
                    }
                    new_ir[new_len++] = (Instruction){OP_CLEAR, 0, 0};
                    i = j; continue;
                }
            }
        }
        new_ir[new_len++] = ir[i];
    }
    free(ir); 
    ir = new_ir; 
    ir_len = new_len;
}

void generate_c(FILE *out, int optimize) {
    if (optimize) fprintf(out, "#pragma GCC optimize(\"O3,unroll-loops\")\n");
    fprintf(out, "#include <stdio.h>\n#include <stdint.h>\n\n");
    fprintf(out, "#define TAPE %d\nuint8_t tape[TAPE] = {0};\nuint8_t vstack[TAPE];\nuint32_t pstack[TAPE];\nuint32_t ptr = 0, vsp = 0, psp = 0;\n\n", TAPE_CONST_VAL);
    fprintf(out, "int main(void) {\n");
    
    for (int i = 0; i < ir_len; i++) {
        Instruction inst = ir[i];
        switch (inst.type) {
            case OP_ADD: fprintf(out, "    tape[ptr] %c= %d;\n", inst.val > 0 ? '+' : '-', abs(inst.val)); break;
            case OP_MOVE: fprintf(out, "    ptr %c= %d;\n", inst.val > 0 ? '+' : '-', abs(inst.val)); break;
            case OP_OUT: fprintf(out, "    putchar(tape[ptr]);\n"); break;
            case OP_IN:  fprintf(out, "    tape[ptr] = getchar();\n"); break;
            case OP_JZ:  fprintf(out, "    while(tape[ptr]) {\n"); break;
            case OP_JNZ: fprintf(out, "    }\n"); break;
            case OP_CLEAR: fprintf(out, "    tape[ptr] = 0;\n"); break;
            case OP_MUL: 
                if (abs(inst.val2) == 1) {
                    fprintf(out, "    tape[ptr %c %d] %c= tape[ptr];\n", inst.val > 0 ? '+' : '-', abs(inst.val), inst.val2 > 0 ? '+' : '-');
                } else {
                    fprintf(out, "    tape[ptr %c %d] %c= tape[ptr] * %d;\n", inst.val > 0 ? '+' : '-', abs(inst.val), inst.val2 > 0 ? '+' : '-', abs(inst.val2));
                }
                break;
            case OP_EXT_PTR_MAX: fprintf(out, "    ptr = TAPE - 1;\n"); break;
            case OP_EXT_PTR_ZERO: fprintf(out, "    ptr = 0;\n"); break;
            case OP_EXT_PUSH_V: fprintf(out, "    if (vsp < TAPE) vstack[vsp++] = tape[ptr];\n"); break;
            case OP_EXT_POP_V: fprintf(out, "    if (vsp > 0) tape[ptr] = vstack[--vsp];\n"); break;
            case OP_EXT_PUSH_P: fprintf(out, "    if (psp < TAPE) pstack[psp++] = ptr;\n"); break;
            case OP_EXT_POP_P: fprintf(out, "    if (psp > 0) ptr = pstack[--psp];\n"); break;
            case OP_EXT_CLR_END: fprintf(out, "    if (ptr == TAPE - 1) tape[ptr] = 0;\n"); break;
            case OP_EXT_CLR_BEGIN: fprintf(out, "    if (ptr == 0) tape[ptr] = 0;\n"); break;
        }
    }
    fprintf(out, "    return 0;\n}\n");
}

int main(int argc, char **argv) {
    int flag_O = 0, flag_E = 0;
    char *input_file = NULL, *output_file = "a.out";
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-O") == 0) flag_O = 1;
        else if (strcmp(argv[i], "-E") == 0) flag_E = 1;
        else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) output_file = argv[++i];
        else input_file = argv[i];
    }
    
    if (!input_file) return 1;
    
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "bfpp \"%s\"", input_file);
    if (flag_E) return system(cmd) == 0 ? 0 : 1;
    
    FILE *pp_in = popen(cmd, "r");
    parse_to_ir(pp_in); 
    pclose(pp_in);
    
    if (flag_O) optimize_ir();

    int is_c_output = 0;
    size_t out_len = strlen(output_file);
    if (out_len > 2 && strcmp(output_file + out_len - 2, ".c") == 0) {
        is_c_output = 1;
    }

    if (is_c_output) {
        FILE *c_out = fopen(output_file, "w");
        generate_c(c_out, flag_O);
        fclose(c_out);
    } else {
        char tmp_c[] = "/tmp/bfc_temp_XXXXXX.c";
        int fd = mkstemp(tmp_c);
        FILE *c_out = fdopen(fd, "w");
        generate_c(c_out, flag_O); 
        fclose(c_out);
        
        snprintf(cmd, sizeof(cmd), "cc %s \"%s\" -o \"%s\"", flag_O ? "-O3" : "", tmp_c, output_file);
        int res = system(cmd);
        
        unlink(tmp_c);
        free(ir);
        return res == 0 ? 0 : 1;
    }

    free(ir);
    return 0;
}
