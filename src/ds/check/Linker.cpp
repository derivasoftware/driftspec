#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

#include "Shared.hpp"
#include "ds/check/Evaluator.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

using model::outer;

std::string Linker::name() const { return "linker"; }

std::vector<std::string> Linker::needs() const { return {}; }

std::vector<model::Finding> Linker::run(Context& context) {
  context_ = &context;
  model_ = &context.model();
  resolver_ = &context.resolver();
  defineLoneExtensions();
  for (const auto& r : context_->binder().references()) resolveReference(r);
  unifyBehaviours();
  for (const auto& r : context_->binder().references()) checkTemplateBound(r);
  return context.take();
}

void Linker::defineLoneExtensions() {
  // a class body with only its requirements reads as an extension; when no other block of the
  // workspace defines that class, it is the class, empty, and its scope contains it (DS-20)
  std::vector<model::Entity> lone;
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.expected && e.kind == model::Kind::Class && e.has("extension")) lone.push_back(e);
  }
  for (model::Entity e : lone) {
    e.expected = false;
    e.flags.erase(std::remove(e.flags.begin(), e.flags.end(), "extension"), e.flags.end());
    model_->declare(e);
    if (model_->find(e.scope)) model_->relate({"contains", e.scope, e.id, e.location, ""});
  }
}

void Linker::resolveReference(const bind::Reference& reference) {
  const bool artefactRef = model::isArtefact(reference.expectedKind);
  if (artefactRef && model::isArtefactName(reference.target)) {
    if (!model::hasRevision(reference.target)) {
      // a link goes against a revision: an unpinned one says nothing about which text was meant
      context_->report(
          {"E-REF-UNPINNED", "error",
           reference.from + " refers to " + reference.target + " without a revision; write " + reference.target + "-<revision>",
           reference.location});
      return;
    }
    // the declared revision of that artefact, whatever it is
    const std::string declaredId = model_->index().revisionOf(reference.target);
    if (!declaredId.empty() && declaredId != reference.target) {
      context_->report({"W-REVISION-DRIFT", "warning",
                        reference.from + " refers to " + reference.target + " but the workspace declares " + declaredId +
                            ": the link is behind the artefact",
                        reference.location});
      model_->relate({reference.relation, reference.from, declaredId, reference.location, reference.multiplicity});
      return;
    }
  }
  std::string target = resolver_->resolve(reference.target, reference.scope);
  if (target.empty() && resolver_->externsNamed(reference.target).size() > 1) {
    context_->report({"E-REF-AMBIGUOUS", "error",
                      reference.from + " refers to " + reference.target + ", which " +
                          std::to_string(resolver_->externsNamed(reference.target).size()) + " extern modules declare: qualify it",
                      reference.location});
    return;
  }
  if (target.empty()) {
    // nobody declares it: an EXPECTED node, reported once (R6)
    model::Entity expected;
    expected.kind = reference.expectedKind;
    expected.name = reference.target;
    expected.scope = reference.scope;
    const bool artefact = model::isArtefact(reference.expectedKind);
    const std::string pkg = model::packageOf(reference.scope);  // the innermost package
    if (artefact) {
      expected.id = model::withoutRevision(reference.target);
    } else if (model::isAbsolute(reference.target)) {
      expected.id = reference.target;  // absolute
    } else if (reference.target.find("::") != std::string::npos) {
      expected.id = pkg + reference.target;  // module-qualified, in this package
    } else {
      expected.id = model::qualify(reference.scope, reference.target);  // bare, in this scope
    }
    expected.location = reference.location;
    expected.expected = true;
    model_->declare(expected);
    target = expected.id;
    // a use case nobody declares is said once, by the use case check (W-USECASE-UNDECLARED), as before its
    // links were resolved here (DS-116)
    if (reference.expectedKind != model::Kind::UseCase && context_->reported().insert(target).second) {
      context_->report({"W-REF-UNDECLARED", "warning",
                        std::string(model::kindName(expected.kind)) + " '" + reference.target + "' is referenced by " + reference.from +
                            " but declared nowhere in the workspace",
                        reference.location});
    }
  }
  if (reference.relation == "part-of") {  // a package reopened by its path: the outer package contains it, as nesting would say
    model_->relate({"contains", target, reference.from, reference.location, ""});
    return;
  }
  if (!kindAllowed(reference, target)) return;
  model_->relate({reference.relation, reference.from, target, reference.location, reference.multiplicity});
  const std::string& r = reference.relation;
  if (r == "uses" || r == "owns" || r == "refs" || r == "shares" || r == "inherits" || r == "bounded-by") {
    context_->checkVisibility(reference.from, target, reference.location);
    context_->checkLeak(reference.from, target, reference.location);
  }
}

// A reference binds only into a relation the language declares between those two kinds (REQ-00066-1):
// a class that names an interface as its base does not inherit from it, it realises it, and is told so.
bool Linker::kindAllowed(const bind::Reference& reference, const std::string& target) {
  const model::RelationSpec* spec = model::relationOf(reference.relation);
  const model::Entity* from = model_->find(reference.from);
  const model::Entity* to = model_->find(target);
  if (!spec || !from || !to || to->expected || model::joins(*spec, from->kind, to->kind)) return true;
  const std::string kind = model::kindName(to->kind);
  const std::string article = std::string("aeiou").find(kind[0]) == std::string::npos ? ", a " : ", an ";
  std::string message = reference.from + " " + reference.relation + " " + target + article + kind + ": the language joins " +
                        reference.relation + " only between other kinds";
  if (reference.relation == "inherits" && to->kind == model::Kind::Interface)
    message =
        reference.from + " names the interface " + target + " as its base: a class realises an interface, write @interface " + to->name;
  context_->report({"E-RELATION-KIND", "error", message, reference.location});
  return false;
}

// Repository<Relation> against class Repository<T: Entity> (R15)
void Linker::checkTemplateBound(const bind::Reference& reference) {
  if (reference.args.empty()) return;
  const std::string templateId = resolver_->resolve(reference.target, reference.scope);
  const model::Entity* tpl = templateId.empty() ? nullptr : model_->find(templateId);
  if (!tpl || tpl->expected) return;
  const std::vector<model::Param>& params = tpl->params;
  for (size_t i = 0; i < params.size() && i < reference.args.size(); ++i) {
    if (params[i].type.empty()) continue;  // a template parameter with no bound
    const std::string boundId = resolver_->resolve(params[i].type, tpl->scope);
    const std::string argId = resolver_->resolve(reference.args[i], reference.scope);
    const model::Entity* arg = argId.empty() ? nullptr : model_->find(argId);
    if (boundId.empty() || !arg || arg->expected) continue;  // waits for a declaration (R6)
    if (!resolver_->derives(argId, boundId)) {
      context_->report({"E-TEMPLATE-BOUND", "error",
                        reference.args[i] + " does not satisfy " + params[i].name + ": " + params[i].type + " of " + templateId +
                            " (used by " + reference.from + ")",
                        reference.location});
    }
  }
}

// `sequence ds@ds::Checker::run / timeout` written in another module is the same behaviour
// as one written beside the class: its identity is the subject it resolves to (B1, B2)
void Linker::unifyBehaviours() {
  std::vector<std::pair<std::string, std::string>> moves;  // old id, new id
  for (const auto& r : model_->index().ofKind("behaviour-of")) {
    const model::Entity* seq = model_->find(r.from);
    const model::Entity* subject = model_->find(r.to);
    if (!seq || !subject || subject->expected) continue;
    if (seq->kind != model::Kind::Sequence && seq->kind != model::Kind::State) continue;
    const auto scenario = seq->fields.find("scenario");
    const std::string wanted = seq->kind == model::Kind::State
                                   ? model::stateId(subject->id)
                                   : model::behaviourId(subject->id, scenario == seq->fields.end() ? "" : scenario->second);
    if (wanted != seq->id) moves.push_back({seq->id, wanted});
  }
  for (const auto& m : moves) {
    if (model_->rename(m.first, m.second)) context_->renamed()[m.first] = m.second;
  }
}

}  // namespace check
}  // namespace ds
