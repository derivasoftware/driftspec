#include "ds/model/Manual.hpp"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/text/Text.hpp"

// The manual ds embeds (DS-79), a class of its own (DS-342): how each kind is written with a
// minimal example that checks clean, every finding code with what it means and what to do, the
// annotations that mean something and the commands with their exit codes. One source: `ds manual`
// prints it and the tests hold it to the language (every example checks clean, every code reported
// is here).

namespace ds {
namespace model {

// The pages are written from the metamodel the manual is made with: the kinds with how each is written, the
// catalogue of codes and the annotations are its (DS-340, DS-341). The manual reads the metamodel and the
// metamodel never reads the manual; the commands are the manual's (DS-342).

Manual::Manual(const Metamodel& metamodel) : metamodel_(metamodel), commands_(buildCommands()), guides_(buildGuides()) {}

std::vector<CommandSpec> Manual::buildCommands() {
  return {
      {"check",
       "ds check <workspace> [--json]",
       "the findings of a workspace, then the counts of its model",
       {{0, "no error among the findings (warnings may be)"},
        {1, "an error is among the findings"},
        {2, "bad usage, or no workspace at the path"}}},
      {"export",
       "ds export <workspace>",
       "the model as JSON: entities, relations, findings and its metamodel",
       {{0, "the model, even with findings"}, {2, "bad usage, or no workspace at the path"}}},
      {"render",
       "ds render <workspace|ir.json> [diagram] [-f html|json|svg|text|cells|ir|layout] [-w N] [-o file] [--embed [--height N]] [--css "
       "file]",
       "the whole model in the viewer (html, the default), its scene (json), or one diagram drawn (svg, text, cells) or as its data "
       "(ir, layout)",
       {{0, "drawn"},
        {1, "no such diagram, or a file of the workspace does not read whole: a drawing is not made of what did not parse"},
        {2, "bad usage (a format that needs a diagram without one), or nothing to read at the path"}}},
      {"query",
       "ds query <workspace> <question> [subject] [--json]",
       "ask the model: diagrams · entities <scope> · relations <id> · artefacts <id> · behaviours <id> · shows <diagram> · findings "
       "[file] · coverage [identity] · coupling · failures <operation>",
       {{0, "answered"}, {1, "not a question, or an identity the model does not declare"}, {2, "bad usage, or no workspace at the path"}}},
      {"manual",
       "ds manual [topic] [--json]",
       "this manual: the index, a kind (class, sequence...), a finding code (W-REF-UNDECLARED...), annotations, commands or codes",
       {{0, "the page"}, {1, "no such topic"}, {2, "bad usage"}}},
  };
}

const CommandSpec* Manual::commandOf(const std::string& name) const {
  for (const auto& c : commands_)
    if (c.name == name) return &c;
  return nullptr;
}

std::string Manual::indent(const std::string& text, const std::string& by) {
  std::string out;
  std::size_t start = 0;
  while (start < text.size()) {
    std::size_t end = text.find('\n', start);
    if (end == std::string::npos) end = text.size();
    out += by + text.substr(start, end - start) + "\n";
    start = end + 1;
  }
  return out;
}

std::string Manual::pad(const std::string& text, std::size_t width) {
  return text.size() >= width ? text + " " : text + std::string(width - text.size(), ' ');
}

std::string Manual::names(const std::vector<Kind>& kinds) const {
  std::string out;
  for (std::size_t i = 0; i < kinds.size(); ++i) out += (i ? ", " : "") + metamodel_.nameOf(kinds[i]);
  return out;
}

std::string Manual::kindPage(const KindSpec& k) const {
  std::string out = metamodel_.nameOf(k.kind) + " — " + k.meaning + "\n";
  out += "declared in: " + (k.parents.empty() ? std::string("the top of a file") : names(k.parents)) +
         (k.top && !k.parents.empty() ? ", or at the top" : "") + "\n\n";
  out += "How it is written\n" + indent(k.syntax, "  ") + "\n";
  if (!k.fields.empty()) {
    out += "Fields\n";
    for (const auto& f : k.fields)
      out += "  " + pad(f.name, 14) + pad(metamodel_.fieldTypeName(f.type), 9) + pad(metamodel_.fieldSourceName(f.source), 9) + f.meaning +
             (f.values.empty() ? "" : " (one of: " + text::join(f.values, ", ") + ")") + "\n";
    out += "  (written: you write it as key: value; syntax: read from how it is declared; derived: computed by the check)\n\n";
  }
  if (!k.flags.empty()) {
    out += "Flags: ";
    for (std::size_t i = 0; i < k.flags.size(); ++i) out += (i ? ", " : "") + k.flags[i];
    out += "\n\n";
  }
  std::string from, to;
  for (const auto& r : metamodel_.relations()) {
    if (r.kind == "contains" || r.kind == "refers") continue;
    for (Kind f : r.from)
      if (f == k.kind)
        from += "  " + pad(r.kind, 20) + "to " + (r.to.empty() ? std::string("anything") : names(r.to)) + "   " + r.meaning + "\n";
    for (Kind t : r.to)
      if (t == k.kind) to += "  " + pad(r.kind, 20) + "from " + (r.from.empty() ? std::string("anything") : names(r.from)) + "\n";
  }
  if (!from.empty()) out += "Relations from it\n" + from + "\n";
  if (!to.empty()) out += "Relations to it\n" + to + "\n";
  out += "Example (a workspace that checks clean)\n" + indent(k.example, "  ");
  return out;
}

std::string Manual::indexPage() const {
  std::string out =
      "ds — a software design as text: checked, drawn and asked.\n\n"
      "A workspace is a directory of .ds files, read as one model in which every entity has one identity.\n"
      "Write its high-level design (packages, interfaces), its low-level design (modules, classes, functions,\n"
      "sequences, state machines) and the artefacts about it (requirements, constraints, verifications) in it;\n"
      "then check it, look at it, ask it questions or hand it to another tool.\n\n"
      "Commands (ds manual commands for their exit codes)\n";
  for (const auto& c : commands_) out += "  " + pad(c.name, 8) + c.does + "\n";
  if (!guides_.empty()) {
    out += "\nGuides (ds manual <guide>: the language explained, with examples)\n  ";
    for (const auto& guide : guides_) out += guide.first + "  ";
    out += "\n";
  }
  out += "\nKinds you write (ds manual <kind>: how it is written, its fields and an example)\n  ";
  std::size_t column = 2;
  for (const auto& k : metamodel_.kinds()) {
    const std::string name = metamodel_.nameOf(k.kind);
    if (column + name.size() > 100) {
      out += "\n  ";
      column = 2;
    }
    out += name + "  ";
    column += name.size() + 2;
  }
  out +=
      "\n\nAlso: ds manual grammar (what goes where, identities, annotations) · ds manual annotations · ds manual codes ·\n"
      "ds manual <CODE> (E-SYNTAX, W-REF-UNDECLARED...)\n";
  return out;
}

std::string Manual::grammarPage() const {
  std::string out =
      "How a workspace is written\n\n"
      "Files\n"
      "  A workspace is every .ds file under a directory, read as one model; files may split a package or a module any way.\n"
      "  One thing per line. // starts a comment. A field is key: value to the end of its line; a longer value goes on\n"
      "  the lines after key:, indented.\n\n"
      "Identities\n"
      "  Packages join with @, everything inside a package with ::  —  shop@store::Cart::add\n"
      "  A package reopens (package shop { } twice, or package shop@store { }); a name is resolved from the innermost\n"
      "  scope outwards, or qualified: store::Cart. Artefacts (requirements, verifications, constraints) are named by\n"
      "  their id, which ends in its revision: REQ-00001-1. The prefix (SREQ, REQ, VER, CST...) is a convention; the\n"
      "  level comes from where it is written.\n\n"
      "What goes inside what\n";
  for (const auto& k : metamodel_.kinds()) {
    std::string inside;
    for (const auto& other : metamodel_.kinds())
      for (Kind parent : other.parents)
        if (parent == k.kind) inside += (inside.empty() ? "" : ", ") + metamodel_.nameOf(other.kind);
    if (!inside.empty()) out += "  " + pad(metamodel_.nameOf(k.kind), 14) + inside + "\n";
  }
  out +=
      "  (and at the top of a file: package, diagram)\n\n"
      "Annotations\n"
      "  @<word> <ID> on a line of its own, inside the body of what it is about: a package, a module, a class, a member, a\n"
      "  function, an enum, an interface, an operation, a sequence, a requirement, a verification, a constraint.\n"
      "  ds manual annotations says which words mean something.\n\n"
      "Definitions and extensions\n"
      "  A body that holds only annotations does not define the entity again: it extends one defined elsewhere\n"
      "  (W-REF-UNDECLARED when nothing defines it). A class that nothing else defines is the class, empty: an\n"
      "  exception that only carries its requirements. Give it a member, a field or a value and it is a definition.\n\n"
      "Behaviour\n"
      "  A sequence is the behaviour of an operation, a method or a function: sequence Cart::add { ... }. A message\n"
      "  goes to self, an attribute, a local (local x: Type) or an earlier result: self -> stock.has(item) -> ok.\n"
      "  A scenario is one path through it, named: written apart (sequence Cart::add / out { ... }) or on a branch\n"
      "  (opt the item is out / out { ... }). A call names the scenario it enters: -> add(item) / out -> r.\n"
      "  A scenario may be written at any cut, in one block: its subject a use case or a box (sequence UC-00001-1\n"
      "  { ... }, sequence sales { ... }), its participants named as what they are, a package, a module, an interface\n"
      "  or a class, and whoever is written before the arrow sending: sales -> warehouse.take(item). A message to a\n"
      "  box is bound to the operation the box offers under that name, or is a label until that level is written.\n"
      "  The scenario of a use case is held to the behaviour below it, the one of the operation it is asked through:\n"
      "  what it says crosses between its participants has to happen below, in that order; it may leave crossings out.\n"
      "  A path ends failing where a return would go: fail ParseError, the failure's name as text, whatever the\n"
      "  mechanism. An attempt runs its steps and handles what they fail with, one on each: attempt { ... } on IoError\n"
      "  { fail LoadError } translates it, a handler that returns resolves it, a failure no handler names goes on.\n"
      "  The default path does not fail; a handler that names a scenario is the path of that failure.\n"
      "  What escapes each operation is computed: ds query <workspace> failures <operation>. A signature promises it:\n"
      "  + load(path: string) -> Book fails LoadError, ParseError, or fails nothing; an interface operation must say it.\n"
      "  An operation with no behaviour fails as its signature says, so a library call can be said to fail.\n\n"
      "The counts ds check prints\n"
      "  entities · expected (named but declared nowhere: W-REF-UNDECLARED) · relations · findings\n";
  return out;
}

std::vector<std::string> Manual::topics() const {
  std::vector<std::string> out;
  for (const auto& guide : guides_) out.push_back(guide.first);
  for (const char* topic : {"grammar", "commands", "annotations", "codes"}) out.push_back(topic);
  for (const auto& k : metamodel_.kinds()) out.push_back(metamodel_.nameOf(k.kind));
  for (const auto& c : metamodel_.codes()) out.push_back(c.code);
  return out;
}

std::string Manual::page(const std::string& topic) const {
  if (topic.empty()) return indexPage();
  for (const auto& guide : guides_)
    if (guide.first == topic) return guide.second;
  if (topic == "grammar") return grammarPage();
  if (const KindSpec* k = metamodel_.specNamed(topic)) return kindPage(*k);
  if (const CodeSpec* c = metamodel_.codeOf(topic))
    return c->code + " (" + metamodel_.severityOf(c->severity).name + ")\n\n" + c->meaning + "\n\nWhat to do: " + c->fix + "\n";
  if (topic == "commands") {
    std::string out;
    for (const auto& c : commands_) {
      out += c.synopsis + "\n  " + c.does + "\n";
      for (const auto& e : c.exits) out += "  exit " + std::to_string(e.first) + "  " + e.second + "\n";
      out += "\n";
    }
    return out;
  }
  if (topic == "annotations") {
    std::string out = "An annotation links the entity it is written in to an artefact: @<word> <ID>.\n\n";
    for (const auto& a : metamodel_.annotations()) out += "  " + pad(a.name, 20) + pad(a.relation, 16) + a.meaning + "\n";
    return out;
  }
  if (topic == "codes") {
    std::string out;
    for (const auto& c : metamodel_.codes())
      out += "  " + pad(c.code, 26) + pad(metamodel_.severityOf(c.severity).name, 9) + c.meaning + "\n";
    return out + "\nds manual <CODE> says what to do about one.\n";
  }
  return "";
}

}  // namespace model
}  // namespace ds
