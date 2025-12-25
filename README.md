## Welcome to BFC!

Brainfuck transpiler with preprocessor

## Usage

1. Preprocess only: `bfc -E myfile.bf` or `bfpp myfile.bf`
2. Translate to C only: `bfc myfile.bf -o myfile.c`
3. Compile to binary: `bfc myfile.bf` or `bfc myfile.bf -o myfile`
4. Compile to binary (optimized): `bfc -O myfile.bf -o myfile`

## Structure

* ./src – BFpp and BFC source
* ./guide – WIP guide
* ./inc – Headers
    * ./inc/bfc – BFC-specific headers
    * ./inc/std – simple definitions
    * ./inc/stack – portable stack implementation
    * ./inc/stack/bfc – BFC-specific stack implementation

## BFpp syntax

* {LOAD filename} – load filename from current dir or BFPP env variable
* {LOAD path/to/filename} – load filename from $BFPP/path/to/filename or from $(pwd)/path/to/filename
* {D Name Body} or {DEF Macro Body} – define a macro
* {U Name} – undef a macro 

## Portability

I tried. This preprocessor is very compatible, but bfc-specific headers are not. Portable stack is implemented, goto is not :)

Actually, BFC is very simple and calls preprocessor – but it implements a return stack for goto.

## Code style

See ./inc/stack/stack.bfh :)

## WHY?

It's VERY readable.

```
{LOAD bfpp.bfh}
{D NEWLINE {x10{INCR}}}
{D ASTERISK {x42{INCR}}}
{D REMOVE TONULL}


REMOVE VALUE FROM CELL THEN
ADD NEWLINE VALUE TO CELL THEN
PUSH CURRENT VALUE TO STACK THEN
WE NEED TO REMOVE NEWLINE SO LETS REMOVE IT
ADD ASTERISK VALUE TO CELL THEN
PUSH CURRENT VALUE TO STACK THEN
REMOVE IT AGAIN THEN
POP CURRENT VALUE FROM STACK AND PRINT IT AND REMOVE THE VALUE
AGAIN: POP CURRENT VALUE FROM STACK AND PRINT IT AND REMOVE THE VALUE
```
