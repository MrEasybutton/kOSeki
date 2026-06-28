#ifndef BAUX2_H
#define BAUX2_H

#include "../../include/types.h"
#include "../../include/string.h"
#include "../../include/graphics.h"

#ifndef bool
#define bool uint8
#define true 1
#define false 0
#endif

typedef enum {
  // 1
  TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
  TOKEN_COMMA, TOKEN_DOT, TOKEN_SEMICOLON,
  TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH,
  // 1/2 char
  TOKEN_BANG, TOKEN_BANG_EQUAL,
  TOKEN_EQUAL, TOKEN_EQUAL_EQUAL,
  TOKEN_PLUS_EQUAL, TOKEN_MINUS_EQUAL,
  TOKEN_LESS, TOKEN_LESS_EQUAL,
  TOKEN_GREATER, TOKEN_GREATER_EQUAL,

  TOKEN_IDENT, TOKEN_STRING, TOKEN_NUMBER,

  TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_FOR,
  TOKEN_VAR, TOKEN_FUN, TOKEN_RETURN, TOKEN_PRINT,
  TOKEN_TRUE, TOKEN_FALSE, TOKEN_NIL,
  TOKEN_PANIC, TOKEN_BOOL_TYPE, TOKEN_FOR_END,
  TOKEN_CLASS, TOKEN_INSTANCE,
  TOKEN_IMPORT,

  TOKEN_BAU, TOKEN_PONDE, TOKEN_RING,
  TOKEN_FLUFFY, TOKEN_FUZZY, TOKEN_FUWA,
  TOKEN_MOCO, TOKEN_CHIHUAHUA, TOKEN_PERO,

  TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
  TokenType type;
  const char *start;
  int length;
  int line;
} Token;

typedef struct Expr Expr;
typedef struct Stmt Stmt;
typedef struct Value Value;
typedef struct Env Env;

typedef Value (*NativeFn)(int arg_count, Value *args);

#include "../../include/pon.h"

typedef enum { DRAW_RECT, DRAW_TEXT } DrawCmdType;

typedef struct {
    DrawCmdType type;
    int x, y, w, h;
    uint32 color;
    char* text;
    int font;
} DrawCmd;

#define MAX_DRAW_CMDS 256

typedef struct {
    DrawCmd cmds[MAX_DRAW_CMDS];
    int count;
    PON_Comp* root_comp;
    Stmt** statements;
    int stmt_count;
    Env* env;
    char* source;
} BAUx2_d;

typedef enum {
  EXPR_BINARY, EXPR_UNARY, EXPR_LITERAL, EXPR_VAR,
  EXPR_ASSIGN, EXPR_CALL, EXPR_LOGICAL,
} ExprType;

struct Expr {
  ExprType type;
  int line;
  union {
    struct { Expr *left; Token op; Expr *right; } binary;
    struct { Token op; Expr *right; }             unary;
    struct { Token name; }                         var;
    struct { Token name; Expr *value; }            assign;
    struct { Expr *callee; Expr **args; int argc; } call;
    struct { Expr *left; Token op; Expr *right; } logical;
    struct {
      double number;
      bool boolean;
      bool is_nil;
      char *string;
      enum { LIT_NUMBER, LIT_BOOL, LIT_STRING, LIT_NIL } lit_type;
    } literal;
  };
};

typedef enum {
  STMT_EXPR, STMT_PRINT, STMT_VAR, STMT_BLOCK,
  STMT_IF, STMT_WHILE, STMT_FUNCTION, STMT_RETURN,
  STMT_PANIC, STMT_FOR_RANGE, STMT_IMPORT
} StmtType;

struct Stmt {
  StmtType type;
  union {
    struct { Expr *expression; } expr;
    struct { Expr *expression; } print;
    struct { Token name; Expr *initializer; } var;
    struct { Stmt **statements; int count; } block;
    struct { Expr *condition; Stmt *then_branch; Stmt *else_branch; } if_stmt;
    struct { Expr *condition; Stmt *body; } while_stmt;
    struct { Token name; Token *params; int param_count; Stmt **body; int body_count; } function;
    struct { Token keyword; Expr *value; } return_stmt;
    struct { Expr *message; } panic_stmt;
    struct { Expr *count_expr; Stmt *body; } for_range;
    struct { Token name; } import_stmt;
  };
};

typedef enum { VAL_NIL, VAL_BOOL, VAL_NUMBER, VAL_STRING, VAL_FUNCTION, VAL_NATIVE_FN } ValueType;

typedef struct Function Function;

struct Value {
  ValueType type;
  union {
    bool boolean;
    double number;
    char *string;
    Function *fn;
    NativeFn native;
  };
};

typedef struct Env Env;

typedef struct {
  char *name;
  Value value;
} EnvEntry;

struct Env {
  Env *enclosing;
  EnvEntry *entries;
  int count;
  int capacity;
  bool is_returning;
  Value return_value;
};

struct Function {
  Token name;
  Token *params;
  int param_count;
  Stmt **body;
  int body_count;
  Env *closure;
};

struct Window;
typedef struct Window Window;
struct Process;
typedef struct Process Process;

void baux2_run(const char *source);
void baux2_cleanup(Window* win);
void baux2_cleanup_process(Process* p);

void scanner_init(const char *source);
Token scan_token();

Stmt **parse(const char *source, int *count);

typedef void (*baux2_print_t)(const char *text);
extern baux2_print_t baux2_print_handler;
extern int g_baux2_curr_pid;

Env *interpret(Stmt **statements, int count);
Value eval_expr(Expr *expr, Env *env);
void execute(Stmt *stmt, Env *env);

Value copy_value(Value value);
void free_value(Value value);

Env *new_env(Env *enclosing);
void env_define(Env *env, const char *name, Value value);
bool env_get(Env *env, Token name, Value *value);
bool env_assign(Env *env, Token name, Value value);
void free_env(Env *env);

void free_expr(Expr *expr);
void free_stmt(Stmt *stmt);

#endif