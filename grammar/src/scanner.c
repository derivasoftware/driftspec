/* External scanner for DriftSpec: `type`, a type as the code writes it, and `block_text`, the indented multiline value
 * of a field (`text:` followed by a newline and lines indented deeper than
 * the first content line's left margin, blank lines included). */
#include <stdbool.h>

#include "tree_sitter/parser.h"

enum TokenType { BLOCK_TEXT, TYPE };

void* tree_sitter_driftspec_external_scanner_create(void) { return NULL; }
void tree_sitter_driftspec_external_scanner_destroy(void* p) { (void)p; }
unsigned tree_sitter_driftspec_external_scanner_serialize(void* p, char* b) {
  (void)p;
  (void)b;
  return 0;
}
void tree_sitter_driftspec_external_scanner_deserialize(void* p, const char* b, unsigned n) {
  (void)p;
  (void)b;
  (void)n;
}

static void skip_spaces(TSLexer* lx) {
  while (lx->lookahead == ' ' || lx->lookahead == '\t' || lx->lookahead == '\r') lx->advance(lx, true);
}

/* A type is the code's text, written as the code writes it (DS-18). Brackets <> [] ()
 * nest; at the top level the type stops at , ) ] > = { } a newline, a lone ':'
 * (not '::'), '->', '<<' (an operator set), '//' or the word `fails`. Spaces
 * inside are kept (`long long`); trailing ones are not part of the token. */
static bool is_word(int32_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static bool scan_type(TSLexer* lx) {
  while (lx->lookahead == ' ' || lx->lookahead == '\t') lx->advance(lx, true);
  int depth = 0;
  bool any = false;
  bool after_space = true;
  for (;;) {
    int32_t c = lx->lookahead;
    if (lx->eof(lx)) break;
    if (c == '\r' || c == '\n') {
      if (depth == 0) break;
      lx->advance(lx, false);
      after_space = true;
      continue;
    }
    if (c == ' ' || c == '\t') {
      lx->advance(lx, false);
      after_space = true;
      continue;
    }
    if (depth == 0 && (c == ',' || c == ')' || c == ']' || c == '>' || c == '=' || c == '{' || c == '}')) break;
    if (c == '"') { /* a string inside a type (Literal["a"]) */
      lx->advance(lx, false);
      while (!lx->eof(lx) && lx->lookahead != '"' && lx->lookahead != '\n') lx->advance(lx, false);
      if (lx->lookahead == '"') lx->advance(lx, false);
      lx->mark_end(lx);
      any = true;
      after_space = false;
      continue;
    }
    if (c == ':') {
      lx->advance(lx, false);
      if (lx->lookahead != ':') break; /* a lone ':' ends it (realises, the outcome's text) */
      lx->advance(lx, false);
      lx->mark_end(lx);
      any = true;
      after_space = false;
      continue;
    }
    if (c == '-' && depth == 0) {
      lx->advance(lx, false);
      if (lx->lookahead == '>') break;
      lx->mark_end(lx);
      any = true;
      after_space = false;
      continue;
    }
    if (c == '/') {
      lx->advance(lx, false);
      if (lx->lookahead == '/') break;
      lx->mark_end(lx);
      any = true;
      after_space = false;
      continue;
    }
    if (c == '<' && depth == 0) {
      lx->advance(lx, false);
      if (lx->lookahead == '<') break; /* an operator set */
      depth++;
      lx->mark_end(lx);
      any = true;
      after_space = false;
      continue;
    }
    if (is_word(c)) {
      char word[8];
      unsigned n = 0;
      while (is_word(lx->lookahead)) {
        if (n < sizeof word) word[n] = (char)lx->lookahead;
        n++;
        lx->advance(lx, false);
      }
      if (depth == 0 && after_space && any && n == 5 && word[0] == 'f' && word[1] == 'a' && word[2] == 'i' && word[3] == 'l' &&
          word[4] == 's')
        break; /* `fails`: what the operation may fail with, not the type */
      lx->mark_end(lx);
      any = true;
      after_space = false;
      continue;
    }
    if (c == '<' || c == '(' || c == '[') depth++;
    if (c == '>' || c == ')' || c == ']') depth--;
    lx->advance(lx, false);
    lx->mark_end(lx);
    any = true;
    after_space = false;
  }
  if (!any || depth != 0) return false;
  lx->result_symbol = TYPE;
  return true;
}

bool tree_sitter_driftspec_external_scanner_scan(void* p, TSLexer* lx, const bool* valid) {
  (void)p;
  if (valid[TYPE] && !valid[BLOCK_TEXT]) return scan_type(lx);
  if (!valid[BLOCK_TEXT]) return false;
  /* The field value starts right after ':'. A block only exists when the rest
   * of the line is empty and the next non-blank line is indented. */
  skip_spaces(lx);
  if (lx->lookahead != '\n') return false;
  /* find the first content line and its indentation */
  unsigned indent = 0;
  for (;;) {
    while (lx->lookahead == '\n') lx->advance(lx, true);
    indent = 0;
    while (lx->lookahead == ' ' || lx->lookahead == '\t') {
      lx->advance(lx, true);
      indent++;
    }
    if (lx->lookahead == '\n') continue;
    break;
  }
  if (indent == 0 || lx->eof(lx)) return false;
  lx->result_symbol = BLOCK_TEXT;
  /* consume lines while they are blank or indented at least `indent` */
  for (;;) {
    while (lx->lookahead != '\n' && !lx->eof(lx)) lx->advance(lx, false);
    lx->mark_end(lx);
    if (lx->eof(lx)) return true;
    /* look ahead at the next line without committing */
    lx->advance(lx, false);
    unsigned n = 0;
    while (lx->lookahead == ' ' || lx->lookahead == '\t') {
      lx->advance(lx, false);
      n++;
    }
    if (lx->lookahead == '\n') continue;        /* blank line inside the block */
    if (lx->eof(lx) || n < indent) return true; /* dedent: the block ended at mark_end */
  }
}
