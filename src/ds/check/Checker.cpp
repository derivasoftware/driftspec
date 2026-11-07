#include "ds/check/Checker.hpp"

#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <regex>
#include <sstream>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

namespace {

// process artefact ids: PREFIX(-PART)*-NUMBER-REVISION, e.g. REQ-00001-2, DS-C-0001-0
const std::regex kArtefactWithRevision("^([A-Z]+(?:-[A-Z]+)*-[0-9]+)-([0-9]+)$");
const std::regex kArtefactBare("^[A-Z]+(?:-[A-Z]+)*-[0-9]+$");

std::string withoutRevision(const std::string& id) {
  std::smatch m;
  return std::regex_match(id, m, kArtefactWithRevision) ? m[1].str() : id;
}

bool looksLikeArtefact(const std::string& name) {
  return std::regex_match(name, kArtefactBare) || std::regex_match(name, kArtefactWithRevision);
}

/// Primitives one converts to the other without saying so; any other pair of primitives is a mismatch.
int primitiveFamily(const std::string& type) {
  for (const char* t : {"int", "float", "double", "size_t", "char"}) {
    if (type == t) return 1;
  }
  for (const char* t : {"string", "path"}) {
    if (type == t) return 2;
  }
  if (type == "bool") return 3;
  if (type == "none" || type == "void") return 4;
  return 0;
}

std::vector<std::string> split(const std::string& text, char sep) {
  std::vector<std::string> out;
  std::stringstream in(text);
  std::string item;
  while (std::getline(in, item, sep)) {
    const auto b = item.find_first_not_of(' ');
    const auto e = item.find_last_not_of(' ');
    if (b != std::string::npos) out.push_back(item.substr(b, e - b + 1));
  }
  return out;
}

std::string join(const std::vector<std::string>& items, const char* sep) {
  std::string out;
  for (const auto& i : items) out += (out.empty() ? "" : sep) + i;
  return out;
}

}  // namespace

namespace {

/// The scope outside `scope`: ds@ds::b → ds@ds → ds@ → "" (R10); with nested packages,
/// app@core@model → app@core@ → app@ → "": every package level adds its name with @.
std::string outer(const std::string& scope) {
  const auto cut = scope.rfind("::");
  if (cut != std::string::npos) return scope.substr(0, cut);
  std::string s = scope;
  if (!s.empty() && s.back() == '@') s.pop_back();  // a package scope: step out of that package
  const auto at = s.rfind('@');
  if (at != std::string::npos) return s.substr(0, at + 1);
  return "";
}

std::string qualifyIn(const std::string& scope, const std::string& name) {
  if (scope.empty()) return name;
  if (scope.back() == '@') return scope + name;
  return scope + "::" + name;
}

}  // namespace

std::string Checker::resolve(const std::string& name, const std::string& scope) const {
  if (model_.find(name)) return name;  // a full id (pkg@mod::Class), a global artefact, a package
  const std::string bare = withoutRevision(name);
  if (bare != name && model_.find(bare)) return bare;
  if (name.find('@') != std::string::npos) return "";  // absolute, and nobody declares it
  for (std::string s = scope; !s.empty(); s = outer(s)) {
    const std::string candidate = qualifyIn(s, name);
    if (model_.find(candidate)) return candidate;
  }
  // the extern catalogue (R11): a bare name nobody declares in scope is the one extern
  // entity of that simple name, declared in any extern module of the workspace
  const std::vector<std::string> externs = externsNamed(name);
  return externs.size() == 1 ? externs.front() : "";
}

std::vector<std::string> Checker::externsNamed(const std::string& name) const {
  std::vector<std::string> out;
  if (name.find("::") != std::string::npos || name.find('@') != std::string::npos) return out;
  // an extern is known by its simple name: a use's template arguments (tuple<2>) are not the
  // design's to check, so any arity of the use, and of the declaration, finds it (R11)
  const std::string simple = name.substr(0, name.find('<'));
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (e.has("extern") && (e.kind == model::Kind::Class || e.kind == model::Kind::Enum) && e.name == simple) out.push_back(e.id);
  }
  // several declarations of the name (tuple<1>, tuple<2>): the one of the use's arity, when there is one
  std::vector<std::string> exact;
  for (const auto& id : out) {
    if (id.size() >= name.size() + 2 && id.compare(id.size() - name.size() - 2, std::string::npos, "::" + name) == 0) exact.push_back(id);
  }
  return exact.size() == 1 ? exact : out;
}

bool Checker::derives(const std::string& derived, const std::string& base) const {
  std::vector<std::string> pending{derived};
  std::set<std::string> seen;
  while (!pending.empty()) {
    const std::string current = pending.back();
    pending.pop_back();
    if (current == base) return true;
    if (!seen.insert(current).second) continue;
    for (const auto& r : model_.relations()) {
      if (r.kind == "inherits" && r.from == current) pending.push_back(r.to);
    }
  }
  return false;
}

std::string Checker::current(const std::string& id) const {
  for (const auto& r : renamed_) {
    if (id == r.first) return r.second;
    if (id.rfind(r.first + "::", 0) == 0 || id.rfind(r.first + "/", 0) == 0) return r.second + id.substr(r.first.size());
  }
  return id;
}

// `sequence ds@ds::Checker::run / timeout` written in another module is the same behaviour
// as one written beside the class: its identity is the subject it resolves to (B1, B2)
void Checker::unifyBehaviours() {
  std::vector<std::pair<std::string, std::string>> moves;  // old id, new id
  for (const auto& r : model_.relations()) {
    if (r.kind != "behaviour-of") continue;
    const model::Entity* seq = model_.find(r.from);
    const model::Entity* subject = model_.find(r.to);
    if (!seq || !subject || subject->expected) continue;
    if (seq->kind != model::Kind::Sequence && seq->kind != model::Kind::State) continue;
    const std::string tail = seq->kind == model::Kind::State ? "::state" : "::sequence";
    const auto scenario = seq->fields.find("scenario");
    const std::string wanted = subject->id + tail + (scenario == seq->fields.end() ? "" : "/" + scenario->second);
    if (wanted != seq->id) moves.push_back({seq->id, wanted});
  }
  for (const auto& m : moves) {
    if (model_.rename(m.first, m.second)) renamed_[m.first] = m.second;
  }
}

void Checker::run(const bind::Binder& binder) {
  for (const auto& r : binder.references()) resolveReference(r);
  unifyBehaviours();
  for (const auto& r : binder.references()) checkTemplateBound(r);
  for (const auto& m : binder.messages()) {
    checkMessage(m);
    rememberResult(m);  // in order: a result exists for the messages after it
  }
  checkLocals();
  for (const auto& e : binder.events()) checkEvent(e);
  checkInterfaces();
  checkUseCases();
  checkDiagrams();
  checkScenarios();
  checkOwnershipCycles();
  classify();
  checkArtefacts();
  checkExtensions();
  checkFields();
  checkLanguages();
  checkRules();
}

// A rule makes a requirement real (REQ-00095-1): it answers to one, and says what holds or what must
// never happen, not both; a definition says what it ranges over. What they say is kept as text until
// the evaluator reads it.
void Checker::checkRules() {
  std::set<std::string> linked;
  for (const auto& r : model_.relations())
    if (r.kind == "requires") linked.insert(r.from);
  const auto said = [](const model::Entity& e, const char* key) {
    const auto it = e.fields.find(key);
    return it != e.fields.end() && !it->second.empty();
  };
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (e.expected) continue;
    if (e.kind == model::Kind::Rule) {
      if (!linked.count(e.id))
        model_.report({"W-RULE-NO-REQUIREMENT", "warning", e.id + " answers to no requirement: write @requirement <ID>", e.location});
      if (said(e, "holds") == said(e, "never"))
        model_.report(
            {"W-RULE-NO-CLAIM", "warning",
             e.id + " says " +
                 (said(e, "holds") ? "both what holds and what must never happen" : "neither what holds nor what must never happen"),
             e.location});
    } else if (e.kind == model::Kind::Definition && !said(e, "for")) {
      model_.report(
          {"W-DEFINITION-NO-RANGE", "warning", e.id + " says nothing about what it ranges over: write for: every <kind>", e.location});
    }
  }
}

// A package or a module says what its code is written in, or holds the language of the box around
// it: the field is filled down, so a reader of the model finds it on every package and module. A
// language ds does not know, or two at once (two declarations of a reopened box), is a finding.
void Checker::checkLanguages() {
  static const std::set<std::string> kKnown{"cpp", "c", "python", "typescript", "lua"};
  std::map<std::string, std::string> parent;
  for (const auto& r : model_.relations()) {
    const model::Entity* from = model_.find(r.from);
    const model::Entity* to = model_.find(r.to);
    if (r.kind == "contains" && from && to && (to->kind == model::Kind::Package || to->kind == model::Kind::Module)) parent[r.to] = r.from;
  }
  std::vector<std::string> boxes;
  for (const auto& pair : model_.entities())
    if (pair.second.kind == model::Kind::Package || pair.second.kind == model::Kind::Module) boxes.push_back(pair.first);
  std::map<std::string, std::string> own;
  for (const auto& id : boxes) {
    model::Entity& e = *model_.find(id);
    const auto written = e.fields.find("language");
    if (written == e.fields.end() || written->second.empty()) continue;
    std::set<std::string> said;
    for (const auto& item : split(written->second, ',')) said.insert(item);
    const std::string first = *said.begin();
    if (said.size() > 1) {
      model_.report(
          {"W-LANGUAGE-UNKNOWN", "warning", e.id + " says two languages (" + written->second + "): one box, one language", e.location});
    } else if (!kKnown.count(first)) {
      model_.report({"W-LANGUAGE-UNKNOWN", "warning",
                     e.id + " is written in '" + first + "', which is none of cpp, c, python, typescript, lua", e.location});
    }
    own[e.id] = first;
  }
  for (const auto& id : boxes) {
    model::Entity& e = *model_.find(id);
    std::set<std::string> seen;
    for (std::string at = e.id; !at.empty() && seen.insert(at).second; at = parent.count(at) ? parent.at(at) : "") {
      const auto it = own.find(at);
      if (it == own.end()) continue;
      e.fields["language"] = it->second;
      break;
    }
  }
}

// a field its kind does not declare in the metamodel is a typo or a field the language does not know
void Checker::checkFields() {
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    for (const auto& f : e.fields) {
      if (model::fieldOf(e.kind, f.first)) continue;
      model_.report({"W-FIELD-UNKNOWN", "warning",
                     std::string(model::kindName(e.kind)) + " '" + e.id + "' has a field '" + f.first + "' that a " +
                         model::kindName(e.kind) + " does not declare",
                     e.location});
    }
  }
}

// approved requirements are verified; a test verification names its tests
void Checker::checkExtensions() {
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (e.expected && e.has("extension") && reported_.insert(e.id).second) {
      model_.report(
          {"W-REF-UNDECLARED", "warning",
           std::string(model::kindName(e.kind)) + " '" + e.id + "' is extended with annotations but defined nowhere in the workspace",
           e.location});
    }
  }
}

void Checker::checkArtefacts() {
  std::set<std::string> verified;
  for (const auto& r : model_.relations()) {
    if (r.kind == "verifies") verified.insert(r.to);
  }
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (e.expected) continue;
    const auto status = e.fields.find("status");
    if (e.kind == model::Kind::Requirement && status != e.fields.end() && status->second == "approved" && !verified.count(e.id)) {
      model_.report({"W-REQ-NO-VERIFICATION", "warning", e.id + " is approved and nothing verifies it", e.location});
    }
    const auto method = e.fields.find("method");
    if (e.kind == model::Kind::Verification && method != e.fields.end() && method->second == "test") {
      const auto tests = e.fields.find("tests");
      if (tests == e.fields.end() || tests->second.empty())
        model_.report({"W-VER-NO-TEST", "warning", e.id + " verifies by test but names no tests", e.location});
    }
  }
}

void Checker::resolveReference(const bind::Reference& reference) {
  const bool artefactRef = reference.expectedKind == model::Kind::Requirement || reference.expectedKind == model::Kind::Verification ||
                           reference.expectedKind == model::Kind::Constraint;
  if (artefactRef && looksLikeArtefact(reference.target)) {
    std::smatch m;
    if (!std::regex_match(reference.target, m, kArtefactWithRevision)) {
      // a link goes against a revision: an unpinned one says nothing about which text was meant
      model_.report({"E-REF-UNPINNED", "error",
                     reference.from + " refers to " + reference.target + " without a revision; write " + reference.target + "-<revision>",
                     reference.location});
      return;
    }
    // the declared revision of that artefact, whatever it is
    const std::string bare = m[1].str();
    std::string declaredId;
    for (const auto& pair : model_.entities()) {
      if (!pair.second.expected && withoutRevision(pair.first) == bare) declaredId = pair.first;
    }
    if (!declaredId.empty() && declaredId != reference.target) {
      model_.report({"W-REVISION-DRIFT", "warning",
                     reference.from + " refers to " + reference.target + " but the workspace declares " + declaredId +
                         ": the link is behind the artefact",
                     reference.location});
      model_.relate({reference.relation, reference.from, declaredId, reference.location, reference.multiplicity});
      return;
    }
  }
  std::string target = resolve(reference.target, reference.scope);
  if (target.empty() && externsNamed(reference.target).size() > 1) {
    model_.report({"E-REF-AMBIGUOUS", "error",
                   reference.from + " refers to " + reference.target + ", which " + std::to_string(externsNamed(reference.target).size()) +
                       " extern modules declare: qualify it",
                   reference.location});
    return;
  }
  if (target.empty()) {
    // nobody declares it: an EXPECTED node, reported once (R6)
    model::Entity expected;
    expected.kind = reference.expectedKind;
    expected.name = reference.target;
    expected.scope = reference.scope;
    const bool artefact = reference.expectedKind == model::Kind::Requirement || reference.expectedKind == model::Kind::Verification ||
                          reference.expectedKind == model::Kind::Constraint;
    const std::string pkg = reference.scope.substr(
        0, reference.scope.rfind('@') == std::string::npos ? 0 : reference.scope.rfind('@') + 1);  // the innermost package
    if (artefact) {
      expected.id = withoutRevision(reference.target);
    } else if (reference.target.find('@') != std::string::npos) {
      expected.id = reference.target;  // absolute
    } else if (reference.target.find("::") != std::string::npos) {
      expected.id = pkg + reference.target;  // module-qualified, in this package
    } else {
      expected.id = qualifyIn(reference.scope, reference.target);  // bare, in this scope
    }
    expected.location = reference.location;
    expected.expected = true;
    model_.declare(expected);
    target = expected.id;
    if (reported_.insert(target).second) {
      model_.report({"W-REF-UNDECLARED", "warning",
                     std::string(model::kindName(expected.kind)) + " '" + reference.target + "' is referenced by " + reference.from +
                         " but declared nowhere in the workspace",
                     reference.location});
    }
  }
  if (reference.relation == "part-of") {  // a package reopened by its path: the outer package contains it, as nesting would say
    model_.relate({"contains", target, reference.from, reference.location, ""});
    return;
  }
  if (!kindAllowed(reference, target)) return;
  model_.relate({reference.relation, reference.from, target, reference.location, reference.multiplicity});
  const std::string& r = reference.relation;
  if (r == "uses" || r == "owns" || r == "refs" || r == "shares" || r == "inherits" || r == "bounded-by") {
    checkVisibility(reference.from, target, reference.location);
    checkLeak(reference.from, target, reference.location);
  }
}

// A reference binds only into a relation the language declares between those two kinds (REQ-00066-1):
// a class that names an interface as its base does not inherit from it, it realises it, and is told so.
bool Checker::kindAllowed(const bind::Reference& reference, const std::string& target) {
  const model::RelationSpec* spec = model::relationOf(reference.relation);
  const model::Entity* from = model_.find(reference.from);
  const model::Entity* to = model_.find(target);
  if (!spec || !from || !to || to->expected || model::joins(*spec, from->kind, to->kind)) return true;
  const std::string kind = model::kindName(to->kind);
  const std::string article = std::string("aeiou").find(kind[0]) == std::string::npos ? ", a " : ", an ";
  std::string message = reference.from + " " + reference.relation + " " + target + article + kind + ": the language joins " +
                        reference.relation + " only between other kinds";
  if (reference.relation == "inherits" && to->kind == model::Kind::Interface)
    message =
        reference.from + " names the interface " + target + " as its base: a class realises an interface, write @interface " + to->name;
  model_.report({"E-RELATION-KIND", "error", message, reference.location});
  return false;
}

std::string Checker::privateOnTheWay(const std::string& from, const std::string& target) const {
  std::set<std::string> chain;
  for (std::string s = from; !s.empty(); s = outer(s)) chain.insert(s);
  // down from the first container they share: a child is seen from inside its parent, so siblings see each other
  for (std::string t = target; !t.empty() && !chain.count(t); t = outer(t)) {
    const model::Entity* e = model_.find(t);
    if (!e || e->has("extern")) return "";
    const auto visibility = e->fields.find("visibility");
    if (visibility != e->fields.end() && visibility->second == "-" && !chain.count(outer(t))) return t;
  }
  return "";
}

void Checker::checkVisibility(const std::string& from, const std::string& target, const model::Location& location) {
  const std::string hidden = privateOnTheWay(from, target);
  if (hidden.empty()) return;
  model_.report({"W-NOT-VISIBLE", "warning",
                 from + " reaches " + target + (hidden == target ? ", which is private" : " through " + hidden + ", which is private"),
                 location});
}

void Checker::checkLeak(const std::string& from, const std::string& target, const model::Location& location) {
  const model::Entity* type = model_.find(target);
  const model::Entity* member = model_.find(from);
  if (!type || !member || type->expected) return;
  if (member->kind != model::Kind::Method && member->kind != model::Kind::Attribute && member->kind != model::Kind::Function &&
      member->kind != model::Kind::Operation) {
    return;
  }
  const auto hidden = type->fields.find("visibility");
  if (hidden == type->fields.end() || hidden->second != "-") return;
  // the member shows the type to whoever can see the member: from outside the type's parent, if all is public on the way
  const std::string parent = outer(target);
  for (std::string s = from; s != parent; s = outer(s)) {
    if (s.empty()) return;  // the member lives elsewhere: that is W-NOT-VISIBLE's
    const model::Entity* e = model_.find(s);
    if (!e) return;
    const auto visibility = e->fields.find("visibility");
    if (visibility != e->fields.end() && visibility->second == "-") return;
  }
  model_.report({"W-PRIVATE-LEAK", "warning", from + " is public and shows " + target + ", which is private", location});
}

// Repository<Relation> against class Repository<T: Entity> (R15)
void Checker::checkTemplateBound(const bind::Reference& reference) {
  if (reference.args.empty()) return;
  const std::string templateId = resolve(reference.target, reference.scope);
  const model::Entity* tpl = templateId.empty() ? nullptr : model_.find(templateId);
  if (!tpl || tpl->expected) return;
  const std::vector<model::Param>& params = tpl->params;
  for (size_t i = 0; i < params.size() && i < reference.args.size(); ++i) {
    if (params[i].type.empty()) continue;  // a template parameter with no bound
    const std::string boundId = resolve(params[i].type, tpl->scope);
    const std::string argId = resolve(reference.args[i], reference.scope);
    const model::Entity* arg = argId.empty() ? nullptr : model_.find(argId);
    if (boundId.empty() || !arg || arg->expected) continue;  // waits for a declaration (R6)
    if (!derives(argId, boundId)) {
      model_.report({"E-TEMPLATE-BOUND", "error",
                     reference.args[i] + " does not satisfy " + params[i].name + ": " + params[i].type + " of " + templateId +
                         " (used by " + reference.from + ")",
                     reference.location});
    }
  }
}

std::string Checker::resolveTypeText(const std::string& text, const std::string& scope) const {
  std::string t = text;
  // containers hand over to their element (the value of a map)
  for (;;) {
    const auto lt = t.find('<');
    if (lt == std::string::npos) break;
    const std::string head = t.substr(0, lt);
    if (head == "list" || head == "set" || head == "optional") {
      t = t.substr(lt + 1, t.size() - lt - 2);
    } else if (head == "map") {
      const auto comma = t.find(',');
      t = comma == std::string::npos ? "" : t.substr(comma + 1, t.size() - comma - 2);
    } else {
      // a template use: identity is name + arity
      int arity = 1;
      for (char c : t.substr(lt)) arity += c == ',' ? 1 : 0;
      t = head + "<" + std::to_string(arity) + ">";
      break;
    }
    const auto b = t.find_first_not_of(' ');
    t = b == std::string::npos ? "" : t.substr(b, t.find_last_not_of(' ') - b + 1);
  }
  if (t.empty()) return "";
  return resolve(t, scope);
}

const model::Entity* Checker::subjectClass(const model::Entity& sequence, const model::Entity** subject) const {
  const auto field = sequence.fields.find("subject");
  if (field == sequence.fields.end()) return nullptr;
  const std::string id = resolve(field->second, sequence.scope);
  const model::Entity* s = id.empty() ? nullptr : model_.find(id);
  if (!s || s->expected) return nullptr;
  *subject = s;
  if (s->kind == model::Kind::Method) return model_.find(s->scope);
  if (s->kind == model::Kind::Class) return s;
  return nullptr;  // a function or an interface's operation has no class: its context is its parameters and its scope
}

void Checker::couple(const model::Entity& sequence, const model::Entity& type, bool transient) {
  const model::Entity* subject = nullptr;
  const model::Entity* cls = subjectClass(sequence, &subject);
  const bool function = subject && (subject->kind == model::Kind::Function || subject->kind == model::Kind::Operation);
  if ((!cls && !function) || type.expected || type.has("extern")) return;
  const std::string home = cls ? cls->scope : subject->scope;
  if (type.scope == home || (cls && type.id == cls->id)) return;  // inside the module: cohesion, not coupling
  const std::string kind = transient ? "couples-transient" : "couples-structural";
  for (const auto& r : model_.relations()) {
    if (r.kind == kind && r.from == sequence.id && r.to == type.id) return;
  }
  model_.relate({kind, sequence.id, type.id, sequence.location, ""});
}

void Checker::call(model::Entity& msg, const model::Entity& op, const bind::Message& message) {
  model_.relate({"calls", msg.id, op.id, message.location, ""});
  checkVisibility(msg.scope, op.id, message.location);
  const int expected = static_cast<int>(op.params.size());
  if (expected != message.argc) {
    model_.report({"E-MSG-ARITY", "error",
                   op.id + " takes " + std::to_string(expected) + " parameter" + (expected == 1 ? "" : "s") + ", " +
                       std::to_string(message.argc) + " given",
                   message.location});
  }
  if (expected == message.argc) checkArguments(msg, op, message);
  const auto returns = op.fields.find("returns");
  const auto result = msg.fields.find("result");
  if (returns != op.fields.end()) {
    if (result != msg.fields.end() && primitiveFamily(returns->second) == 4) {
      model_.report({"W-RESULT-NONE", "warning", op.id + " returns nothing, so '" + result->second + "' holds no value", message.location});
    }
    msg.fields["result_type"] = returns->second;
    const model::Entity* owner = op.kind == model::Kind::Method ? model_.find(op.scope) : nullptr;
    msg.fields["result_scope"] = owner ? owner->scope : op.scope;
  }
  expand(msg, op, message);
}

void Checker::expand(model::Entity& msg, const model::Entity& op, const bind::Message& message) {
  // a message to an operation with a behaviour expands to it (B5): derived, never written; a message
  // that names a scenario expands to that one, and one nobody declares is a finding
  const auto scenario = msg.fields.find("scenario");
  const bool named = scenario != msg.fields.end();
  const std::string id = op.id + "::sequence" + (named ? "/" + scenario->second : "");
  if (const model::Entity* child = model_.find(id)) {
    model_.relate({"expands-to", msg.id, child->id, message.location, ""});
    msg.fields["expands"] = child->id;
  } else if (named && model::hasScenario(model_, op.id + "::sequence", scenario->second)) {
    // a named branch of the one behaviour: the call enters that path of it
    model_.relate({"expands-to", msg.id, op.id + "::sequence", message.location, ""});
    msg.fields["expands"] = op.id + "::sequence";
    msg.fields["expands-path"] = scenario->second;
  } else if (named) {
    model_.report({"W-SCENARIO-UNDECLARED", "warning", op.id + " has no scenario '" + scenario->second + "'", message.location});
  }
}

void Checker::checkReturn(const bind::Message& message) {
  const model::Entity* seq = model_.find(current(message.sequence));
  const model::Entity* msg = model_.find(current(message.message));
  if (!seq || !msg) return;
  const model::Entity* subject = nullptr;
  const model::Entity* cls = subjectClass(*seq, &subject);
  if (!subject || subject->kind == model::Kind::Class || subject->kind == model::Kind::Operation) return;
  const auto returns = subject->fields.find("returns");
  if (returns == subject->fields.end()) return;  // declares no return: nothing to check against
  const std::string value = msg->fields.count("result") ? msg->fields.at("result") : "";
  if (value.empty()) {
    // a bare return ends a branch; it gives nothing back, which only an operation returning none may do at its end
    return;
  }
  const std::string home = cls ? cls->scope : subject->scope;
  std::string given;
  bool mismatch = false;
  if (value.front() == '"' || value == "true" || value == "false" || value.front() == '-' ||
      std::isdigit(static_cast<unsigned char>(value.front()))) {
    given = value.front() == '"' ? "string" : (value == "true" || value == "false") ? "bool" : "int";
    mismatch = !compatible({given, home}, {returns->second, home});
  } else {
    std::pair<std::string, std::string> named;
    bool transient = false;
    // a name the context does not know is a local of the prose: absence is not an error (B4)
    if (!typeOfName(*seq, cls, *subject, value, &named, &transient) || named.first.empty()) return;
    given = named.first;
    mismatch = !compatible(named, {returns->second, home});
  }
  if (mismatch) {
    model_.report(
        {"W-RETURN-TYPE", "warning", subject->id + " returns " + returns->second + ", and '" + value + "' is " + given, message.location});
  }
}

void Checker::checkMessage(const bind::Message& message) {
  if (message.returns) {
    checkReturn(message);
    return;
  }
  const model::Entity* seq = model_.find(current(message.sequence));
  model::Entity* msg = model_.find(current(message.message));
  if (!seq || !msg) return;
  const model::Entity* subject = nullptr;
  const model::Entity* cls = subjectClass(*seq, &subject);
  const bool function = subject && (subject->kind == model::Kind::Function || subject->kind == model::Kind::Operation);
  if (!cls && !function) return;  // the subject waits for its declaration (R6)
  const std::string home = cls ? cls->scope : subject->scope;
  // a call with no target from a function, or one the class does not declare, is a function of the
  // module, resolved outward like any name: a free function calls its neighbours (2026-10-26)
  if (message.to == "self" && (function || !model_.find(cls->id + "::" + message.op))) {
    const std::string id = resolve(message.op, home);
    const model::Entity* target = id.empty() ? nullptr : model_.find(id);
    if (target && target->kind == model::Kind::Function) {
      msg->fields["type"] = target->scope;
      couple(*seq, *target, true);  // a function of another module couples like a local
      call(*msg, *target, message);
      return;
    }
    if (function) {
      model_.report({"W-MSG-UNDECLARED", "warning", seq->name + " calls " + message.op + ", which no function of " + home + " declares",
                     message.location});
      return;
    }
  }
  // the participant: self, or a name of the behaviour's context (B3)
  std::string typeId;
  bool transient = false;
  if (message.to == "self") {
    typeId = cls->id;
  } else {
    std::pair<std::string, std::string> value;
    if (!typeOfName(*seq, cls, *subject, message.to, &value, &transient)) {
      const std::string context = cls ? "an attribute of " + cls->id + ", a parameter of " : "a parameter of ";
      model_.report({"W-PARTICIPANT-UNDECLARED", "warning",
                     "'" + message.to + "' is neither " + context + subject->name + ", a local of " + seq->name +
                         " nor the result of an earlier message",
                     message.location});
      return;
    }
    typeId = value.first.empty() ? "" : resolveTypeText(value.first, value.second);
  }
  if (typeId.empty()) return;  // a primitive or an unknown spelling: nothing to check
  const model::Entity* type = model_.find(typeId);
  if (!type || type->expected) return;  // reported once the type is declared (R6)
  msg->fields["type"] = typeId;
  if (message.to != "self") couple(*seq, *type, transient);
  const std::string opId = typeId + "::" + message.op;
  const model::Entity* op = model_.find(opId);
  if (!op) {
    model_.report(
        {"W-MSG-UNDECLARED", "warning", seq->name + " calls " + opId + ", which " + typeId + " does not declare", message.location});
    return;
  }
  call(*msg, *op, message);
}

bool Checker::typeOfName(const model::Entity& sequence, const model::Entity* cls, const model::Entity& subject, const std::string& name,
                         std::pair<std::string, std::string>* value, bool* transient) const {
  const std::string home = cls ? cls->scope : subject.scope;
  if (const model::Entity* attribute = cls ? model_.find(cls->id + "::" + name) : nullptr) {
    const auto type = attribute->fields.find("type");
    *value = {type == attribute->fields.end() ? "" : type->second, cls->scope};
    return true;
  }
  for (const auto& p : subject.params) {
    if (p.name != name) continue;
    *value = {p.type, home};
    return true;
  }
  const auto local = sequence.fields.find("local." + name);
  if (local != sequence.fields.end()) {
    *value = {local->second, sequence.scope};
    *transient = true;
    return true;
  }
  const auto results = results_.find(sequence.id);
  if (results != results_.end()) {
    const auto result = results->second.find(name);
    if (result != results->second.end()) {
      *value = result->second;
      *transient = true;
      return true;
    }
  }
  return false;
}

bool Checker::compatible(const std::pair<std::string, std::string>& argument, const std::pair<std::string, std::string>& parameter) const {
  if (argument.first == parameter.first) return true;
  const std::string argumentId = resolveTypeText(argument.first, argument.second);
  const std::string parameterId = resolveTypeText(parameter.first, parameter.second);
  const model::Entity* a = argumentId.empty() ? nullptr : model_.find(argumentId);
  const model::Entity* p = parameterId.empty() ? nullptr : model_.find(parameterId);
  // what is not declared, or lives outside the workspace, has no obligations (R6, R11)
  if ((a && (a->expected || a->has("extern"))) || (p && (p->expected || p->has("extern")))) return true;
  if (a && p) return derives(argumentId, parameterId);
  const int af = primitiveFamily(argument.first);
  const int pf = primitiveFamily(parameter.first);
  if (a || p) return !(a ? pf : af);
  return !af || !pf || af == pf;
}

void Checker::checkArguments(const model::Entity& msg, const model::Entity& op, const bind::Message& message) {
  if (op.params.empty() || msg.args.empty()) return;
  const model::Entity* seq = model_.find(current(message.sequence));
  const model::Entity* subject = nullptr;
  const model::Entity* cls = seq ? subjectClass(*seq, &subject) : nullptr;
  if (!subject) return;
  const std::vector<model::Param>& parameters = op.params;
  const std::vector<std::string>& arguments = msg.args;
  const model::Entity* owner = op.kind == model::Kind::Method ? model_.find(op.scope) : nullptr;
  const std::string scope = owner ? owner->scope : op.scope;
  for (size_t i = 0; i < parameters.size() && i < arguments.size(); ++i) {
    if (parameters[i].type.empty()) continue;  // a parameter with no type
    std::pair<std::string, std::string> argument;
    bool transient = false;
    const std::string& written = arguments[i];
    if (written.front() == '"') {
      argument = {"string", scope};  // a literal has the type it is written in
    } else if (std::isdigit(static_cast<unsigned char>(written.front())) || written.front() == '-') {
      argument = {"int", scope};
    } else if (written == "true" || written == "false") {
      argument = {"bool", scope};
    } else if (!typeOfName(*seq, cls, *subject, written, &argument, &transient) || argument.first.empty()) {
      continue;
    }
    const std::pair<std::string, std::string> parameter{parameters[i].type, scope};
    if (compatible(argument, parameter)) continue;
    model_.report({"W-ARG-TYPE", "warning",
                   op.id + " takes " + parameters[i].name + ": " + parameters[i].type + ", and '" + arguments[i] + "' is " + argument.first,
                   message.location});
  }
}

void Checker::rememberResult(const bind::Message& message) {
  if (message.returns) return;
  const model::Entity* seq = model_.find(current(message.sequence));
  const model::Entity* msg = model_.find(current(message.message));
  if (!seq || !msg) return;
  const auto result = msg->fields.find("result");
  if (result == msg->fields.end()) return;
  const auto type = msg->fields.find("result_type");
  const auto scope = msg->fields.find("result_scope");
  // a call that resolved to nothing leaves a value of unknown type: the name exists, the finding was given
  results_[seq->id][result->second] = {type == msg->fields.end() ? "" : type->second, scope == msg->fields.end() ? "" : scope->second};
}

std::string Checker::outcomeOf(const std::string& sequence, const std::string& path, std::set<std::string>* seen) const {
  if (!seen->insert(sequence + "/" + path).second) return "";
  const std::vector<const model::Entity*> messages = model::pathOf(model_, sequence, path, nullptr);
  std::string wanted;  // the name of the result the behaviour returns; empty: its last call decides
  for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
    const model::Entity& m = **it;
    const auto result = m.fields.find("result");
    if (m.fields.count("return")) {
      const std::string value = result == m.fields.end() ? "" : result->second;
      if (value.empty()) return "";
      if (!(std::isdigit(static_cast<unsigned char>(value.front())) || value.front() == '-')) {
        wanted = value;
        continue;
      }
      return value;
    }
    if (!wanted.empty() && (result == m.fields.end() || result->second != wanted)) continue;
    const auto expands = m.fields.find("expands");
    const auto into = m.fields.find("expands-path");
    return expands == m.fields.end() ? "" : outcomeOf(expands->second, into == m.fields.end() ? "" : into->second, seen);
  }
  return "";
}

// A use case is a contract (DS-77): what goes in, what comes out, the outcomes it ends in. A
// specific one keeps its base's signature and outcomes; an interface operation that realises it
// takes and returns the same and handles every outcome as a scenario of its behaviour; a scenario
// no outcome names is behaviour no use case asked for.
void Checker::checkUseCases() {
  const auto isUseCase = [this](const std::string& id) {
    const model::Entity* e = model_.find(id);
    return e && !e->expected && e->kind == model::Kind::UseCase;
  };
  std::map<std::string, std::string> base;
  std::map<std::string, std::vector<const model::Relation*>> realisedBy;
  for (const auto& r : model_.relations()) {
    const model::Entity* from = model_.find(r.from);
    if (!from) continue;
    if (r.kind == "inherits" && from->kind == model::Kind::UseCase) {
      if (isUseCase(r.to))
        base[r.from] = r.to;
      else
        model_.report(
            {"W-USECASE-UNDECLARED", "warning", r.from + " specialises '" + r.to + "', which is no use case of the workspace", r.location});
    } else if (r.kind == "realises" && from->kind == model::Kind::Operation) {
      if (isUseCase(r.to))
        realisedBy[r.to].push_back(&r);
      else
        model_.report(
            {"W-USECASE-UNDECLARED", "warning", r.from + " realises '" + r.to + "', which is no use case of the workspace", r.location});
    }
  }
  // the chain of a use case up to its most generic, never round a cycle
  const auto chain = [&base](const std::string& id) {
    std::vector<std::string> out{id};
    std::set<std::string> seen{id};
    for (auto it = base.find(id); it != base.end() && seen.insert(it->second).second; it = base.find(it->second)) out.push_back(it->second);
    return out;
  };
  const auto outcomes = [this, &chain](const std::string& id) {
    std::set<std::string> out;
    for (const auto& uc : chain(id))
      for (const auto& pair : model_.entities())
        if (pair.second.kind == model::Kind::Outcome && pair.second.scope == uc) out.insert(pair.second.name);
    return out;
  };
  const auto contract = [this, &chain](const std::string& id) -> const model::Entity* {  // the nearest that writes a signature
    for (const auto& uc : chain(id)) {
      const model::Entity* e = model_.find(uc);
      if (e && e->fields.count("operation")) return e;
    }
    return nullptr;
  };
  const auto signature = [](const model::Entity& e) {
    std::string out = "(";
    for (size_t i = 0; i < e.params.size(); ++i) out += (i ? ", " : "") + model::lastName(e.params[i].type);
    const auto returns = e.fields.find("returns");
    return out + ") -> " + (returns == e.fields.end() ? std::string("none") : model::lastName(returns->second));
  };
  for (const auto& pair : base) {  // a specific use case keeps the contract of its base
    const model::Entity* own = model_.find(pair.first);
    const model::Entity* inherited = contract(pair.second);
    if (own && inherited && own->fields.count("operation") && signature(*own) != signature(*inherited)) {
      model_.report(
          {"W-USECASE-SIGNATURE", "warning",
           pair.first + " specialises " + pair.second + " but takes and gives " + signature(*own) + ", not " + signature(*inherited),
           own->location});
    }
  }
  std::set<std::string> realised;
  for (const auto& pair : realisedBy) {
    for (const auto& uc : chain(pair.first)) realised.insert(uc);
    const model::Entity* promised = contract(pair.first);
    const std::set<std::string> ends = outcomes(pair.first);
    for (const model::Relation* r : pair.second) {
      const model::Entity* op = model_.find(r->from);
      if (promised && signature(*op) != signature(*promised)) {
        model_.report({"W-USECASE-SIGNATURE", "warning",
                       op->id + " realises " + pair.first + " but takes and gives " + signature(*op) + ", not " + signature(*promised),
                       r->location});
      }
      const std::string behaviour = op->id + "::sequence";
      for (const auto& end : ends) {
        if (end == "default" || model_.find(behaviour + "/" + end) || model::hasScenario(model_, behaviour, end)) continue;
        model_.report({"W-USECASE-OUTCOME-UNHANDLED", "warning",
                       op->id + " realises " + pair.first + " and has no scenario for its outcome '" + end + "'", r->location});
      }
      for (const auto& other : model_.entities()) {  // a scenario written apart that no outcome names
        const model::Entity& s = other.second;
        if (s.kind != model::Kind::Sequence || s.id.rfind(behaviour + "/", 0) != 0) continue;
        const std::string scenario = s.id.substr(behaviour.size() + 1);
        if (!ends.count(scenario)) {
          model_.report({"W-SCENARIO-NO-OUTCOME", "warning",
                         op->id + " handles '" + scenario + "', and " + pair.first + " names no such outcome", s.location});
        }
      }
    }
  }
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (e.kind != model::Kind::UseCase || e.expected || realised.count(e.id)) continue;
    model_.report({"W-USECASE-UNREALISED", "warning",
                   e.id + " is realised by no interface operation, nor is any use case that specialises it", e.location});
  }
}

void Checker::checkInterfaces() {
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (e.kind == model::Kind::State) {
      const auto cut = e.id.rfind("::state");
      const model::Entity* subject = cut == std::string::npos ? nullptr : model_.find(e.id.substr(0, cut));
      if (subject && subject->kind == model::Kind::Interface) checkViews(e);
    }
    if (e.kind != model::Kind::Operation) continue;
    const std::string behaviour = e.id + "::sequence";
    if (!model_.find(behaviour)) {
      model_.report({"W-OPERATION-NO-BEHAVIOUR", "warning", e.id + " has no sequence that says how it is handled", e.location});
    }
    for (const auto& field : e.fields) {
      if (field.first.rfind("exit ", 0) != 0) continue;
      const std::string code = field.first.substr(5);
      std::string id = field.second == "default" ? behaviour : behaviour + "/" + field.second;
      std::string path;
      if (!model_.find(id) && model::hasScenario(model_, behaviour, field.second)) {
        id = behaviour;  // a named branch of the one behaviour
        path = field.second;
      }
      if (!model_.find(id)) {
        model_.report({"W-SCENARIO-UNDECLARED", "warning",
                       e.id + " exits " + code + " on '" + field.second + "', and it has no such scenario", e.location});
        continue;
      }
      std::set<std::string> seen;
      const std::string outcome = outcomeOf(id, path, &seen);
      if (!outcome.empty() && outcome != code) {
        model_.report({"W-OUTCOME-MISMATCH", "warning",
                       e.id + " promises exit " + code + " on '" + field.second + "', and that trace ends in " + outcome, e.location});
      }
    }
  }
}

void Checker::checkViews(const model::Entity& state) {
  std::set<std::string> views;
  for (const auto& pair : model_.entities()) {
    if (pair.second.kind == model::Kind::View && pair.second.scope == state.id) views.insert(pair.second.name);
  }
  if (views.empty()) return;  // the details of a view are optional: a bare machine asks nothing of them
  std::map<std::string, std::set<std::string>> next;
  const auto moves = state.fields.find("transitions");
  if (moves != state.fields.end()) {
    for (const auto& move : split(moves->second, '\n')) {
      const auto gt = move.find('>');
      const std::string from = move.substr(0, gt);
      const std::string to = move.substr(gt + 1, move.find('>', gt + 1) - gt - 1);
      for (const std::string& name : {from, to}) {
        if (!views.count(name)) {
          model_.report({"W-VIEW-UNDECLARED", "warning",
                         "the navigation of " + state.name + " names '" + name + "', which is not one of its views", state.location});
        }
      }
      if (from != to) next[from].insert(to);  // staying where you are is no way in or out
    }
  }
  const auto initial = state.fields.find("initial");
  std::set<std::string> reached;
  std::vector<std::string> pending;
  if (initial != state.fields.end()) pending.push_back(initial->second);
  while (!pending.empty()) {
    const std::string view = pending.back();
    pending.pop_back();
    if (!reached.insert(view).second) continue;
    for (const auto& to : next[view]) pending.push_back(to);
  }
  for (const auto& view : views) {
    const model::Entity* v = model_.find(state.id + "::" + view);
    if (!reached.count(view)) {
      model_.report({"W-VIEW-UNREACHABLE", "warning", "nothing leads to the view " + view + " of " + state.name, v->location});
    } else if (views.size() > 1 && next[view].empty()) {
      model_.report({"W-VIEW-NO-WAY-OUT", "warning", "nothing leaves the view " + view + " of " + state.name, v->location});
    }
  }
}

void Checker::checkScenarios() {
  // a scenario has one name: a branch of the behaviour, or a behaviour of its own, never both nor twice
  std::map<std::string, int> named;
  for (const auto& pair : model_.entities()) {
    const model::Entity& f = pair.second;
    if (f.kind != model::Kind::Frame) continue;
    for (const char* key : {"scenario", "else-scenario"}) {
      const auto name = f.fields.find(key);
      if (name == f.fields.end()) continue;
      const std::string base = f.scope.substr(0, f.scope.find("::sequence") + std::string("::sequence").size());
      if (++named[base + "/" + name->second] == 2 || model_.find(base + "/" + name->second)) {
        model_.report(
            {"W-SCENARIO-TWICE", "warning", base + " names the scenario '" + name->second + "' twice: a scenario is one path", f.location});
      }
    }
  }
}

void Checker::checkDiagrams() {
  for (const auto& pair : model_.entities()) {
    if (pair.second.kind != model::Kind::Diagram) continue;
    // a sequence diagram that asks for a scenario: it is a behaviour of its own, or a named branch of one
    const auto include = pair.second.fields.find("include");
    if (include != pair.second.fields.end() && pair.second.fields.count("kind") && pair.second.fields.at("kind") == "sequence") {
      for (const auto& pattern : split(include->second, ',')) {
        const auto slash = pattern.find(" / ");
        if (slash == std::string::npos) continue;
        const std::string behaviour = pattern.substr(0, slash) + "::sequence";
        const std::string scenario = pattern.substr(slash + 3);
        if (!model_.find(behaviour) || model_.find(behaviour + "/" + scenario) || model::hasScenario(model_, behaviour, scenario)) continue;
        model_.report({"W-SCENARIO-UNDECLARED", "warning", pair.second.name + " asks for " + pattern + ", a scenario nobody declares",
                       pair.second.location});
      }
    }
    const auto depth = pair.second.fields.find("depth");
    if (depth == pair.second.fields.end() || depth->second == "*") continue;
    if (depth->second.empty() || depth->second.find_first_not_of("0123456789") != std::string::npos) {
      model_.report({"W-DIAGRAM-DEPTH", "warning",
                     pair.second.name + " asks for depth '" + depth->second + "': a number, or * for every level", pair.second.location});
    }
  }
}

void Checker::checkLocals() {
  std::vector<std::pair<std::string, std::string>> locals;
  for (const auto& pair : model_.entities()) {
    if (pair.second.kind != model::Kind::Sequence) continue;
    for (const auto& f : pair.second.fields) {
      if (f.first.rfind("local.", 0) == 0) locals.push_back({pair.first, f.second});
    }
  }
  for (const auto& l : locals) {
    const model::Entity* seq = model_.find(l.first);
    const std::string typeId = resolveTypeText(l.second, seq->scope);
    const model::Entity* type = typeId.empty() ? nullptr : model_.find(typeId);
    if (type) couple(*seq, *type, true);
  }
}

void Checker::checkEvent(const bind::Event& event) {
  const std::string type = resolve(event.entity, event.scope);
  const model::Entity* entity = type.empty() ? nullptr : model_.find(type);
  if (!entity || entity->expected) return;
  const std::string op = type + "::" + event.event;
  if (const model::Entity* target = model_.find(op)) {
    model_.relate({event.action ? "does" : "on", current(event.state), target->id, event.location, ""});
    return;
  }
  if (entity->kind == model::Kind::Interface) return;  // a jump with nothing behind it is a label of the navigation
  if (event.action) {
    model_.report({"W-ACTION-UNDECLARED", "warning",
                   "state " + event.state + " runs '" + event.event + "', which is not an operation of " + type, event.location});
    return;
  }
  model_.report({"W-EVENT-UNDECLARED", "warning",
                 "state " + event.state + " reacts to '" + event.event + "', which is not an operation of " + type, event.location});
}

// A owns B and B owns A: nobody can create either (R16). Self-composition (a tree) is fine.
void Checker::checkOwnershipCycles() {
  struct Edge {
    std::string to;
    model::Location location;
  };
  std::map<std::string, std::vector<Edge>> owns;
  for (const auto& r : model_.relations()) {
    if (r.kind != "owns") continue;
    const model::Entity* attribute = model_.find(r.from);
    const model::Entity* part = model_.find(r.to);
    if (!attribute || !part || part->expected) continue;
    const std::string whole = attribute->scope;
    if (whole.empty() || whole == r.to) continue;
    owns[whole].push_back({r.to, r.location});
  }
  std::set<std::string> reportedCycles;
  std::vector<std::string> path;
  std::set<std::string> onPath;
  std::set<std::string> done;
  std::function<void(const std::string&)> walk = [&](const std::string& node) {
    path.push_back(node);
    onPath.insert(node);
    for (const auto& edge : owns[node]) {
      if (onPath.count(edge.to)) {
        std::vector<std::string> cycle(std::find(path.begin(), path.end(), edge.to), path.end());
        std::vector<std::string> key = cycle;
        std::sort(key.begin(), key.end());
        if (reportedCycles.insert(join(key, "|")).second) {
          model_.report({"E-OWNERSHIP-CYCLE", "error", "composition cycle: " + join(cycle, " owns ") + " owns " + edge.to, edge.location});
        }
      } else if (!done.count(edge.to)) {
        walk(edge.to);
      }
    }
    onPath.erase(node);
    path.pop_back();
    done.insert(node);
  };
  for (const auto& pair : owns) {
    if (!done.count(pair.first)) walk(pair.first);
  }
}

// interface / abstract / value and the operator families are derived, never written (R14, R17)
void Checker::classify() {
  std::vector<std::string> classes;
  for (const auto& pair : model_.entities()) {
    if (pair.second.kind == model::Kind::Class && !pair.second.expected) classes.push_back(pair.first);
  }
  for (const auto& id : classes) {
    int methods = 0, abstractMethods = 0, attributes = 0;
    for (const auto& pair : model_.entities()) {
      const model::Entity& m = pair.second;
      if (m.scope != id) continue;
      if (m.kind == model::Kind::Method && !m.has("constructor") && !m.has("static")) {
        ++methods;
        if (m.has("abstract")) ++abstractMethods;
      } else if (m.kind == model::Kind::Attribute && !m.has("static")) {
        ++attributes;
      }
    }
    model::Entity* cls = model_.find(id);
    std::vector<std::string> stereotypes;
    if (cls->has("extern")) stereotypes.push_back("extern");
    if (cls->has("dataclass")) stereotypes.push_back("value");
    if (methods > 0 && abstractMethods == methods && attributes == 0) {
      stereotypes.push_back("interface");
    } else if (abstractMethods > 0) {
      stereotypes.push_back("abstract");
    }
    const auto ops = cls->fields.find("operators");
    if (ops != cls->fields.end()) {
      for (const auto& item : split(ops->second, ',')) {
        if (item == "==" || item == "!=") {
          if (std::find(stereotypes.begin(), stereotypes.end(), "equatable") == stereotypes.end()) stereotypes.push_back("equatable");
        } else if (item == "ordered") {
          stereotypes.push_back("ordered");
        } else if (item == "hash") {
          stereotypes.push_back("hashable");
        } else if (item == "print") {
          stereotypes.push_back("printable");
        }
      }
    }
    if (!stereotypes.empty()) cls->fields["stereotypes"] = join(stereotypes, ", ");
  }
}

}  // namespace check
}  // namespace ds
