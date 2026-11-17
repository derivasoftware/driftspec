// The editor coloured by DriftSpec's own grammar (DS-269): the tree-sitter parser of the language, compiled to
// WebAssembly, and the highlight queries every other editor reads (grammar/queries/highlights.scm). A document is
// parsed whole when its text has changed and CodeMirror asks for a line; what the queries capture says the colour
// of each character. Until the grammar is loaded, or where it cannot be, the page keeps its mode of regular
// expressions.
const RUNTIME = "https://cdn.jsdelivr.net/npm/web-tree-sitter@0.26.13/";

// a capture of the queries, most specific first, and the token of the page's theme it is drawn as
const STYLES = [
  ["comment", "comment"], ["string", "string"], ["number", "number"], ["keyword", "keyword"],
  ["type", "type"], ["module", "type"], ["function", "def"], ["constant", "variable-2"], ["attribute", "variable-2"],
  ["label", "variable-2"], ["property", "property"], ["variable", "variable"], ["operator", "operator"],
  ["punctuation", "operator"],
];
const styleOf = (capture) => { for (const [name, style] of STYLES) if (capture === name || capture.startsWith(name + ".")) return style; return null; };

let parser = null, query = null;
const painted = new WeakMap();  // a document -> the generation of its text and, per line, the style of each character

function paint(doc) {
  const generation = doc.changeGeneration();
  const had = painted.get(doc);
  if (had && had.generation === generation) return had.lines;
  const lines = [];
  const tree = parser.parse(doc.getValue());
  for (const { name, node } of query.captures(tree.rootNode)) {  // a later pattern of the queries wins, as in Neovim
    const style = styleOf(name);
    if (!style) continue;
    const from = node.startPosition, to = node.endPosition;
    for (let row = from.row; row <= to.row; row++) {
      const line = lines[row] || (lines[row] = []);
      const start = row === from.row ? from.column : 0, end = row === to.row ? to.column : doc.getLine(row).length;
      for (let ch = start; ch < end; ch++) line[ch] = style;
    }
  }
  tree.delete();
  painted.set(doc, { generation, lines });
  return lines;
}

/// Load the grammar and declare the mode that reads it; resolves to the mode's name, or to null when the grammar
/// cannot be loaded and the page keeps the mode it had.
export async function grammarMode(CodeMirror) {
  try {
    const { Parser, Language, Query } = await import(RUNTIME + "web-tree-sitter.js");
    await Parser.init({ locateFile: (name) => RUNTIME + name });
    const language = await Language.load("tree-sitter-driftspec.wasm");
    parser = new Parser();
    parser.setLanguage(language);
    query = new Query(language, await (await fetch("highlights.scm")).text());
  } catch (e) {
    console.warn("the grammar could not be loaded; the editor keeps its simple colours", e);
    return null;
  }
  CodeMirror.defineMode("driftspec-grammar", () => ({
    startState: () => ({}),
    token(stream) {
      const at = stream.lineOracle;  // CodeMirror's own context: the document and the line being read
      const line = at && at.doc ? paint(at.doc)[at.line] : null;
      if (!line) { stream.skipToEnd(); return null; }
      const style = line[stream.pos] || null;
      let end = stream.pos + 1;
      while (end < stream.string.length && (line[end] || null) === style) end++;
      stream.pos = end;
      return style;
    },
    lineComment: "//",
  }));
  return "driftspec-grammar";
}
