#include "baux2.h"
#include "../../include/kheap.h"
#include "../../include/serial.h"

typedef struct {
  const char *start;
  const char *current;
  int line;
} Scanner;

Scanner scanner;

void scanner_init(const char *source) {
  scanner.start = source;
  scanner.current = source;
  scanner.line = 1;
}

static bool is_at_end() {
  return *scanner.current == '\0';
}

static char advance() {
  scanner.current++;
  return scanner.current[-1];
}

static char peek() {
  return *scanner.current;
}

static char peek_next() {
  if (is_at_end()) return '\0';
  return scanner.current[1];
}

static bool match(char expected) {
  if (is_at_end()) return false;
  if (*scanner.current != expected) return false;
  scanner.current++;
  return true;
}

static Token make_token(TokenType type) {
  Token token;
  token.type = type;
  token.start = scanner.start;
  token.length = (int)(scanner.current - scanner.start);
  token.line = scanner.line;
  return token;
}

static Token error_token(const char *message) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner.line;
  return token;
}

static void skip_whitespace() {
  for (;;) {
    char c = peek();
    switch (c) {
      case ' ':
      case '\r':
      case '\t':
        advance();
        break;
      case '\n':
        scanner.line++;
        advance();
        break;
      case '/':
        if (peek_next() == '/') {
          while (peek() != '\n' && !is_at_end()) advance();
        } else {
          return;
        }
        break;
      default:
        return;
    }
  }
}

static TokenType check_keyword(int start, int length, const char *rest, TokenType type) {
  if (scanner.current - scanner.start == start + length &&
      memcmp(scanner.start + start, rest, length) == 0) {
    return type;
  }
  return TOKEN_IDENT;
}

/*
for my ref (the branches are falling off my leaves)

BAU - print
CHIHUAHUA - panick
FUWA - if
MOCO - else
OFFCOLLAB - import
PERO - while
PONDE - range loop (RING is closing)
RUFF - var, weakly typed
RUFFIAN - function
*/

static TokenType identifier_type() {
  switch (scanner.start[0]) {
    case 'B': 
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'A': return check_keyword(2, 1, "U", TOKEN_BAU);
          case 'O': return check_keyword(2, 2, "OL", TOKEN_BOOL_TYPE);
        }
      }
      break;
    case 'C': return check_keyword(1, 8, "HIHUAHUA", TOKEN_CHIHUAHUA);
    case 'F':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'L': return check_keyword(2, 4, "UFFY", TOKEN_FLUFFY);
          case 'U':
            if (scanner.current - scanner.start > 2) {
              switch (scanner.start[2]) {
                case 'W': return check_keyword(3, 1, "A", TOKEN_FUWA);
                case 'Z': return check_keyword(3, 2, "ZY", TOKEN_FUZZY);
              }
            }
            break;
        }
      }
      break;
    case 'M': return check_keyword(1, 3, "OCO", TOKEN_MOCO);
    case 'O':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'F': return check_keyword(2, 7, "FCOLLAB", TOKEN_IMPORT);
        }
      }
      break;
    case 'P':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'E': return check_keyword(2, 2, "RO", TOKEN_PERO);
          case 'O': return check_keyword(2, 3, "NDE", TOKEN_PONDE);
        }
      }
      break;
    case 'R':
      if (scanner.current - scanner.start > 1) {
        switch (scanner.start[1]) {
          case 'I': return check_keyword(2, 2, "NG", TOKEN_RING);
          case 'U': 
            if (scanner.current - scanner.start > 2) {
               if (scanner.start[2] == 'F' && scanner.start[3] == 'F') {
                  if (scanner.current - scanner.start == 4) return TOKEN_VAR;
                  return check_keyword(4, 3, "IAN", TOKEN_FUN);
               }
            }
            break;
        }
      }
      break;
    case 'f': return check_keyword(1, 2, "un", TOKEN_FUN);
    case 'i': return check_keyword(1, 1, "f", TOKEN_IF);
    case 'e': return check_keyword(1, 3, "lse", TOKEN_ELSE);
    case 'r': return check_keyword(1, 5, "eturn", TOKEN_RETURN);
    case 'v': return check_keyword(1, 2, "ar", TOKEN_VAR);
    case 'w': return check_keyword(1, 4, "hile", TOKEN_WHILE);
    case 'n': return check_keyword(1, 2, "il", TOKEN_NIL);
  }

  return TOKEN_IDENT;
}

static Token identifier() {
  while (isalpha(peek()) || (peek() >= '0' && peek() <= '9') || peek() == '.' || peek() == '_') advance();
  return make_token(identifier_type());
}

static Token number() {
  while (peek() >= '0' && peek() <= '9') advance();

  if (peek() == '.' && (peek_next() >= '0' && peek_next() <= '9')) {
    advance();
    while (peek() >= '0' && peek() <= '9') advance();
  }

  return make_token(TOKEN_NUMBER);
}

static Token string() {
  while (peek() != '"' && !is_at_end()) {
    if (peek() == '\n') scanner.line++;
    advance();
  }

  if (is_at_end()) return error_token("Unterminated string.");

  advance();
  return make_token(TOKEN_STRING);
}

Token scan_token() {
  skip_whitespace();
  scanner.start = scanner.current;

  if (is_at_end()) return make_token(TOKEN_EOF);

  char c = advance();
  if (isalpha(c) || c == '_' || (c == '.' && isalpha(peek()))) {
      return identifier();
  }
  if (c >= '0' && c <= '9') return number();

  switch (c) {
    case '(': return make_token(TOKEN_LPAREN);
    case ')': return make_token(TOKEN_RPAREN);
    case '{': return make_token(TOKEN_LBRACE);
    case '}': return make_token(TOKEN_RBRACE);
    case ';': return make_token(TOKEN_SEMICOLON);
    case ',': return make_token(TOKEN_COMMA);
    case '.': { kprint("[?] token_dot\n"); return make_token(TOKEN_DOT); }
    case '-': return make_token(match('=') ? TOKEN_MINUS_EQUAL : TOKEN_MINUS);
    case '+': return make_token(match('=') ? TOKEN_PLUS_EQUAL : TOKEN_PLUS);
    case '/': return make_token(TOKEN_SLASH);
    case '*': return make_token(TOKEN_STAR);
    case '!': return make_token(match('=') ? TOKEN_BANG_EQUAL : TOKEN_BANG);
    case '=': return make_token(match('=') ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL);
    case '<': return make_token(match('=') ? TOKEN_LESS_EQUAL : TOKEN_LESS);
    case '>': return make_token(match('=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER);
    case '"': return string();
  }

  kprint("unexpected?\n");
  return error_token("Unexpected character.");
}