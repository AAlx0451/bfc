## bfc
Brainfuck transpiler with preprocessor

## Usage
1. Preprocess only: `bfc -E myfile.bf` or `bfpp myfile.bf`
2. Translate to C only: `bfc myfile.bf -o myfile.c`
3. Compile to binary: `bfc myfile.bf` or `bfc myfile.bf -o myfile`
4. Compile to binary (optimized): `bfc -O myfile.bf -o myfile`

## Guide
See `./guide`
