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

std::string Derivation::name() const { return "derivation"; }

std::vector<std::string> Derivation::needs() const {
  // the members and boxes the linker settled; nothing it derives is read by the steps before it
  return {"linker"};
}

std::vector<model::Finding> Derivation::run(Context& out_context) {
  context_ = &out_context;
  model_ = &out_context.model();
  resolver_ = &out_context.resolver();
  classify();
  fillLanguages();
  return out_context.take();
}

// abstract / value and the operator families are derived, never written (R14, R17)
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
    if (cls->has("dataclass")) stereotypes.push_back("value");
    if (abstractMethods > 0) stereotypes.push_back("abstract");  // all of them or some: an abstract class (DS-301)
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

// A package or a module says what its code is written in, or holds the language of the box around
// it: the field is filled down, so a reader of the model finds it on every package and module. A
// language ds does not know, or two at once (two declarations of a reopened box), is a finding.
void Derivation::fillLanguages() {
  static const std::set<std::string> kKnown{"cpp", "c", "python", "typescript", "lua"};
  std::map<std::string, std::string> parent;
  for (const auto& r : model_->index().ofKind("contains")) {
    const model::Entity* from = model_->find(r.from);
    const model::Entity* to = model_->find(r.to);
    if (from && to && (to->kind == model::Kind::Package || to->kind == model::Kind::Module)) parent[r.to] = r.from;
  }
  std::vector<std::string> boxes;
  for (const auto& pair : model_->entities())
    if (pair.second.kind == model::Kind::Package || pair.second.kind == model::Kind::Module) boxes.push_back(pair.first);
  std::map<std::string, std::string> own;
  for (const auto& id : boxes) {
    model::Entity& e = *model_->find(id);
    const auto written = e.fields.find("language");
    if (written == e.fields.end() || written->second.empty()) continue;
    std::set<std::string> said;
    for (const auto& item : split(written->second, ',')) said.insert(item);
    const std::string first = *said.begin();
    if (said.size() > 1) {
      context_->report(
          {"W-LANGUAGE-UNKNOWN", "warning", e.id + " says two languages (" + written->second + "): one box, one language", e.location});
    } else if (!kKnown.count(first)) {
      context_->report({"W-LANGUAGE-UNKNOWN", "warning",
                        e.id + " is written in '" + first + "', which is none of cpp, c, python, typescript, lua", e.location});
    }
    own[e.id] = first;
  }
  for (const auto& id : boxes) {
    model::Entity& e = *model_->find(id);
    std::set<std::string> seen;
    for (std::string at = e.id; !at.empty() && seen.insert(at).second; at = parent.count(at) ? parent.at(at) : "") {
      const auto it = own.find(at);
      if (it == own.end()) continue;
      e.fields["language"] = it->second;
      break;
    }
  }
}

}  // namespace check
}  // namespace ds
