#include "driftspec/check/Checker.hpp"

#include <algorithm>
#include <functional>
#include <map>
#include <regex>
#include <sstream>
#include <vector>

namespace driftspec {
namespace check {

namespace {

// process artefact ids: PREFIX(-PART)*-NUMBER-REVISION, e.g. REQ-00001-2, DS-C-0001-0
const std::regex kArtefactWithRevision("^([A-Z]+(?:-[A-Z]+)*-[0-9]+)-([0-9]+)$");
const std::regex kArtefactBare("^[A-Z]+(?:-[A-Z]+)*-[0-9]+$");

std::string withoutRevision(const std::string& id) {
  std::smatch m;
  return std::regex_match(id, m, kArtefactWithRevision) ? m[1].str() : id;
}

bool looksLikeArtefact(const std::string& name) { return std::regex_match(name, kArtefactBare) || std::regex_match(name, kArtefactWithRevision); }

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

/// The scope outside `scope`: ds@ds::b → ds@ds → ds@ → "" (R10).
std::string outer(const std::string& scope) {
  const auto cut = scope.rfind("::");
  if (cut != std::string::npos) return scope.substr(0, cut);
  const auto at = scope.find('@');
  if (at != std::string::npos && at + 1 < scope.size()) return scope.substr(0, at + 1);
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
  return "";
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
  for (const auto& m : binder.messages()) checkMessage(m);
  checkLocals();
  for (const auto& e : binder.events()) checkEvent(e);
  checkOwnershipCycles();
  classify();
  checkArtefacts();
}

// approved requirements are verified; a test verification names its tests
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
      if (tests == e.fields.end() || tests->second.empty()) model_.report({"W-VER-NO-TEST", "warning", e.id + " verifies by test but names no tests", e.location});
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
      model_.report({"E-REF-UNPINNED", "error", reference.from + " refers to " + reference.target + " without a revision; write " + reference.target + "-<revision>",
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
                     reference.from + " refers to " + reference.target + " but the workspace declares " + declaredId + ": the link is behind the artefact",
                     reference.location});
      model_.relate({reference.relation, reference.from, declaredId, reference.location, reference.multiplicity});
      return;
    }
  }
  std::string target = resolve(reference.target, reference.scope);
  if (target.empty()) {
    // nobody declares it: an EXPECTED node, reported once (R6)
    model::Entity expected;
    expected.kind = reference.expectedKind;
    expected.name = reference.target;
    expected.scope = reference.scope;
    const bool artefact = reference.expectedKind == model::Kind::Requirement || reference.expectedKind == model::Kind::Verification ||
                          reference.expectedKind == model::Kind::Constraint;
    const std::string pkg = reference.scope.substr(0, reference.scope.find('@') == std::string::npos ? 0 : reference.scope.find('@') + 1);
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
  model_.relate({reference.relation, reference.from, target, reference.location, reference.multiplicity});
}

// Repository<Relation> against class Repository<T: Entity> (R15)
void Checker::checkTemplateBound(const bind::Reference& reference) {
  if (reference.args.empty()) return;
  const std::string templateId = resolve(reference.target, reference.scope);
  const model::Entity* tpl = templateId.empty() ? nullptr : model_.find(templateId);
  if (!tpl || tpl->expected) return;
  const auto paramsField = tpl->fields.find("params");
  if (paramsField == tpl->fields.end()) return;
  const std::vector<std::string> params = split(paramsField->second, ',');
  for (size_t i = 0; i < params.size() && i < reference.args.size(); ++i) {
    const auto bound = tpl->fields.find("bound." + params[i]);
    if (bound == tpl->fields.end()) continue;
    const std::string boundId = resolve(bound->second, tpl->scope);
    const std::string argId = resolve(reference.args[i], reference.scope);
    const model::Entity* arg = argId.empty() ? nullptr : model_.find(argId);
    if (boundId.empty() || !arg || arg->expected) continue;  // waits for a declaration (R6)
    if (!derives(argId, boundId)) {
      model_.report({"E-TEMPLATE-BOUND", "error",
                     reference.args[i] + " does not satisfy " + params[i] + ": " + bound->second + " of " + templateId + " (used by " + reference.from + ")",
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
  return nullptr;
}

void Checker::couple(const model::Entity& sequence, const model::Entity& type, bool transient) {
  const model::Entity* subject = nullptr;
  const model::Entity* cls = subjectClass(sequence, &subject);
  if (!cls || type.expected || type.has("extern")) return;
  if (type.scope == cls->scope || type.id == cls->id) return;  // inside the module: cohesion, not coupling
  const std::string kind = transient ? "couples-transient" : "couples-structural";
  for (const auto& r : model_.relations()) {
    if (r.kind == kind && r.from == sequence.id && r.to == type.id) return;
  }
  model_.relate({kind, sequence.id, type.id, sequence.location, ""});
}

void Checker::checkMessage(const bind::Message& message) {
  const model::Entity* seq = model_.find(current(message.sequence));
  model::Entity* msg = model_.find(current(message.message));
  if (!seq || !msg) return;
  const model::Entity* subject = nullptr;
  const model::Entity* cls = subjectClass(*seq, &subject);
  if (!cls) return;  // the subject waits for its declaration (R6)
  // the participant: self, an attribute of the class, a parameter of the method, or a local (B3)
  std::string typeId;
  bool transient = false;
  if (message.to == "self") {
    typeId = cls->id;
  } else if (const model::Entity* attribute = model_.find(cls->id + "::" + message.to)) {
    const auto type = attribute->fields.find("type");
    typeId = type == attribute->fields.end() ? "" : resolveTypeText(type->second, cls->scope);
  } else {
    bool found = false;
    if (subject->kind == model::Kind::Method) {
      const auto params = subject->fields.find("params");
      if (params != subject->fields.end()) {
        for (const auto& p : split(params->second, ',')) {
          const auto colon = p.find(':');
          if (colon == std::string::npos || p.substr(0, colon) != message.to) continue;
          typeId = resolveTypeText(p.substr(colon + 1).substr(p.substr(colon + 1).find_first_not_of(' ')), cls->scope);
          found = true;
        }
      }
    }
    if (!found) {
      const auto local = seq->fields.find("local." + message.to);
      if (local == seq->fields.end()) {
        model_.report({"W-PARTICIPANT-UNDECLARED", "warning",
                       "'" + message.to + "' is neither an attribute of " + cls->id + ", a parameter of " + subject->name + " nor a local of " + seq->name,
                       message.location});
        return;
      }
      typeId = resolveTypeText(local->second, seq->scope);
      transient = true;
    }
  }
  if (typeId.empty()) return;  // a primitive or an unknown spelling: nothing to check
  const model::Entity* type = model_.find(typeId);
  if (!type || type->expected) return;  // reported once the type is declared (R6)
  msg->fields["type"] = typeId;
  if (message.to != "self") couple(*seq, *type, transient);
  const std::string opId = typeId + "::" + message.op;
  const model::Entity* op = model_.find(opId);
  if (!op) {
    model_.report({"W-MSG-UNDECLARED", "warning", seq->name + " calls " + opId + ", which " + typeId + " does not declare", message.location});
    return;
  }
  model_.relate({"calls", msg->id, op->id, message.location, ""});
  const auto params = op->fields.find("params");
  const int expected = params == op->fields.end() || params->second.empty() ? 0 : static_cast<int>(split(params->second, ',').size());
  if (expected != message.argc) {
    model_.report({"E-MSG-ARITY", "error",
                   opId + " takes " + std::to_string(expected) + " parameter" + (expected == 1 ? "" : "s") + ", " + std::to_string(message.argc) + " given",
                   message.location});
  }
  // a message to an operation with a behaviour expands to it (B5): derived, never written
  if (const model::Entity* child = model_.find(opId + "::sequence")) {
    model_.relate({"expands-to", msg->id, child->id, message.location, ""});
    msg->fields["expands"] = child->id;
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
  if (event.action) {
    model_.report({"W-ACTION-UNDECLARED", "warning", "state " + event.state + " runs '" + event.event + "', which is not an operation of " + type, event.location});
    return;
  }
  model_.report({"W-EVENT-UNDECLARED", "warning", "state " + event.state + " reacts to '" + event.event + "', which is not an operation of " + type,
                 event.location});
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
}  // namespace driftspec
