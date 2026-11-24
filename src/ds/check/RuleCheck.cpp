#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
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

std::string RuleCheck::name() const { return "rules"; }

std::vector<std::string> RuleCheck::needs() const {
  // rules range over the finished model: relations, calls and derived fields
  return {"linker", "behaviour", "derivation"};
}

RuleCheck::RuleCheck(Context& context)
    : context_(&context), model_(&context.model()), metamodel_(&context.metamodel()), resolver_(&context.resolver()) {}

std::vector<model::Finding> RuleCheck::run() {
  checkRules();
  return context_->take();
}

// A rule makes a requirement real (REQ-00095-2): it answers to one, and says what holds or what must
// never happen, once; a definition says in which cases it holds. Then every rule is evaluated over the
// model (REQ-00096-1).
void RuleCheck::checkRules() {
  std::set<std::string> linked;
  for (const auto& r : model_->index().ofKind("satisfies")) linked.insert(r.from);
  const auto said = [](const model::Entity& e, const char* key) {
    const auto it = e.fields.find(key);
    return it != e.fields.end() && !it->second.empty();
  };
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.expected) continue;
    if (e.kind == model::Kind::Rule) {
      if (!linked.count(e.id))
        context_->report(context_->metamodel().finding(model::Code::W_RULE_NO_REQUIREMENT,
                                                       e.id + " answers to no requirement: write @requirement <ID>", e.location));
      const auto f = model_->formulas().find(e.id);
      const std::size_t claims = f == model_->formulas().end() ? 0 : f->second.claims.size();
      if (claims != 1)
        context_->report(context_->metamodel().finding(model::Code::W_RULE_NO_CLAIM,
                                                       e.id + (claims ? " says more than one claim: write one holds or one never"
                                                                      : " says neither what holds nor what must never happen"),
                                                       e.location));
    } else if (e.kind == model::Kind::Definition && !said(e, "if")) {
      context_->report(context_->metamodel().finding(model::Code::W_DEFINITION_NO_CASE,
                                                     e.id + " says no case in which it holds: write if <literals>", e.location));
    }
  }
  for (const auto& f : Evaluator(*model_, *metamodel_, *resolver_).run()) context_->report(f);
}

}  // namespace check
}  // namespace ds
