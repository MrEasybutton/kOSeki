#include "baux2.h"
#include "serial.h"
#include "../../include/kheap.h"
#include "../../include/console.h"

//idk what happened but the indentation is messed up
typedef struct {
  Token current;
  Token previous;
  bool had_error;
  bool panic_mode;
  int error_count;
} Parser;

Parser parser;

#define MAX_PARSE_ERRORS 67

static void error_at(Token *token, const char *message) {
  if (parser.panic_mode) return;
  parser.panic_mode = true;
  parser.error_count++;
  kprint("[line %d] Error", token->line);

  if (token->type == TOKEN_EOF) {
    kprint(" at end");
  } else if (token->type == TOKEN_ERROR) {
    // no
  } else {
    kprint(" at '%.*s'", token->length, token->start);
  }

  kprint(": %s\n", message);
  parser.had_error = true;
  
  if (parser.error_count >= MAX_PARSE_ERRORS) {
      kprint("rock down, retreat, retreat!\n");
  }
}

static void error(const char *message) {
  error_at(&parser.previous, message);
}

static void advance() {
  parser.previous = parser.current;

  for (;;) {
    parser.current = scan_token();
    if (parser.current.type != TOKEN_ERROR) break;
    error_at(&parser.current, parser.current.start);
  }
}

static void consume(TokenType type, const char *message) {
  if (parser.current.type == type) {
    advance();
    return;
  }
  error_at(&parser.current, message);
}

static bool check(TokenType type) {
  return parser.current.type == type;
}

static bool match(TokenType type) {
  if (!check(type)) return false;
  advance();
  return true;
}

Expr *new_expr(ExprType type) {
  Expr *expr = (Expr *)kmalloc(sizeof(Expr));
  memset(expr, 0, sizeof(Expr));
  expr->type = type;
  expr->line = parser.previous.line;
  return expr;
}

Stmt *new_stmt(StmtType type) {
  Stmt *stmt = (Stmt *)kmalloc(sizeof(Stmt));
  memset(stmt, 0, sizeof(Stmt));
  stmt->type = type;
  return stmt;
}

static Expr *expression();
static Stmt *statement();
static Stmt *declaration();

static Expr *primary() {
  if (match(TOKEN_FALSE) || match(TOKEN_FUZZY)) {
    Expr *e = new_expr(EXPR_LITERAL);
    e->literal.boolean = false;
    e->literal.lit_type = LIT_BOOL;
    return e;
  }
  if (match(TOKEN_TRUE) || match(TOKEN_FLUFFY)) {
    Expr *e = new_expr(EXPR_LITERAL);
    e->literal.boolean = true;
    e->literal.lit_type = LIT_BOOL;
    return e;
  }
  if (match(TOKEN_NIL)) {
    Expr *e = new_expr(EXPR_LITERAL);
    e->literal.lit_type = LIT_NIL;
    return e;
  }

  if (match(TOKEN_NUMBER)) {
    Expr *e = new_expr(EXPR_LITERAL);
    char buf[64];
    int len = parser.previous.length < 63 ? parser.previous.length : 63;
    memcpy(buf, parser.previous.start, len);
    buf[len] = '\0';
    e->literal.number = atof(buf);
    e->literal.lit_type = LIT_NUMBER;
    return e;
  }

  if (match(TOKEN_STRING)) {
    Expr *e = new_expr(EXPR_LITERAL);
    int len = parser.previous.length - 2;
    e->literal.string = (char *)kmalloc(len + 1);
    memcpy(e->literal.string, parser.previous.start + 1, len);
    e->literal.string[len] = '\0';
    e->literal.lit_type = LIT_STRING;
    return e;
  }

  if (match(TOKEN_IDENT)) {
    Expr *e = new_expr(EXPR_VAR);
    e->var.name = parser.previous;
    return e;
  }

  if (match(TOKEN_LPAREN)) {
    Expr *expr = expression();
    consume(TOKEN_RPAREN, "Expect ')' after expression.");
    return expr;
  }

  error("Expect expression.");
  return NULL;
}

static Expr *finish_call(Expr *callee) {
  Expr **args = NULL;
  int count = 0;

  if (!check(TOKEN_RPAREN)) {
    do {
      args = (Expr **)krealloc(args, sizeof(Expr *) * (count + 1));
      args[count++] = expression();
    } while (match(TOKEN_COMMA));
  }

  consume(TOKEN_RPAREN, "Expect ')' after arguments.");
  Expr *expr = new_expr(EXPR_CALL);
  expr->call.callee = callee;
  expr->call.args = args;
  expr->call.argc = count;
  return expr;
}

static Expr *call() {
  Expr *expr = primary();
  for (;;) {
    if (match(TOKEN_LPAREN)) {
      expr = finish_call(expr);
    } else {
      break;
    }
  }
  return expr;
}

static Expr *unary() {
  if (match(TOKEN_BANG) || match(TOKEN_MINUS)) {
    Token op = parser.previous;
    Expr *right = unary();
    Expr *e = new_expr(EXPR_UNARY);
    e->unary.op = op;
    e->unary.right = right;
    return e;
  }
  return call();
}

static Expr *factor() {
  Expr *expr = unary();
  while (match(TOKEN_STAR) || match(TOKEN_SLASH)) {
    Token op = parser.previous;
    Expr *right = unary();
    Expr *new_e = new_expr(EXPR_BINARY);
    new_e->binary.left = expr;
    new_e->binary.op = op;
    new_e->binary.right = right;
    expr = new_e;
  }
  return expr;
}

static Expr *term() {
  Expr *expr = factor();
  while (match(TOKEN_PLUS) || match(TOKEN_MINUS)) {
    Token op = parser.previous;
    Expr *right = factor();
    Expr *new_e = new_expr(EXPR_BINARY);
    new_e->binary.left = expr;
    new_e->binary.op = op;
    new_e->binary.right = right;
    expr = new_e;
  }
  return expr;
}

static Expr *comparison() {
  Expr *expr = term();
  while (match(TOKEN_GREATER) || match(TOKEN_GREATER_EQUAL) ||
         match(TOKEN_LESS) || match(TOKEN_LESS_EQUAL)) {
    Token op = parser.previous;
    Expr *right = term();
    Expr *new_e = new_expr(EXPR_BINARY);
    new_e->binary.left = expr;
    new_e->binary.op = op;
    new_e->binary.right = right;
    expr = new_e;
  }
  return expr;
}

static Expr *equality() {
  Expr *expr = comparison();
  while (match(TOKEN_BANG_EQUAL) || match(TOKEN_EQUAL_EQUAL)) {
    Token op = parser.previous;
    Expr *right = comparison();
    Expr *new_e = new_expr(EXPR_BINARY);
    new_e->binary.left = expr;
    new_e->binary.op = op;
    new_e->binary.right = right;
    expr = new_e;
  }
  return expr;
}

static Expr *and_expr() {
  Expr *expr = equality();
  // uoeghhhfhwqf (this is a stub)
  return expr;
}

static Expr *or_expr() {
  return and_expr();
}

static Expr *assignment() {
  Expr *expr = or_expr();
  if (match(TOKEN_EQUAL) || match(TOKEN_PLUS_EQUAL) || match(TOKEN_MINUS_EQUAL)) {
    Token op = parser.previous;
    Expr *value = assignment();
    if (expr->type == EXPR_VAR) {
      Token name = expr->var.name;
      
      if (op.type == TOKEN_PLUS_EQUAL || op.type == TOKEN_MINUS_EQUAL) {
        Expr *left_var = new_expr(EXPR_VAR);
        left_var->var.name = name;
        
        Expr *binary = new_expr(EXPR_BINARY);
        binary->binary.left = left_var;
        binary->binary.op = op;
        binary->binary.op.type = (op.type == TOKEN_PLUS_EQUAL) ? TOKEN_PLUS : TOKEN_MINUS;
        binary->binary.right = value;
        
        value = binary;
      }

      Expr *e = new_expr(EXPR_ASSIGN);
      e->assign.name = name;
      e->assign.value = value;
      kfree(expr);
      return e;
    }
    error("Invalid assignment target.");
  }
  return expr;
}

static Expr *expression() {
  return assignment();
}

static Stmt *print_statement() {
  Expr *value = expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after value.");
  Stmt *stmt = new_stmt(STMT_PRINT);
  stmt->print.expression = value;
  return stmt;
}

static Stmt *expression_statement() {
  Expr *expr = expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after expression.");
  Stmt *stmt = new_stmt(STMT_EXPR);
  stmt->expr.expression = expr;
  return stmt;
}

static Stmt *block_statement() {
  Stmt **stmts = NULL;
  int count = 0;
  while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
    stmts = (Stmt **)krealloc(stmts, sizeof(Stmt *) * (count + 1));
    stmts[count++] = declaration();
  }
  consume(TOKEN_RBRACE, "Expect '}' after block.");
  Stmt *stmt = new_stmt(STMT_BLOCK);
  stmt->block.statements = stmts;
  stmt->block.count = count;
  return stmt;
}

static Stmt *if_statement() {
  consume(TOKEN_LPAREN, "Expect '(' after 'FUWA'.");
  Expr *condition = expression();
  consume(TOKEN_RPAREN, "Expect ')' after condition.");
  Stmt *then_branch = statement();
  Stmt *else_branch = NULL;
  if (match(TOKEN_ELSE) || match(TOKEN_MOCO)) {
    else_branch = statement();
  }
  Stmt *stmt = new_stmt(STMT_IF);
  stmt->if_stmt.condition = condition;
  stmt->if_stmt.then_branch = then_branch;
  stmt->if_stmt.else_branch = else_branch;
  return stmt;
}

static Stmt *while_statement() {
  consume(TOKEN_LPAREN, "Expect '(' after 'PERO'.");
  Expr *condition = expression();
  consume(TOKEN_RPAREN, "Expect ')' after condition.");
  Stmt *body = statement();
  Stmt *stmt = new_stmt(STMT_WHILE);
  stmt->while_stmt.condition = condition;
  stmt->while_stmt.body = body;
  return stmt;
}

static Stmt *for_statement() {
  consume(TOKEN_LPAREN, "Expect '(' after 'PONDE'.");
  Expr *count_expr = expression();
  consume(TOKEN_RPAREN, "Expect ')' after count.");
  
  //parse til RING
  Stmt **stmts = NULL;
  int count = 0;
  while (!check(TOKEN_RING) && !check(TOKEN_EOF)) {
    stmts = (Stmt **)krealloc(stmts, sizeof(Stmt *) * (count + 1));
    stmts[count++] = declaration();
  }
  consume(TOKEN_RING, "Expect 'RING' after loop body.");
  
  Stmt *body = new_stmt(STMT_BLOCK);
  body->block.statements = stmts;
  body->block.count = count;
  
  Stmt *stmt = new_stmt(STMT_FOR_RANGE);
  stmt->for_range.count_expr = count_expr;
  stmt->for_range.body = body;
  return stmt;
}

static Stmt *panic_statement() {
  Expr *msg = expression();
  consume(TOKEN_SEMICOLON, "Expect ';' after panic message.");
  Stmt *stmt = new_stmt(STMT_PANIC);
  stmt->panic_stmt.message = msg;
  return stmt;
}

static Stmt *import_statement() {
  consume(TOKEN_IDENT, "Expect library name after 'OFFCOLLAB'.");
  Token name = parser.previous;
  consume(TOKEN_SEMICOLON, "Expect ';' after library name.");
  Stmt *stmt = new_stmt(STMT_IMPORT);
  stmt->import_stmt.name = name;
  return stmt;
}

static Stmt *return_statement() {
  Token keyword = parser.previous;
  Expr *value = NULL;
  if (!check(TOKEN_SEMICOLON)) {
    value = expression();
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after return value.");
  Stmt *stmt = new_stmt(STMT_RETURN);
  stmt->return_stmt.keyword = keyword;
  stmt->return_stmt.value = value;
  return stmt;
}

static Stmt *statement() {
  if (match(TOKEN_PRINT) || match(TOKEN_BAU)) return print_statement(); // BAU
  if (match(TOKEN_IF) || match(TOKEN_FUWA)) return if_statement();
  if (match(TOKEN_WHILE) || match(TOKEN_PERO)) return while_statement();
  if (match(TOKEN_FOR) || match(TOKEN_PONDE)) return for_statement();
  if (match(TOKEN_RETURN)) return return_statement();
  if (match(TOKEN_PANIC) || match(TOKEN_CHIHUAHUA)) return panic_statement();
  if (match(TOKEN_IMPORT)) return import_statement();
  if (match(TOKEN_LBRACE)) return block_statement();
  return expression_statement();
}

static Stmt *function_declaration() {
  consume(TOKEN_IDENT, "Expect function name.");
  Token name = parser.previous;
  consume(TOKEN_LPAREN, "Expect '(' after function name.");
  
  Token *params = NULL;
  int count = 0;
  if (!check(TOKEN_RPAREN)) {
    do {
      params = (Token *)krealloc(params, sizeof(Token) * (count + 1));
      consume(TOKEN_IDENT, "Expect parameter name.");
      params[count++] = parser.previous;
    } while (match(TOKEN_COMMA));
  }
  consume(TOKEN_RPAREN, "Expect ')' after parameters.");
  
  consume(TOKEN_LBRACE, "Expect '{' before function body.");
  
  Stmt **body = NULL;
  int body_count = 0;
  while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
    body = (Stmt **)krealloc(body, sizeof(Stmt *) * (body_count + 1));
    body[body_count++] = declaration();
  }
  consume(TOKEN_RBRACE, "Expect '}' after function body.");
  
  Stmt *stmt = new_stmt(STMT_FUNCTION);
  stmt->function.name = name;
  stmt->function.params = params;
  stmt->function.param_count = count;
  stmt->function.body = body;
  stmt->function.body_count = body_count;
  return stmt;
}

static Stmt *var_declaration() {
  consume(TOKEN_IDENT, "Expect variable name.");
  Token name = parser.previous;
  Expr *initializer = NULL;
  if (match(TOKEN_EQUAL)) {
    initializer = expression();
  }
  consume(TOKEN_SEMICOLON, "Expect ';' after variable declaration.");
  Stmt *stmt = new_stmt(STMT_VAR);
  stmt->var.name = name;
  stmt->var.initializer = initializer;
  return stmt;
}

static Stmt *declaration() {
  if (match(TOKEN_FUN)) return function_declaration();
  if (match(TOKEN_VAR)) return var_declaration(); // RUFF
  return statement();
}

static void sync() {
  parser.panic_mode = false;

  while (parser.current.type != TOKEN_EOF) {
    if (parser.previous.type == TOKEN_SEMICOLON) return;

    switch (parser.current.type) {
      case TOKEN_FUN:
      case TOKEN_VAR:
      case TOKEN_IF:
      case TOKEN_WHILE:
      case TOKEN_PRINT:
      case TOKEN_RETURN:
      case TOKEN_BAU:
        return;
      default: ; // do nothing
    }

    advance();
  }
}

Stmt **parse(const char *source, int *count) {
  scanner_init(source);
  parser.had_error = false;
  parser.panic_mode = false;
  parser.error_count = 0;
  advance();

  int check_count = 0;
  int start_errors = 0;
  Token lookahead = parser.current;
  while (check_count < 10 && lookahead.type != TOKEN_EOF) {
    if (lookahead.type == TOKEN_ERROR) start_errors++;
    lookahead = scan_token();
    check_count++;
  }

  scanner_init(source);
  advance();

  if (start_errors > 4) {
      kprint("ts gotta be invalid dawg like come on now\n");
      *count = 0;
      return NULL;
  }

  Stmt **stmts = NULL;
  *count = 0;

  while (!match(TOKEN_EOF)) {
    if (parser.error_count >= MAX_PARSE_ERRORS) {
        // cleanup
        for (int i = 0; i < *count; i++) free_stmt(stmts[i]);
        if (stmts) kfree(stmts);
        *count = 0;
        return NULL;
    }
    stmts = (Stmt **)krealloc(stmts, sizeof(Stmt *) * ((*count) + 1));
    stmts[(*count)++] = declaration();
    if (parser.panic_mode) sync();
  }

  if (parser.had_error) {
      for (int i = 0; i < *count; i++) free_stmt(stmts[i]);
      if (stmts) kfree(stmts);
      *count = 0;
      return NULL;
  }

  return stmts;
}

void free_expr(Expr *expr) {
  if (expr == NULL) return;
  switch (expr->type) {
    case EXPR_BINARY:
      free_expr(expr->binary.left);
      free_expr(expr->binary.right);
      break;
    case EXPR_UNARY:
      free_expr(expr->unary.right);
      break;
    case EXPR_LITERAL:
      if (expr->literal.lit_type == LIT_STRING && expr->literal.string) kfree(expr->literal.string);
      break;
    case EXPR_ASSIGN:
      free_expr(expr->assign.value);
      break;
    case EXPR_CALL:
      free_expr(expr->call.callee);
      for (int i = 0; i < expr->call.argc; i++) free_expr(expr->call.args[i]);
      kfree(expr->call.args);
      break;
    case EXPR_LOGICAL:
      free_expr(expr->logical.left);
      free_expr(expr->logical.right);
      break;
    default: break;
  }
  kfree(expr);
}

void free_stmt(Stmt *stmt) {
  if (stmt == NULL) return;
  switch (stmt->type) {
    case STMT_EXPR: free_expr(stmt->expr.expression); break;
    case STMT_PRINT: free_expr(stmt->print.expression); break;
    case STMT_VAR: free_expr(stmt->var.initializer); break;
    case STMT_BLOCK:
      for (int i = 0; i < stmt->block.count; i++) free_stmt(stmt->block.statements[i]);
      kfree(stmt->block.statements);
      break;
    case STMT_IF:
      free_expr(stmt->if_stmt.condition);
      free_stmt(stmt->if_stmt.then_branch);
      free_stmt(stmt->if_stmt.else_branch);
      break;
    case STMT_WHILE:
      free_expr(stmt->while_stmt.condition);
      free_stmt(stmt->while_stmt.body);
      break;
    case STMT_PANIC:
      free_expr(stmt->panic_stmt.message);
      break;
    case STMT_FOR_RANGE:
      free_expr(stmt->for_range.count_expr);
      free_stmt(stmt->for_range.body);
      break;
    case STMT_FUNCTION:
      for (int i = 0; i < stmt->function.body_count; i++) free_stmt(stmt->function.body[i]);
      if (stmt->function.body) kfree(stmt->function.body);
      if (stmt->function.params) kfree(stmt->function.params);
      break;
    case STMT_RETURN:
      if (stmt->return_stmt.value) free_expr(stmt->return_stmt.value);
      break;
    case STMT_IMPORT:
      break;
    default: break;
  }
  kfree(stmt);
}