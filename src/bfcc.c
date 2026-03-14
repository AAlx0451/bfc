#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
                else { ungetc(third, stdin); ungetc(next, stdin); }
            } else ungetc(next, stdin);
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

void generate_c(int optimize) {
    if (optimize) printf("#pragma GCC optimize(\"O3,unroll-loops\")\n");
    printf("#include <stdio.h>\n#include <stdint.h>\n#include <stdlib.h>\n\n");
    printf("#define TAPE %d\nuint8_t tape[TAPE] = {0};\nuint8_t vstack[TAPE];\nuint32_t pstack[TAPE];\nuint32_t ptr = 0, vsp = 0, psp = 0;\n\n", TAPE_CONST_VAL);
    printf("int main(void) {\n");

    for (int i = 0; i < ir_len; i++) {
        Instruction inst = ir[i];
        switch (inst.type) {
            case OP_ADD: printf("    tape[ptr] %c= %d;\n", inst.val > 0 ? '+' : '-', abs(inst.val)); break;
            case OP_MOVE: printf("    ptr %c= %d;\n", inst.val > 0 ? '+' : '-', abs(inst.val)); break;
            case OP_OUT: printf("    putchar(tape[ptr]);\n"); break;
            case OP_IN:  printf("    tape[ptr] = getchar();\n"); break;
            case OP_JZ:  printf("    while(tape[ptr]) {\n"); break;
            case OP_JNZ: printf("    }\n"); break;
            case OP_CLEAR: printf("    tape[ptr] = 0;\n"); break;
            case OP_MUL: 
                if (abs(inst.val2) == 1) {
                    printf("    tape[ptr %c %d] %c= tape[ptr];\n", inst.val > 0 ? '+' : '-', abs(inst.val), inst.val2 > 0 ? '+' : '-');
                } else {
                    printf("    tape[ptr %c %d] %c= tape[ptr] * %d;\n", inst.val > 0 ? '+' : '-', abs(inst.val), inst.val2 > 0 ? '+' : '-', abs(inst.val2));
                }
                break;
            case OP_EXT_PTR_MAX: printf("    ptr = TAPE - 1;\n"); break;
            case OP_EXT_PTR_ZERO: printf("    ptr = 0;\n"); break;
            case OP_EXT_PUSH_V: printf("    if (vsp < TAPE) vstack[vsp++] = tape[ptr];\n"); break;
            case OP_EXT_POP_V: printf("    if (vsp > 0) tape[ptr] = vstack[--vsp];\n"); break;
            case OP_EXT_PUSH_P: printf("    if (psp < TAPE) pstack[psp++] = ptr;\n"); break;
            case OP_EXT_POP_P: printf("    if (psp > 0) ptr = pstack[--psp];\n"); break;
            case OP_EXT_CLR_END: printf("    if (ptr == TAPE - 1) tape[ptr] = 0;\n"); break;
            case OP_EXT_CLR_BEGIN: printf("    if (ptr == 0) tape[ptr] = 0;\n"); break;
        }
    }
    printf("    return 0;\n}\n");
}

int main(int argc, char **argv) {
    int flag_O = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-O") == 0) flag_O = 1;
    }

    parse_to_ir(stdin);
    if (flag_O) optimize_ir();
    generate_c(flag_O);

    free(ir);
    return 0;
}
