/* External scanner for DriftSpec: `block_text`, the indented multiline value
 * of a field (`text:` followed by a newline and lines indented deeper than
 * the first content line's left margin, blank lines included). */
#include "tree_sitter/parser.h"
#include <stdbool.h>

enum TokenType { BLOCK_TEXT };

void *tree_sitter_driftspec_external_scanner_create(void) { return NULL; }
void tree_sitter_driftspec_external_scanner_destroy(void *p) { (void)p; }
unsigned tree_sitter_driftspec_external_scanner_serialize(void *p, char *b) { (void)p; (void)b; return 0; }
void tree_sitter_driftspec_external_scanner_deserialize(void *p, const char *b, unsigned n) { (void)p; (void)b; (void)n; }

static void skip_spaces(TSLexer *lx) { while (lx->lookahead == ' ' || lx->lookahead == '\t' || lx->lookahead == '\r') lx->advance(lx, true); }

bool tree_sitter_driftspec_external_scanner_scan(void *p, TSLexer *lx, const bool *valid) {
  (void)p;
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
    while (lx->lookahead == ' ' || lx->lookahead == '\t') { lx->advance(lx, true); indent++; }
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
    while (lx->lookahead == ' ' || lx->lookahead == '\t') { lx->advance(lx, false); n++; }
    if (lx->lookahead == '\n') continue;          /* blank line inside the block */
    if (lx->eof(lx) || n < indent) return true;   /* dedent: the block ended at mark_end */
  }
}
