#ifndef MY_ASSEMBLER_H
#define MY_ASSEMBLER_H

#include <stdio.h>

#define MAX_LINES     5000
#define MAX_INST      256
#define MAX_OPERAND   3


extern char *input_data[MAX_LINES];
extern int line_num;

struct token_unit {
    char *label;
    char *operator;
    char operand[MAX_OPERAND][20];
    char comment[100];
};
typedef struct token_unit token;
extern token *token_table[MAX_LINES];

struct inst_unit {
    char str[10];
    unsigned char op;
    int format;
    int ops;
};
typedef struct inst_unit inst;

extern inst *inst_table[MAX_INST];
extern int inst_index;

int   load_input_file(const char *path);
int   tokenize_all_lines(void);
int   load_inst_table(const char *path);
inst* find_inst(const char *mnemonic);
int   print_parsed_with_opcode(FILE *out);

#endif

