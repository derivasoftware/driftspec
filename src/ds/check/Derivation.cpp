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

std::string Derivation::name() const { return "derivation"; }

std::vector<std::string> Derivation::needs() const {
  // today's order, kept so the output stays the same; relaxed when the derivations move (DS-138)
  return {"contracts"};
}

void Derivation::run(Context& context) {
  context_ = &context;
  model_ = &context.model();
  resolver_ = &context.resolver();
  classify();
}

// interface / abstract / value and the operator families are derived, never written (R14, R17)
void Derivation::classify() {
  std::vector<std::string> classes;
  for (const auto& pair : model_->entities()) {
    if (pair.second.kind == model::Kind::Class && !pair.second.expected) classes.push_back(pair.first);
  }
  for (const auto& id : classes) {
    int methods = 0, abstractMethods = 0, attributes = 0;
    for (const auto& pair : model_->entities()) {
      const model::Entity& m = pair.second;
      if (m.scope != id) continue;
      // a constructor, a destructor and a static method are no part of the contract (DS-143)
      if (m.kind == model::Kind::Method && !m.has("constructor") && !m.has("destructor") && !m.has("static")) {
        ++methods;
        if (m.has("abstract")) ++abstractMethods;
      } else if (m.kind == model::Kind::Attribute && !m.has("static")) {
        ++attributes;
      }
    }
    model::Entity* cls = model_->find(id);
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
