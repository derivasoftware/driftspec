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

std::string RuleCheck::name() const { return "rules"; }

std::vector<std::string> RuleCheck::needs() const {
  // today's order, kept so the output stays the same; relaxed when the derivations move (DS-138)
  return {"dependencies"};
}

void RuleCheck::run(Context& context) {
  context_ = &context;
  model_ = &context.model();
  resolver_ = &context.resolver();
  checkRules();
}

// A rule makes a requirement real (REQ-00095-2): it answers to one, and says what holds or what must
// never happen, once; a definition says in which cases it holds. Then every rule is evaluated over the
// model (REQ-00096-1).
void RuleCheck::checkRules() {
  std::set<std::string> linked;
  for (const auto& r : model_->relations())
    if (r.kind == "requires") linked.insert(r.from);
  const auto said = [](const model::Entity& e, const char* key) {
    const auto it = e.fields.find(key);
    return it != e.fields.end() && !it->second.empty();
  };
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.expected) continue;
    if (e.kind == model::Kind::Rule) {
      if (!linked.count(e.id))
        model_->report({"W-RULE-NO-REQUIREMENT", "warning", e.id + " answers to no requirement: write @requirement <ID>", e.location});
      const auto f = model_->formulas().find(e.id);
      const std::size_t claims = f == model_->formulas().end() ? 0 : f->second.claims.size();
      if (claims != 1)
        model_->report({"W-RULE-NO-CLAIM", "warning",
                        e.id + (claims ? " says more than one claim: write one holds or one never"
                                       : " says neither what holds nor what must never happen"),
                        e.location});
    } else if (e.kind == model::Kind::Definition && !said(e, "if")) {
      model_->report({"W-DEFINITION-NO-CASE", "warning", e.id + " says no case in which it holds: write if <literals>", e.location});
    }
  }
  Evaluator(*model_, *resolver_).run();
}

}  // namespace check
}  // namespace ds
