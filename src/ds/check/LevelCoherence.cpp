#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/axis/Cut.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/Paths.hpp"

namespace ds {
namespace check {

namespace {

/// One message that crosses from a participant of the scenario to another.
struct Crossing {
  std::string from;
  std::string to;
  std::string op;     ///< the operation it calls; empty for a label
  std::string label;  ///< the operation as written
  model::Location location;
};

/// What the level below a scenario does, seen from the scenario's participants.
struct Below {
  std::vector<Crossing> crossings;  ///< in the order they happen
  std::set<std::string> entered;    ///< the participants something is seen to happen in
  std::set<std::string> open;       ///< those in which a call is made whose behaviour is not written
};

std::string calleeOf(const model::Model& model, const std::string& message) {
  const auto calls = model.index().fromOfKind(message, "calls");
  return calls.empty() ? "" : calls.front().to;
}

/// The behaviour a scenario is asked through at one of its entries: the scenario of the same name of the
/// operation, when it has one (an outcome and the path that ends in it), or the one the call enters.
std::pair<std::string, std::string> behaviourBelow(const model::Model& model, const model::Entity& scenario, const model::Entity& entry) {
  const std::string named = scenario.field("scenario");
  const std::string op = calleeOf(model, entry.id);
  if (!named.empty() && !op.empty()) {
    const std::string own = model::Identity(op).sequence(named).text();
    if (model.find(own)) return {own, ""};
    const std::string shared = model::Identity(op).sequence().text();
    const model::Paths paths(model);
    if (model.find(shared) && paths.hasScenario(shared, named)) return {shared, named};
  }
  return {entry.field("expands"), entry.field("expands-path")};
}

/// Walks a behaviour into everything its calls enter, keeping what crosses between the participants.
template <typename On>
void collect(const model::Model& model, const std::string& sequence, const std::string& path, const On& on, std::set<std::string>& stack,
             Below& out) {
  const std::string key = model::Identity(sequence).withScenario(path).text();
  if (!stack.insert(key).second) return;  // a behaviour already being walked: the cycle ends here
  const model::Paths paths(model);
  const std::string self = paths.holderOf(sequence);
  for (const model::Entity* m : paths.pathOf(sequence, path, nullptr)) {
    if (m->field("return") == "true") continue;
    const std::string from = on(m->field("from-type").empty() ? self : m->field("from-type"));
    const std::string to = on(m->field("type"));
    const std::string entered = m->field("expands");
    if (!to.empty()) {
      out.entered.insert(to);
      if (entered.empty()) out.open.insert(to);  // what that call does is not written: the participant may do more
    }
    if (!from.empty() && !to.empty() && from != to)
      out.crossings.push_back({from, to, calleeOf(model, m->id), m->field("op"), m->location});
    if (!entered.empty()) collect(model, entered, m->field("expands-path"), on, stack, out);
  }
  stack.erase(key);
}

/// Whether a crossing of the level below is the one the scenario says: the same two ends, and the same operation
/// when the scenario's message is bound to one.
bool same(const Crossing& said, const Crossing& done) {
  return said.from == done.from && said.to == done.to && (said.op.empty() || said.op == done.op);
}

/// Where each crossing said finds its place, in order, among those done: the longest such choice; -1 for one
/// that finds none.
std::vector<int> placed(const std::vector<Crossing>& said, const std::vector<Crossing>& done) {
  const size_t n = said.size(), m = done.size();
  std::vector<std::vector<int>> best(n + 1, std::vector<int>(m + 1, 0));
  for (size_t i = n; i-- > 0;)
    for (size_t j = m; j-- > 0;) best[i][j] = same(said[i], done[j]) ? best[i + 1][j + 1] + 1 : std::max(best[i + 1][j], best[i][j + 1]);
  std::vector<int> out(n, -1);
  for (size_t i = 0, j = 0; i < n && j < m;) {
    if (same(said[i], done[j]) && best[i][j] == best[i + 1][j + 1] + 1) {
      out[i] = static_cast<int>(j);
      ++i;
      ++j;
    } else if (best[i + 1][j] >= best[i][j + 1]) {
      ++i;
    } else {
      ++j;
    }
  }
  return out;
}

}  // namespace

std::string LevelCoherence::name() const { return "levels"; }

std::vector<std::string> LevelCoherence::needs() const { return {"behaviour"}; }

std::vector<model::Finding> LevelCoherence::check(const Context& context) {
  const model::Model& model = context.model();
  const model::Paths paths(model);
  std::vector<model::Finding> found;
  for (const auto& pair : model.entities()) {
    const model::Entity& scenario = pair.second;
    if (scenario.kind != model::Kind::Sequence || scenario.expected) continue;
    const model::Entity* subject = model.find(model::Identity(scenario.id).subject().text());
    if (!subject || subject->kind != model::Kind::UseCase) continue;  // the level that is asked from outside

    // the cut the scenario stands on is its participants: whatever one of them holds, it stands for
    const std::vector<const model::Entity*> written = paths.pathOf(scenario.id, "", nullptr);
    std::set<std::string> participants;
    for (const model::Entity* m : written) {
      if (m->field("return") == "true") continue;
      for (const char* end : {"type", "from-type"})
        if (!m->field(end).empty()) participants.insert(m->field(end));
    }
    // what stands for anything below, or nothing when it is outside what the scenario shows
    const auto on = [&participants](const std::string& id) {
      return id.empty() ? std::string() : axis::Cut::standsAmong(id, participants);
    };

    // what it says crosses, and the operations it is asked through: a message nobody sends comes from outside
    std::vector<Crossing> said;
    std::vector<const model::Entity*> entries;
    for (const model::Entity* m : written) {
      if (m->field("return") == "true") continue;
      if (m->field("from-type").empty()) {
        if (!m->field("expands").empty()) entries.push_back(m);
        continue;
      }
      const std::string from = on(m->field("from-type"));
      const std::string to = on(m->field("type"));
      if (!from.empty() && !to.empty() && from != to) said.push_back({from, to, calleeOf(model, m->id), m->field("op"), m->location});
    }
    if (entries.empty() || said.empty()) continue;  // no level below it is written, or it says nothing to hold it to

    Below below;
    std::string through;
    for (const model::Entity* entry : entries) {
      const auto behaviour = behaviourBelow(model, scenario, *entry);
      below.entered.insert(on(paths.holderOf(behaviour.first)));
      std::set<std::string> stack;
      collect(model, behaviour.first, behaviour.second, on, stack, below);
      through += (through.empty() ? "" : ", ") + calleeOf(model, entry->id);
    }

    const std::vector<int> place = placed(said, below.crossings);
    const std::set<int> taken(place.begin(), place.end());
    for (size_t i = 0; i < said.size(); ++i) {
      if (place[i] >= 0) continue;
      const Crossing& c = said[i];
      const std::string says = scenario.name + " has " + c.from + " send " + c.label + " to " + c.to;
      // a sender is known below when something is seen to happen in it and every call made in it is written
      if (!below.entered.count(c.from) || below.open.count(c.from)) {
        found.push_back(context.metamodel().finding(
            model::Code::W_LEVEL_NOT_BELOW,
            says + ", which the behaviour below it (" + through + ") does not show yet: what " + c.from + " does is not all written",
            c.location));
        continue;
      }
      // it is done below, at a place no other crossing said takes: the order is what differs
      bool elsewhere = false, atAll = false;
      for (size_t j = 0; j < below.crossings.size(); ++j) {
        if (!same(c, below.crossings[j])) continue;
        atAll = true;
        elsewhere = elsewhere || !taken.count(static_cast<int>(j));
      }
      if (elsewhere) {
        found.push_back(context.metamodel().finding(
            model::Code::W_LEVEL_ORDER, says + " here, and the behaviour below it (" + through + ") does it in another order", c.location));
      } else {
        found.push_back(context.metamodel().finding(
            model::Code::W_LEVEL_NO_CROSSING,
            says + ", and the behaviour below it (" + through + ") " + (atAll ? "does not do it that many times" : "never does"),
            c.location));
      }
    }
  }
  return found;
}

}  // namespace check
}  // namespace ds
