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
#include "ds/machine/Machine.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/Paths.hpp"

namespace ds {
namespace check {

std::string UseCaseContracts::name() const { return "contract-2-usecases"; }

std::vector<std::string> UseCaseContracts::needs() const {
  // what ContractCheck needed: the expansions behaviour typing made, over the links the linker made, and what
  // escapes each operation
  return {"linker", "behaviour", "failures"};
}

std::vector<model::Finding> UseCaseContracts::check(const Context& context) {
  context_ = &context;
  model_ = &context.model();
  metamodel_ = &context.metamodel();
  resolver_ = &context.resolver();
  found_.clear();
  checkUseCases();
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

// A use case is a contract (DS-77): what goes in, what comes out, the outcomes it ends in. A
// specific one keeps its base's signature and outcomes; an interface operation that realises it
// takes and returns the same and handles every outcome as a scenario of its behaviour; a scenario
// no outcome names is behaviour no use case asked for.
void UseCaseContracts::checkUseCases() {
  const model::Paths paths(*model_);
  const auto isUseCase = [this](const std::string& id) {
    const model::Entity* e = model_->find(id);
    return e && !e->expected && e->kind == model::Kind::UseCase;
  };
  std::map<std::string, std::string> base;
  std::map<std::string, std::vector<model::Relation>> realisedBy;  // copies: the index answers with copies
  for (const auto& r : model_->index().all()) {
    const model::Entity* from = model_->find(r.from);
    if (!from) continue;
    if (r.kind == "inherits" && from->kind == model::Kind::UseCase) {
      if (isUseCase(r.to))
        base[r.from] = r.to;
      else
        found_.push_back(context_->metamodel().finding(
            model::Code::W_USECASE_UNDECLARED, r.from + " specialises '" + r.to + "', which is no use case of the workspace", r.location));
    } else if (r.kind == "realises" && from->kind == model::Kind::Operation) {
      if (isUseCase(r.to))
        realisedBy[r.to].push_back(r);
      else
        found_.push_back(context_->metamodel().finding(
            model::Code::W_USECASE_UNDECLARED, r.from + " realises '" + r.to + "', which is no use case of the workspace", r.location));
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
      for (const auto& member : model_->index().members(uc)) {
        const model::Entity& held = *model_->find(member);
        if (held.kind == model::Kind::Outcome) out.insert(held.name);
      }
    return out;
  };
  const auto contract = [this, &chain](const std::string& id) -> const model::Entity* {  // the nearest that writes a signature
    for (const auto& uc : chain(id)) {
      const model::Entity* e = model_->find(uc);
      if (e && e->fields.count("operation")) return e;
    }
    return nullptr;
  };
  const auto signature = [](const model::Entity& e) {
    std::string out = "(";
    for (size_t i = 0; i < e.params.size(); ++i) out += (i ? ", " : "") + model::Identity(e.params[i].type).lastName();
    const auto returns = e.fields.find("returns");
    return out + ")" + (returns == e.fields.end() ? std::string() : " -> " + model::Identity(returns->second).lastName());
  };
  for (const auto& pair : base) {  // a specific use case keeps the contract of its base
    const model::Entity* own = model_->find(pair.first);
    const model::Entity* inherited = contract(pair.second);
    if (own && inherited && own->fields.count("operation") && signature(*own) != signature(*inherited)) {
      found_.push_back(context_->metamodel().finding(
          model::Code::W_USECASE_SIGNATURE,
          pair.first + " specialises " + pair.second + " but takes and gives " + signature(*own) + ", not " + signature(*inherited),
          own->location));
    }
  }
  std::set<std::string> realised;
  for (const auto& pair : realisedBy) {
    for (const auto& uc : chain(pair.first)) realised.insert(uc);
    const model::Entity* promised = contract(pair.first);
    const std::set<std::string> ends = outcomes(pair.first);
    for (const model::Relation& r : pair.second) {
      const model::Entity* op = model_->find(r.from);
      if (promised && signature(*op) != signature(*promised)) {
        found_.push_back(context_->metamodel().finding(
            model::Code::W_USECASE_SIGNATURE,
            op->id + " realises " + pair.first + " but takes and gives " + signature(*op) + ", not " + signature(*promised), r.location));
      }
      const std::string behaviour = model::Identity(op->id).sequence().text();
      for (const auto& end : ends) {
        if (end == "default" || model_->find(model::Identity(behaviour).withScenario(end).text()) || paths.hasScenario(behaviour, end))
          continue;
        found_.push_back(context_->metamodel().finding(
            model::Code::W_USECASE_OUTCOME_UNHANDLED,
            op->id + " realises " + pair.first + " and has no scenario for its outcome '" + end + "'", r.location));
      }
      for (const auto& other : model_->entities()) {  // a scenario written apart that no outcome names
        const model::Entity& s = other.second;
        if (s.kind != model::Kind::Sequence || model::Identity(model::Identity(s.id).subject().text()).sequence().text() != behaviour ||
            model::Identity(s.id).scenario().empty())
          continue;
        const std::string scenario = s.id.substr(behaviour.size() + 1);
        if (!ends.count(scenario)) {
          found_.push_back(context_->metamodel().finding(
              model::Code::W_SCENARIO_NO_OUTCOME, op->id + " handles '" + scenario + "', and " + pair.first + " names no such outcome",
              s.location));
        }
      }
    }
  }
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.kind != model::Kind::UseCase || e.expected || realised.count(e.id)) continue;
    found_.push_back(context_->metamodel().finding(model::Code::W_USECASE_UNREALISED,
                                                   e.id + " is realised by no interface operation, nor is any use case that specialises it",
                                                   e.location));
  }
}

}  // namespace check
}  // namespace ds
