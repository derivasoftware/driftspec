#include "driftspec/check/Checker.hpp"

#include <regex>

namespace driftspec {
namespace check {

namespace {

std::string withoutRevision(const std::string& id) {
  static const std::regex revision("^([A-Z]+-[0-9]+)-[0-9]+$");
  std::smatch m;
  return std::regex_match(id, m, revision) ? m[1].str() : id;
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

void Checker::run(const bind::Binder& binder) {
  for (const auto& r : binder.references()) resolveReference(r);
  for (const auto& m : binder.messages()) checkMessage(m);
  for (const auto& e : binder.events()) checkEvent(e);
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
                          reference.expectedKind == model::Kind::Boundary || reference.target.find("::") != std::string::npos;
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
  model_.relate({reference.relation, reference.from, target, reference.location});
}

void Checker::checkMessage(const bind::Message& message) {
  const std::string type = resolve(message.participantType, message.scope);
  const model::Entity* entity = type.empty() ? nullptr : model_.find(type);
  if (!entity || entity->expected) return;  // reported once the type is declared (R6)
  const std::string op = type + "::" + message.op;
  if (const model::Entity* target = model_.find(op)) {
    model_.relate({"calls", message.sequence, target->id, message.location});
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
    model_.relate({"on", event.state, target->id, event.location});
    return;
  }
  model_.report({"W-EVENT-UNDECLARED", "warning", "state " + event.state + " reacts to '" + event.event + "', which is not an operation of " + type,
                 event.location});
}

}  // namespace check
}  // namespace driftspec
