#include <set>
#include <string>
#include <utility>
#include <vector>

#include "Shared.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/Paths.hpp"
#include "ds/query/Filter.hpp"

namespace ds {
namespace check {

std::string WalkthroughCheck::name() const { return "walkthroughs"; }

std::vector<std::string> WalkthroughCheck::needs() const {
  return {"linker"};  // the steps' relations to what they name, which the linker makes
}

WalkthroughCheck::WalkthroughCheck(const Context& context)
    : context_(&context), model_(&context.model()), metamodel_(&context.metamodel()), resolver_(&context.resolver()) {}

std::vector<model::Finding> WalkthroughCheck::check() {
  found_.clear();
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.kind == model::Kind::Diagram && e.field("kind") == "walkthrough") {
      bool any = false;
      for (const auto& r : model_->index().fromOfKind(e.id, "contains")) any = any || model_->find(r.to);
      if (!any)
        found_.push_back(
            metamodel_->finding(model::Code::W_WALK_EMPTY, e.id + " has no step: write step <title> { ... }", e.location, e.id));
    }
    if (e.kind == model::Kind::Step) checkStep(e);
  }
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

void WalkthroughCheck::report(model::Code code, const model::Entity& step, const std::string& message) {
  // the step is the subject: a door attaches it to the step, and the step names its walkthrough (DS-426)
  found_.push_back(metamodel_->finding(code, step.id + " (" + step.name + ") " + message, step.location, step.id));
}

bool WalkthroughCheck::drawn(const std::string& id, const std::set<std::string>& shown) const {
  // a box is drawn when the diagram takes it; a member is drawn in the box that holds it: an operation in its
  // interface, a method, an attribute or an operator in its class. A box in a closed one is not drawn: its holder is
  if (shown.count(id)) return true;
  const model::Entity* e = model_->find(id);
  if (!e) return false;
  const bool member = e->kind == model::Kind::Method || e->kind == model::Kind::Attribute || e->kind == model::Kind::Operator ||
                      e->kind == model::Kind::Operation;
  if (!member) return false;
  std::string holder = e->scope;
  while (!holder.empty() && (holder.back() == '@' || holder.back() == ':')) holder.pop_back();
  return !holder.empty() && shown.count(holder);
}

void WalkthroughCheck::checkStep(const model::Entity& step) {
  const std::string show = step.field("show");
  if (show.empty()) report(model::Code::W_WALK_EMPTY, step, "shows no diagram: write show: <diagram>");
  if (step.field("text").empty()) report(model::Code::W_WALK_EMPTY, step, "says nothing: write text: <a sentence>");
  const model::Entity* diagram = show.empty() ? nullptr : model_->find(model::Identity::ofDiagram(show).text());
  if (!show.empty() && !diagram) report(model::Code::W_WALK_UNDECLARED, step, "shows " + show + ", which is no diagram of the workspace");
  // the scenario of the behaviour the shown diagram draws
  const std::string scenario = step.field("scenario");
  if (diagram && !scenario.empty()) {
    // a scenario is a named branch of the behaviour, or a sequence written apart for it (`sequence X / s`)
    const model::Identity subject(diagram->field("include"));
    const bool apart = model_->find(subject.sequence(scenario).text()) != nullptr;
    if (!apart && !model::Paths(*model_).hasScenario(subject.sequence().text(), scenario))
      found_.push_back(metamodel_->finding(model::Code::W_SCENARIO_UNDECLARED,
                                           step.id + " (" + step.name + ") shows " + show + " / " + scenario + ", and " +
                                               diagram->field("include") + " has no scenario '" + scenario + "'",
                                           step.location, step.id));
  }
  // what it lights, each named, and drawn by the diagram it shows: the diagram's own filter says what it draws
  std::set<std::string> shown;
  const bool filtered = diagram && diagram->field("kind") != "walkthrough";
  if (filtered)
    if (const auto filter = query::FilterFactory::of(*metamodel_, *diagram)) shown = filter->select(*model_);
  std::set<std::string> lit;
  for (const auto& how : {std::pair<const char*, const char*>{"focus", "focuses"}, {"highlight", "highlights"}})
    for (const auto& written : splitTrimmed(step.field(how.first), ',')) {
      const std::string id = resolver_->resolve(written, "");
      if (id.empty() || !model_->find(id)) {
        report(model::Code::W_WALK_UNDECLARED, step, std::string(how.second) + " " + written + ", which nothing declares");
        continue;
      }
      lit.insert(id);
      if (filtered && !drawn(id, shown)) report(model::Code::W_WALK_NOT_SHOWN, step, "lights " + id + ", which " + show + " does not draw");
    }
  const std::string at = step.field("at");
  if (!at.empty()) {
    const std::string id = resolver_->resolve(at, "");
    if (id.empty() || !model_->find(id))
      report(model::Code::W_WALK_UNDECLARED, step, "points at " + at + ", which nothing declares");
    else if (!lit.count(id))
      report(model::Code::W_WALK_AT, step, "points at " + id + ", which it neither focuses nor highlights");
  }
}

}  // namespace check
}  // namespace ds
