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

std::string withoutRevision(const std::string& id) {
  static const std::regex revision("^([A-Z]+-[0-9]+)-[0-9]+$");
  std::smatch m;
  return std::regex_match(id, m, revision) ? m[1].str() : id;
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

std::string Checker::resolve(const std::string& name, const std::string& scope) const {
  if (model_.find(name)) return name;
  const std::string bare = withoutRevision(name);
  if (bare != name && model_.find(bare)) return bare;
  std::string s = scope;
  while (!s.empty()) {
    const std::string candidate = s + "::" + name;
    if (model_.find(candidate)) return candidate;
    const auto cut = s.rfind("::");
    s = cut == std::string::npos ? "" : s.substr(0, cut);
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

void Checker::run(const bind::Binder& binder) {
  for (const auto& r : binder.references()) resolveReference(r);
  for (const auto& r : binder.references()) checkTemplateBound(r);
  for (const auto& m : binder.messages()) checkMessage(m);
  for (const auto& e : binder.events()) checkEvent(e);
  checkOwnershipCycles();
  classify();
}

void Checker::resolveReference(const bind::Reference& reference) {
  std::string target = resolve(reference.target, reference.scope);
  if (target.empty()) {
    // nobody declares it: an EXPECTED node, reported once (R6)
    model::Entity expected;
    expected.kind = reference.expectedKind;
    expected.name = reference.target;
    expected.scope = reference.scope;
    const bool artefact = reference.expectedKind == model::Kind::Requirement || reference.expectedKind == model::Kind::Verification ||
                          reference.expectedKind == model::Kind::Constraint || reference.expectedKind == model::Kind::Boundary ||
                          reference.target.find("::") != std::string::npos;
    expected.id = artefact ? withoutRevision(reference.target) : (reference.scope.empty() ? reference.target : reference.scope + "::" + reference.target);
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

void Checker::checkMessage(const bind::Message& message) {
  const std::string type = resolve(message.participantType, message.scope);
  const model::Entity* entity = type.empty() ? nullptr : model_.find(type);
  if (!entity || entity->expected) return;  // reported once the type is declared (R6)
  const std::string op = type + "::" + message.op;
  if (const model::Entity* target = model_.find(op)) {
    model_.relate({"calls", message.sequence, target->id, message.location, ""});
    return;
  }
  model_.report({"W-MSG-UNDECLARED", "warning", "sequence " + message.sequence + " calls " + op + ", which " + type + " does not declare",
                 message.location});
}

void Checker::checkEvent(const bind::Event& event) {
  const std::string type = resolve(event.entity, event.scope);
  const model::Entity* entity = type.empty() ? nullptr : model_.find(type);
  if (!entity || entity->expected) return;
  const std::string op = type + "::" + event.event;
  if (const model::Entity* target = model_.find(op)) {
    model_.relate({"on", event.state, target->id, event.location, ""});
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
