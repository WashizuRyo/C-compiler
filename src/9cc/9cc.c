#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//
// Tokenizer
//

typedef enum {
  TK_RESERVED,
  TK_NUM,
  TK_EOF,
} TokenKind;

typedef struct Token Token;

struct Token {
  TokenKind kind;
  Token *next;
  int val;
  char *str;
  int len;
};

char *user_input;

Token *token;

void error(char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}


void error_at(char *loc, char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);

  int pos = loc - user_input;
  fprintf(stderr, "%s\n", user_input);
  fprintf(stderr, "%*s", pos, " ");
  fprintf(stderr, "^ ");
  vfprintf(stderr, fmt, ap);
  fprintf(stderr, "\n");
  exit(1);
}

bool consume(char *op) {
  if (token->kind != TK_RESERVED ||
      strlen(op) != token->len   ||
      memcmp(token->str, op, token->len))
    return false;
  token = token->next;
  return true;
}

void expect(char *op) {
  if (token->kind != TK_RESERVED ||
      strlen(op) != token->len   ||
      memcmp(token->str, op, token->len))
    error_at(token->str, "'%c'ではありません", op);
  token = token->next;
}

int expect_number() {
  if (token->kind != TK_NUM)
    error_at(token->str, "数ではありません");
  int val = token->val;
  token = token->next;
  return val;
}

bool at_eof() {
  return token->kind == TK_EOF;
}

Token *new_token(TokenKind kind, Token *cur, char *str, int len) {
  Token *tok = calloc(1, sizeof(Token));
  tok->kind = kind;
  tok->str = str;
  tok->len = len;
  cur->next = tok;
  return tok;
}

bool startswith(char *p, char *q) {
  return memcmp(p, q, strlen(q)) == 0;
}

Token *tokenize() {
  char *p = user_input;
  Token head;
  head.next = NULL;
  Token *cur = &head;

  while(*p) {
    if(isspace(*p)) {
      p++;
      continue;
    }

    if (startswith(p, "==") || startswith(p, "!=") || 
        startswith(p, "<=") || startswith(p, ">=")) {
      cur = new_token(TK_RESERVED, cur, p, 2);
      p += 2;
      continue;
      }

    if (strchr("+-*/()<>", *p)) {
      cur = new_token(TK_RESERVED, cur, p++, 1);
      continue;
    }

    if (isdigit(*p)) {
      cur = new_token(TK_NUM, cur, p, 0);
      char *q = p;
      cur->val = strtol(p, &p, 10);
      cur->len = p - q;
      continue;
    }

  error_at(p, "トークナイズできません");
  }

  new_token(TK_EOF, cur, p, 0);
  return head.next;
}

//
// Parser
//

typedef enum {
  ND_ADD, // +
  ND_SUB, // -
  ND_MUL, // *
  ND_DIV, // /
  ND_EQ,  // ==
  ND_NE,  // !=
  ND_LT,  // <
  ND_LE,  // <=
  ND_NUM, // 整数
} NodeKind;

typedef struct Node Node;

struct Node {
  NodeKind kind; // ノードの型
  Node *lhs;     // 左辺
  Node *rhs;     // 右辺
  int val;       // kindがND_NUMの場合のみに使う
};

Node *new_node(NodeKind kind) {
  Node *node = calloc(1, sizeof(Node));
  node->kind = kind;
  return node;
}

Node *new_binary(NodeKind kind, Node *lhs, Node *rhs) {
  Node *node = new_node(kind);
  node->lhs = lhs;
  node->rhs = rhs;
  return node;
}

Node *new_num(int val) {
  Node *node = new_node(ND_NUM);
  node->val = val;
  return node;
}

Node *expr();
Node *equality();
Node *relational();
Node *add();
Node *mul();
Node *primary();
Node *unary();

Node *expr() {
  return equality();
}

Node *equality() {
  Node *node = relational();

  for(;;) {
    if (consume("=="))
      node = new_binary(ND_EQ, node, relational());
    else if (consume("!="))
      node = new_binary(ND_NE, node, relational());
    else 
      return node;
  }
}

Node *relational() {
  Node *node = add();

  for(;;) {
    if (consume("<"))
      node = new_binary(ND_LT, node, add());
    else if (consume("<="))
      node = new_binary(ND_LE, node, add());
    else if (consume(">"))
      node = new_binary(ND_LT, add(), node);
    else if (consume(">="))
      node = new_binary(ND_LE, add(), node);
    else 
      return node;
  }
}

Node *add() {
  Node *node = mul();

  for(;;) {
    if (consume("+"))
      node = new_binary(ND_ADD, node, mul());
    else if (consume("-"))
      node = new_binary(ND_SUB, node, mul());
    else 
      return node;
  }
}

Node *mul() {
  Node *node = unary();

  for(;;) {
    if (consume("*"))
      node = new_binary(ND_MUL, node, unary());
    else if (consume("/"))
      node = new_binary(ND_DIV, node, unary());
    else 
      return node;
  }
}

Node *unary() {
  if(consume("+"))
    return primary();
  else if (consume("-"))
    return new_binary(ND_SUB, new_num(0), primary());
  return primary();
}

Node *primary() {
  if (consume("(")) {
    Node *node = expr();
    expect(")");
    return node;
  }

  return new_num(expect_number());
}

//
// Code generator
// 

void gen(Node *node) {
  if (node->kind == ND_NUM) {
    printf("  mov x2, #%d\n", node->val);
    printf("  str x2, [sp, -16]!\n");
    return;
  }

  gen(node->lhs);
  gen(node->rhs);

  printf("  ldr x1, [sp], 16\n");
  printf("  ldr x0, [sp], 16\n");

  switch (node->kind) {
    case ND_ADD:
      printf("  add x0, x0, x1\n");
      break;
    case ND_SUB:
      printf("  sub x0, x0, x1\n");
      break;
    case ND_MUL:
      printf(" mul x0, x0, x1\n");
      break;
    case ND_DIV:
      printf("  sdiv x0, x0, x1\n");
      break;
    case ND_EQ:
      printf("  cmp x0, x1\n");
      printf("  cset x0, EQ\n");
      break;
    case ND_NE:
      printf("  cmp x0, x1\n");
      printf("  cset x0, NE\n");
      break;
    case ND_LT:
      printf("  cmp x0, x1\n");
      printf("  cset x0, LT\n");
      break;
    case ND_LE:
      printf("  cmp x0, x1\n");
      printf("  cset x0, LE\n");
      break;
  }

  printf("  str x0, [sp, -16]!\n");
}

int main(int argc, char **argv) {
  if(argc != 2) 
    error("%s: invalid number of arguments", argv[0]);

  user_input = argv[1];
  token = tokenize();
  Node *node = expr();

  printf(".globl main\n");
  printf("main:\n");

  gen(node);

  printf("  ldr x0, [sp], 16\n");
  printf("  ret\n");
  return 0;
}