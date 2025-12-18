/*
 * bftt – translates text to brainfuck code
 * calling convention:
 * #0 and #1 will be saved
 * be sure #100, #101 are 0
 */

/*
 * you can include it to brainfuck code
 * only #0, #1 used
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void fprint_repeated(FILE *stream, char c, int count) {
    if (count <= 0) return;
    for (int i = 0; i < count; i++) {
        fputc(c, stream);
    }
}

void generate_move_code(FILE *output, int from_offset, int to_offset) {
    int distance = to_offset - from_offset;
    char move_op = (distance > 0) ? '>' : '<';
    char return_op = (distance > 0) ? '<' : '>';
    int abs_dist = (distance > 0) ? distance : -distance;

    fputc('[', output);
    fputc('-', output);
    fprint_repeated(output, move_op, abs_dist);
    fputc('+', output);
    fprint_repeated(output, return_op, abs_dist);
    fputc(']', output);
}

void generate_clear(FILE *output) {
    fputc('[', output);
    fputc('-', output);
    fputc(']', output);
}

void generate_optimized_increment(FILE *output, int diff) {
    if (diff == 0) return;

    const int threshold = 12;
    char op = (diff > 0) ? '+' : '-';
    int abs_diff = (diff > 0) ? diff : -diff;

    if (abs_diff < threshold) {
        fprint_repeated(output, op, abs_diff);
        return;
    }

    int factor = (int)sqrt(abs_diff);
    int quotient = abs_diff / factor;
    int remainder = abs_diff % factor;

    fputc('>', output);
    generate_clear(output);
    fprint_repeated(output, '+', factor);
    
    fputc('[', output);
    fputc('<', output);
    fprint_repeated(output, op, quotient);
    fputc('>', output);
    fputc('-', output);
    fputc(']', output);
    
    fputc('<', output);
    fprint_repeated(output, op, remainder);
}

void convert_to_bf(FILE *input, FILE *output) {
    const int BACKUP_BASE = 100;

    generate_move_code(output, 0, BACKUP_BASE);
    
    fputc('>', output);
    generate_move_code(output, 1, BACKUP_BASE + 1);
    
    fputc('<', output);

    int c;
    int current_value = 0;
    while ((c = fgetc(input)) != EOF) {
        if (c == 0) continue;
        int diff = c - current_value;
        generate_optimized_increment(output, diff);
        fputc('.', output);
        current_value = c;
    }

    generate_clear(output);
    fputc('>', output);
    generate_clear(output);
    fputc('<', output);

    fprint_repeated(output, '>', BACKUP_BASE);
    
    generate_move_code(output, BACKUP_BASE, 0); 
    
    fputc('>', output);
    
    generate_move_code(output, BACKUP_BASE + 1, 1);
    
    fprint_repeated(output, '<', BACKUP_BASE + 1);
    
    fputc('\n', output);
}

int main(int argc, char *argv[]) {
    if (argc > 2) {
        fprintf(stderr, "Usage: %s [input_file.txt]\n", argv[0]);
        return 1;
    }

    FILE *input_stream = stdin;
    FILE *output_stream = stdout;
    char *output_filename = NULL;

    if (argc == 2) {
        const char *input_filename = argv[1];
        input_stream = fopen(input_filename, "r");
        if (!input_stream) {
            perror("Error opening input");
            return 1;
        }

        output_filename = malloc(strlen(input_filename) + 5);
        if (!output_filename) {
            perror("Memory error");
            fclose(input_stream);
            return 1;
        }
        strcpy(output_filename, input_filename);
        char *dot = strrchr(output_filename, '.');
        if (dot) *dot = '\0';
        strcat(output_filename, ".bf");

        output_stream = fopen(output_filename, "w");
        if (!output_stream) {
            perror("Error opening output");
            fclose(input_stream);
            free(output_filename);
            return 1;
        }
        printf("Input: %s -> Output: %s\n", input_filename, output_filename);
    }

    convert_to_bf(input_stream, output_stream);

    if (argc == 2) {
        fclose(input_stream);
        fclose(output_stream);
        free(output_filename);
    }

    return 0;
}
