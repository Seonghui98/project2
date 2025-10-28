#include "my_assembler.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>


char *input_data[MAX_LINES];
int line_num = 0;

token *token_table[MAX_LINES];

inst *inst_table[MAX_INST];
int inst_index = 0;


static char* strdup_safe(const char *s){
    if(!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char*)malloc(n);
    if(p) memcpy(p, s, n);
    return p;
}
static void rstrip(char *s){
    int i = (int)strlen(s) - 1;
    while(i >= 0 && (s[i]=='\n' || s[i]=='\r')) s[i--] = '\0';
}
static void trim(char *s){
    int i = 0, j = (int)strlen(s) - 1;
    while(i <= j && isspace((unsigned char)s[i])) i++;
    while(j >= i && isspace((unsigned char)s[j])) j--;
    memmove(s, s+i, j-i+1);
    s[j-i+1] = '\0';
}


int load_input_file(const char *path){
    FILE *fp = fopen(path, "r");
    if(!fp) return -1;
    char buf[512];
    while(line_num < MAX_LINES && fgets(buf, sizeof(buf), fp)){
        input_data[line_num] = strdup_safe(buf);
        line_num++;
    }
    fclose(fp);
    return line_num;
}


static void tokenize_line(char *line, token *tk){

    memset(tk, 0, sizeof(*tk));


    char tmp[512];
    strncpy(tmp, line, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';
    rstrip(tmp);
    if(tmp[0] == '.' || tmp[0] == '\0'){
        tk->operator = NULL; return;
    }


    int has_label = !(tmp[0]==' ' || tmp[0]=='\t');


    char *p = tmp;
    char *tok1 = strtok(p, " \t");
    char *tok2 = strtok(NULL, " \t");
    char *rest = strtok(NULL, "\n");

    if(has_label){
        tk->label    = tok1 ? strdup_safe(tok1) : NULL;
        tk->operator = tok2 ? strdup_safe(tok2) : NULL;
    }else{
        tk->label    = NULL;
        tk->operator = tok1 ? strdup_safe(tok1) : NULL;
        rest         = tok2 ? tok2 : rest;
    }

 
    if(rest){
        char ops[256];
        strncpy(ops, rest, sizeof(ops)-1);
        ops[sizeof(ops)-1] = '\0';
        trim(ops);
        int k=0;
        char *q = strtok(ops, ",");
        while(q && k < MAX_OPERAND){
            trim(q);
            strncpy(tk->operand[k], q, sizeof(tk->operand[k])-1);
            tk->operand[k][sizeof(tk->operand[k])-1] = '\0';
            k++;
            q = strtok(NULL, ",");
        }
    }
}

int tokenize_all_lines(void){
    for(int i=0;i<line_num;i++){
        token *t = (token*)malloc(sizeof(token));
        tokenize_line(input_data[i], t);
        token_table[i] = t;
    }
    return line_num;
}


int load_inst_table(const char *path){
    FILE *fp = fopen(path, "r");
    if(!fp) return -1;
    char line[256];
    while(inst_index < MAX_INST && fgets(line, sizeof(line), fp)){
        rstrip(line);
        if(line[0]=='\0' || line[0]=='.') continue;

        char name[16], klass[8];
        int fmt=0;
        unsigned int hex=0xFF;
        if(sscanf(line, "%15s %7s %d %x", name, klass, &fmt, &hex) == 4){
            inst *it = (inst*)malloc(sizeof(inst));
            memset(it, 0, sizeof(*it));
            strncpy(it->str, name, sizeof(it->str)-1);
            it->op     = (unsigned char)(hex & 0xFF);
            it->format = fmt;
            it->ops    = 1;
            inst_table[inst_index++] = it;
        }
    }
    fclose(fp);
    return inst_index;
}

inst* find_inst(const char *mnemonic){
    if(!mnemonic) return NULL;
    for(int i=0;i<inst_index;i++){
        if(strcmp(inst_table[i]->str, mnemonic)==0) return inst_table[i];
    }
    return NULL;
}


static int is_directive(const char *op){
    if(!op) return 0;
    return (!strcmp(op,"START")||!strcmp(op,"END")||
            !strcmp(op,"BYTE") ||!strcmp(op,"WORD")||
            !strcmp(op,"RESB") ||!strcmp(op,"RESW")||
            !strcmp(op,"EQU")  ||!strcmp(op,"LTORG")||
            !strcmp(op,"CSECT")||!strcmp(op,"EXTDEF")||!strcmp(op,"EXTREF"));
}

int print_parsed_with_opcode(FILE *out){
    if(!out) out = stdout;
    fprintf(out, "LINE | LABEL       | OPCODE   | OPERAND(S)            | HEXCODE\n");
    fprintf(out, "-----------------------------------------------------------------\n");

    int printed = 0;
    for(int i=0;i<line_num;i++){
        token *t = token_table[i];
        if(!t || !t->operator) continue;

        inst *info = find_inst(t->operator);
        int is_dir = is_directive(t->operator);


        char ops_join[128] = "-";
        char buf[128] = "";
        if(t->operand[0][0]){
            snprintf(buf, sizeof(buf), "%s", t->operand[0]);
            for(int k=1;k<MAX_OPERAND;k++){
                if(t->operand[k][0]){
                    strncat(buf, ",", sizeof(buf)-strlen(buf)-1);
                    strncat(buf, t->operand[k], sizeof(buf)-strlen(buf)-1);
                }
            }
            snprintf(ops_join, sizeof(ops_join), "%s", buf);
        }

        fprintf(out, "%-4d | %-11s | %-8s | %-21s | ",
                i+1,
                t->label ? t->label : "-",
                t->operator,
                ops_join);

        if(is_dir || !info){
            fprintf(out, "-\n");
        }else{
            fprintf(out, "%02X\n", info->op & 0xFF);
        }
        printed++;
    }
    return printed;
}