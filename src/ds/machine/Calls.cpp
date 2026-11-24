#include "ds/machine/Calls.hpp"

#include "ds/model/Paths.hpp"

namespace ds {
namespace machine {

std::string Calls::named(const model::Entity& sequence) {
  const std::string subject = sequence.field("subject").empty() ? sequence.name : sequence.field("subject");
  return subject + (sequence.field("scenario").empty() ? "" : " / " + sequence.field("scenario"));
}

Calls::May Calls::either(const May& a, const May& b) {
  May out;
  for (const auto& pair : a) {
    const auto other = b.find(pair.first);
    if (other == b.end()) continue;
    out[pair.first] = pair.second;
    out[pair.first].insert(other->second.begin(), other->second.end());
  }
  return out;
}

void Calls::of(const model::Entity& sequence, std::vector<Call>& found) const {
  May may;
  walk(sequence, sequence.id, may, found);
}

void Calls::walk(const model::Entity& sequence, const std::string& holder, May& may, std::vector<Call>& found) const {
  for (const std::string& id : model_.index().children(holder)) {
    const model::Entity* step = model_.find(id);
    if (!step) continue;
    if (step->kind == model::Kind::Message) {
      call(sequence, *step, may, found);
    } else if (step->kind == model::Kind::Frame) {
      std::vector<std::string> sides;
      for (const std::string& side : model_.index().children(id)) {
        const model::Entity* branch = model_.find(side);
        if (branch && branch->kind == model::Kind::Frame && branch->field("kind") == "branch") sides.push_back(side);
      }
      May after = may;  // it may not happen
      if (sides.size() > 1) {
        for (std::size_t i = 0; i < sides.size(); ++i) {
          May side = may;
          walk(sequence, sides[i], side, found);
          after = i == 0 ? side : either(after, side);  // one of its sides happens
        }
      } else {
        May inside = may;
        walk(sequence, id, inside, found);
        after = either(may, inside);
      }
      may = after;
    }
  }
}

void Calls::call(const model::Entity& sequence, const model::Entity& m, May& may, std::vector<Call>& found) const {
  if (m.field("return") == "true" || m.field("op").empty()) return;
  const std::string receiver = m.field("to");
  const model::Paths paths(model_);
  if ((receiver == "self" ? paths.holderOf(sequence.id) : m.field("type")) != subject_) return;
  std::string event = m.field("op");
  if (event.rfind("::") != std::string::npos) event = event.substr(event.rfind("::") + 2);
  std::set<std::string> targets;
  for (const Jump& jump : jumps_)
    if (jump.event == event) targets.insert(jump.to);
  if (targets.empty()) return;  // no event of the machine: it moves nothing
  const auto known = may.find(receiver);
  const std::set<std::string> now = known == may.end() ? std::set<std::string>(states_.begin(), states_.end()) : known->second;
  std::set<std::string> next;
  for (const Jump& jump : jumps_) {
    if (jump.event != event || !now.count(jump.from)) continue;
    next.insert(jump.to);
    if (!jump.guard.empty()) next.insert(jump.from);  // the guard may not hold: it stays
  }
  Call made{named(sequence), m.id, receiver, event, {}, !next.empty()};
  for (const std::string& state : states_)  // in the order the machine writes its states
    if (now.count(state)) made.from.push_back(state);
  found.push_back(made);
  may[receiver] = next.empty() ? targets : next;  // a call that does not fit: go on as if it had been made
}

}  // namespace machine
}  // namespace ds
