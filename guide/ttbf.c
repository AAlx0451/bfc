#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void generate_clear(FILE *output) {
    fprintf(output, "CYCLE\n");
    fprintf(output, "    DECR\n");
    fprintf(output, "CYCL\n");
}

void generate_increment(FILE *output, int diff) {
    if (diff == 0) return;

    const int threshold = 12;
    const char* op_str = (diff > 0) ? "INCR" : "DECR";
    int abs_diff = (diff > 0) ? diff : -diff;

    if (abs_diff < threshold) {
        fprintf(output, "{x%d{%s}}\n", abs_diff, op_str);
        return;
    }

    int factor = (int)sqrt(abs_diff);
    if (factor < 2) factor = 2; 
    
    int quotient = abs_diff / factor;
    int remainder = abs_diff % factor;

    fprintf(output, "NEXT\n");
    generate_clear(output);
    fprintf(output, "{x%d{INCR}}\n", factor);
    
    fprintf(output, "CYCLE\n");
    fprintf(output, "    PREV\n");
    fprintf(output, "    {x%d{%s}}\n", quotient, op_str);
    fprintf(output, "    NEXT\n");
    fprintf(output, "    DECR\n");
    fprintf(output, "CYCL\n");
    
    fprintf(output, "PREV\n");
    if (remainder > 0) {
        fprintf(output, "{x%d{%s}}\n", remainder, op_str);
    }
}

void convert_to_bf(FILE *input, FILE *output) {
    const int BACKUP_BASE = 100;

    fprintf(output, "{LOAD ./f.bfh}\n\n");
    
    fprintf(output, "PROLOG\n");
    fprintf(output, "NEXT\n");
    fprintf(output, "PROLOG\n");
    fprintf(output, "PREV\n\n");

    int c;
    int current_value = 0;
    while ((c = fgetc(input)) != EOF) {
        int diff = c - current_value;
        generate_increment(output, diff);
        fprintf(output, "PRINT\n");
        current_value = c;
    }

    fprintf(output, "\nEPINEXT\n");
    fprintf(output, "{x%d{NEXT}}\n", BACKUP_BASE);
    fprintf(output, "EPILOG\n");
    fprintf(output, "NEXT\n");
    fprintf(output, "EPILOG\n");
    fprintf(output, "ENDF\n");
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
