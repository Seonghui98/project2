#include <stdio.h> // 표준 입출력
#include <stdlib.h> // 동적 메모리, 종료 코드
#include <string.h> // 문자열 처리 함수
#include <ctype.h> // 문자 판별(isspace 등)

// 상수 정의
#define MAX_LINES    5000 // 최대 소스 라인 수 5000으로 지정
#define MAX_INST     256 // 최대 명령어 엔트리 수 256으로 지정
#define MAX_OPERAND  3 // 최대 피연산자 개수 3개 지정

// 입력 관리 구조

char *input_data[MAX_LINES]; // 원본 한 줄 문자열 보관
static int line_num = 0; // 읽힌 라인 총 개수

struct token_unit { // 한 줄을 토큰으로 나눈 결과 구조
    char *label; // 라벨
    char *operator; // 연산자(명령어/지시자)
    char  operand[MAX_OPERAND][20]; // 피연산자 문자열들
    char  comment[100];
};
typedef struct token_unit token; // token 별칭

token *token_table[MAX_LINES]; // 각 라인을 가리키는 토큰 포인터 테이블

// Instruction 구조
struct inst_unit{ // inst.data의 한 레코드를 담는 구조
    char str[10];
    unsigned char op; // 1바이트 OPCODE
    int format; // 명령 형식
    int ops; // 피연산자 개수
}; // struct inst_unit 끝
typedef struct inst_unit inst;

inst *inst_table[MAX_INST]; // 명령어 테이블(포인터 배열)
int inst_index = 0; // 현재 테이블에 적재된 개수

//  유틸 함수
static char* strdup_safe(const char *s) {
    if (!s) return NULL; // NULL 입력 방지
    size_t n = strlen(s) + 1; // 복사할 길이 계산
    char *p = (char*)malloc(n); // 버퍼 할당
    if (p) memcpy(p, s, n); // 내용 복사
    return p; // 새로운 버퍼로 반환
}

static void rstrip(char *s) { // 줄 끝의 개행/CR 제거
    int i = (int)strlen(s) - 1; // 마지막 인덱스
    while (i >= 0 && (s[i]=='\n' || s[i]=='\r')) s[i--] = '\0'; // 개행 문자 제거
}

static void trim(char *s) { // 앞뒤 공백 제거
    int i = 0, j = (int)strlen(s) - 1; // 좌/우 포인터
    while (i <= j && isspace((unsigned char)s[i])) i++; // 선행 공백 스킵
    while (j >= i && isspace((unsigned char)s[j])) j--; // 후행 공백 스킵
    memmove(s, s + i, j - i + 1); // 중앙 구간을 앞으로 당김
    s[j - i + 1] = '\0';
}

// CLASS 값으로 operand 개수 유추
static int infer_ops_from_class(const char *klass) { // CLASS→ops 맵핑
    if (!klass) return 1; // 기본 1개로 가정
    if (!strcmp(klass, "RR")) return 2; // 레지스터-레지스터
    if (!strcmp(klass, "RN")) return 2; // 레지스터-상수
    if (!strcmp(klass, "R"))  return 1; // 레지스터 1개
    if (!strcmp(klass, "M"))  return 1; // 메모리/심볼 1개
    if (!strcmp(klass, "N"))  return 0; // X
    if (!strcmp(klass, "-"))  return 0; // X
    return 1; // 그 외는 1개로 처리
}

// 파일 로더, input.txt 라인 전체 적재
static int load_input_file(const char *path) { // 입력 파일(input.txt) 읽기
    FILE *fp = fopen(path, "r"); // 텍스트 모드로 열기
    if (!fp) return -1; // 실패 시 에러 코드
    char buf[512]; // 한 줄 버퍼
    while (line_num < MAX_LINES && fgets(buf, sizeof(buf), fp)) { // EOF까지
        input_data[line_num] = strdup_safe(buf); // 라인 복사 저장
        line_num++; // 라인 수 증가
    }
    fclose(fp); // 파일 닫기
    return line_num; // 읽은 라인 수 반환
}

// inst.data 로드
static int load_inst_data(const char *path) { // 명령어 테이블 로드
    FILE *fp = fopen(path, "r"); // 파일 열기
    if (!fp) return -1; // 실패 시 에러 코드

    char line[256]; // 입력 버퍼
    while (inst_index < MAX_INST && fgets(line, sizeof(line), fp)) {
        rstrip(line); // 개행 제거
        if (line[0] == '\0' || line[0] == '.') continue; // 빈줄,주석 스킵

        char name[16], klass[8]; // 임시 파싱 버퍼
        int fmt = 0;
        unsigned int hex = 0xFF; // 16진수 OPCODE 임시


        if (sscanf(line, "%15s %7s %d %x", name, klass, &fmt, &hex) == 4) { // 필드 파싱
            inst *it = (inst*)malloc(sizeof(inst)); // 엔트리 할당
            if (!it) break; // 메모리 부족 보호
            memset(it, 0, sizeof(*it)); // 초기화
            strncpy(it->str, name, sizeof(it->str)-1); // 니모닉 복사
            it->op     = (unsigned char)(hex & 0xFF); // 1바이트로 저장
            it->format = fmt; // 형식 기록
            it->ops    = infer_ops_from_class(klass); // ops 추정
            inst_table[inst_index++] = it; // 테이블에 등록
        } // 조건문 종료
    } // while 종료
    fclose(fp); // 파일 닫기
    return inst_index; // 적재된 엔트리 수 반환
}

static inst* find_inst(const char *mnemonic) { // 니모닉으로 inst 검색
    if (!mnemonic) return NULL; // 방어 코드
    for (int i = 0; i < inst_index; ++i) { // 선형 탐색
        if (strcmp(inst_table[i]->str, mnemonic) == 0) return inst_table[i]; // 일치 시 반환
    } // for문 종료
    return NULL; // 없으면 NULL
} // find_inst 끝

// 파싱(토큰화)
static void tokenize_line(char *line, token *tk) { // 한 줄을 token 구조로 파싱
    memset(tk, 0, sizeof(*tk)); // 구조체 초기화

    char tmp[512]; // 작업 버퍼
    strncpy(tmp, line, sizeof(tmp)-1); // 원본 복사
    tmp[sizeof(tmp)-1] = '\0'; // 널 보장
    rstrip(tmp); // 줄 끝 정리

    if (tmp[0] == '.' || tmp[0] == '\0') { // 주석, 빈 줄
        tk->operator = NULL; // 출력 스킵을 위해 operator 비움
        return; // 조기 종료
    } // if문 종료

    int has_label = !(tmp[0] == ' ' || tmp[0] == '\t'); // 선행 공백 없으면 라벨 존재로 가정

    char *p = tmp; // 토큰화 시작 포인터
    char *tok1 = strtok(p, " \t"); // 첫 토큰
    char *tok2 = strtok(NULL, " \t"); // 두 번째 토큰
    char *rest = strtok(NULL, "\n"); // 나머지(피연산자 문자열)

    if (has_label) { // 라벨이 있는 형태
        tk->label    = tok1 ? strdup_safe(tok1) : NULL; // 라벨 설정
        tk->operator = tok2 ? strdup_safe(tok2) : NULL; // 연산자 설정
    } else { // 라벨이 없는 형태
        tk->label    = NULL; // 라벨 없음
        tk->operator = tok1 ? strdup_safe(tok1) : NULL; // 연산자 설정
        rest         = tok2 ? tok2 : rest; // 피연산자 시작 위치 보정
    } // if문 종료

    if (rest) { // 피연산자 구간이 존재할 때
        char ops[256]; // 임시 버퍼
        strncpy(ops, rest, sizeof(ops)-1); // 복사
        ops[sizeof(ops)-1] = '\0'; // 널 보장
        trim(ops); // 공백 정리
        int k = 0; // 오퍼랜드 인덱스
        char *q = strtok(ops, ","); // 쉼표 기준 분리
        while (q && k < MAX_OPERAND) { // 최대 3개까지
            trim(q); // 개별 토큰 트리밍
            strncpy(tk->operand[k], q, sizeof(tk->operand[k])-1); // 저장
            tk->operand[k][sizeof(tk->operand[k])-1] = '\0'; // 널 종료
            k++; // 다음 위치
            q = strtok(NULL, ","); // 다음 토큰
        } // while문 종료
    } // if문 종료
}

static int tokenize_all(void) { // 모든 라인에 대해 토큰화 수행
    for (int i = 0; i < line_num; ++i) {
        token *t = (token*)malloc(sizeof(token)); // 토큰 구조 할당
        if (!t) return -1; // 메모리 부족 시 실패
        tokenize_line(input_data[i], t); // 한 줄 파싱
        token_table[i] = t; // 테이블에 등록
    } // for문 종료
    return 0;
}


static int is_directive(const char *op) { // 지시자 여부 확인
    if (!op) return 0; // NULL 방어
    return (!strcmp(op, "START") || !strcmp(op, "END")  || // 시작, 끝
            !strcmp(op, "BYTE")  || !strcmp(op, "WORD") || // 데이터 리터럴
            !strcmp(op, "RESB")  || !strcmp(op, "RESW") || // 메모리 예약
            !strcmp(op, "EQU")   || !strcmp(op, "LTORG")|| // 어셈블러 지시
            !strcmp(op, "CSECT") || !strcmp(op, "EXTDEF") || // 모듈 관련
            !strcmp(op, "EXTREF")); // 외부 참조
}

// 요구 형식대로 출력
static void print_result(FILE *out) { // 결과 표를 출력
    if (!out) out = stdout; // 기본 스트림 설정
    fprintf(out, "LINE | LABEL       | OPCODE   | OPERAND(S)            | HEXCODE\n"); // 헤더
    fprintf(out, "-----------------------------------------------------------------\n"); // 구분선

    for (int i = 0; i < line_num; ++i) { // 모든 라인 순회
        token *t = token_table[i]; // 현재 라인의 토큰
        if (!t || !t->operator) continue; // 주석, 빈 줄은 스킵하기

        inst *info = find_inst(t->operator); // 연산자에 해당하는 inst 검색
        int dir = is_directive(t->operator); // 지시자 여부 판정

        char ops_join[128] = "-"; // 피연산자 표기 기본값
        if (t->operand[0][0]) { // 첫 피연산자가 존재하면
            snprintf(ops_join, sizeof(ops_join), "%s", t->operand[0]); // 첫 항 복사
            for (int k = 1; k < MAX_OPERAND; ++k) { // 나머지 항 결합
                if (t->operand[k][0]) { // 비어있지 않으면
                    strncat(ops_join, ",", sizeof(ops_join)-strlen(ops_join)-1); // 콤마 추가
                    strncat(ops_join, t->operand[k], sizeof(ops_join)-strlen(ops_join)-1); // 항 추가
                } // if문 종료
            } // for문 종료
        }

        fprintf(out, "%-4d | %-11s | %-8s | %-21s | ", // 표 본문 출력
                i + 1, // 1부터 시작하는 행 번호
                t->label ? t->label : "-", // 라벨이 없으면 '-' 출력
                t->operator, // 연산자
                ops_join); // 결합된 오퍼랜드 문자열

        if (dir || !info) fprintf(out, "-\n"); // 지시자/미등록: HEXCODE 생략
        else              fprintf(out, "%02X\n", info->op & 0xFF); // 1바이트 OPCODE 출력
    } // for문 종료
}

// 메인 함수, 같은 폴더에 input.txt, inst,data 파일을 배치한다
int main(void) {
    if (load_input_file("input.txt") < 0) { // 입력 파일 로드
        fprintf(stderr, "input.txt 파일을 열 수 없습니다.\n"); // 입력 파일 없으면 오류 메시지
        return 1; // 비정상 종료 코드
    } // if문 종료
    if (tokenize_all() < 0) { // 전체 토큰화 수행
        fprintf(stderr, "토큰화 중 메모리 부족 오류가 발생했습니다.\n"); // 토큰화 안되면 오류 메시지
        return 1; // 비정상 종료 코드
    } // if문 종료
    if (load_inst_data("inst.data") < 0) { // 명령어 테이블 로드
        fprintf(stderr, "inst.data 파일을 열 수 없습니다.\n"); // 명렁어 테이블 파일 없으면 오류 메시지
        return 1; // 비정상 종료 코드
    } // if문 종료

    print_result(stdout); // 결과 표 출력
    return 0; // 정상 종료
}
