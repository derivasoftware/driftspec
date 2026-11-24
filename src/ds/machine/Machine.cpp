#include "ds/machine/Machine.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <utility>

#include "ds/machine/Calls.hpp"
#include "ds/machine/Context.hpp"
#include "ds/machine/Jump.hpp"
#include "ds/machine/StateRule.hpp"
#include "ds/machine/Subject.hpp"
#include "ds/machine/WrittenFields.hpp"
#include "ds/model/Identity.hpp"

namespace ds {
namespace machine {

model::Location Machine::at(const model::Entity& machine, int line) {
  model::Location where = machine.location;
  if (line > 0) {
    where.line = line;
    where.endLine = line;
  }
  return where;
}

void Machine::note(std::vector<std::string>& all, const std::string& name) {
  if (!name.empty() && std::find(all.begin(), all.end(), name) == all.end()) all.push_back(name);
}

Machine::Machine(const model::Model& model, const model::Entity& machine) : model_(model), machine_(machine) {
  for (const auto& r : model_.index().fromOfKind(machine_.id, "behaviour-of")) subject_ = r.to;
  if (subject_.empty()) subject_ = model::Identity(machine_.id).machineSubject().text();
}

std::vector<std::string> Machine::states() const {
  std::vector<std::string> out;
  for (const StateRule& rule : StateRule::of(machine_)) note(out, rule.state);
  note(out, machine_.field("initial"));
  for (const std::string& jump : WrittenFields::pieces(machine_.field("transitions"), '\n')) {
    const std::vector<std::string> ends = WrittenFields::pieces(jump, '>');
    if (ends.size() < 2) continue;
    note(out, ends[0]);
    note(out, ends[1]);
  }
  return out;
}

std::vector<std::string> Machine::rules(const std::string& state) const {
  std::vector<std::string> out;
  for (const StateRule& rule : StateRule::of(machine_))
    if (rule.state == state) out.push_back(rule.text);
  return out;
}

std::string Machine::initial() const {
  if (!machine_.field("initial").empty()) return machine_.field("initial");
  const std::vector<StateRule> all = StateRule::of(machine_);
  Context defaults;
  for (const StateRule& rule : all) {
    for (const Literal& l : rule.literals) {
      const model::Entity* a = Subject(model_, subject_).attribute(l.name);
      if (a && !a->field("default").empty()) defaults.set(l.name, {a->field("default")});
    }
  }
  std::vector<std::string> starts;
  for (const StateRule& rule : all) {
    Context asks;
    if (Context::allowed(Subject(model_, subject_), rule, &asks) && defaults.within(asks)) note(starts, rule.state);
  }
  return starts.size() == 1 ? starts[0] : "";
}

std::vector<model::Finding> Machine::faults(const model::Metamodel& metamodel) const {
  std::vector<model::Finding> found;
  const model::Entity* owner = model_.find(subject_);
  const std::string who = owner ? owner->name : machine_.name;
  const std::vector<StateRule> all = StateRule::of(machine_);
  std::vector<Context> asks(all.size());
  std::vector<bool> can(all.size(), true);
  for (std::size_t i = 0; i < all.size(); ++i) {
    bool sound = true;  // a rule that names what is not there is said once: it is not held to the others
    for (const Literal& l : all[i].literals) {
      if (l.op.empty()) continue;  // a name alone that the subject does not hold is a condition with a name
      const std::vector<std::string> values = Subject(model_, subject_).values(l.name);
      if (!Subject(model_, subject_).attribute(l.name)) {
        sound = false;
        found.push_back(
            metamodel.finding(model::Code::W_STATE_UNDECLARED,
                              "the state " + all[i].state + " of " + who + " asks " + l.written() + ", and " + who + " holds no " + l.name,
                              at(machine_, all[i].line)));
      } else if (!values.empty() && std::find(values.begin(), values.end(), l.value) == values.end()) {
        sound = false;
        found.push_back(metamodel.finding(model::Code::W_STATE_VALUE,
                                          "the state " + all[i].state + " of " + who + " asks " + l.written() + ", and " + l.name +
                                              " is never " + l.value + ": it is one of " + WrittenFields::joined(values, ", "),
                                          at(machine_, all[i].line)));
      }
    }
    can[i] = Context::allowed(Subject(model_, subject_), all[i], &asks[i]);
    if (!sound) {
      can[i] = false;
    } else if (!can[i]) {
      found.push_back(metamodel.finding(model::Code::W_STATE_IMPOSSIBLE,
                                        "the state " + all[i].state + " of " + who + " can never be: its rule (" + all[i].text +
                                            ") asks two things that exclude each other",
                                        at(machine_, all[i].line)));
    }
  }
  for (std::size_t i = 0; i < all.size(); ++i) {
    for (std::size_t j = i + 1; j < all.size(); ++j) {
      if (all[i].state == all[j].state || !can[i] || !can[j]) continue;
      Context both;
      if (!asks[i].together(asks[j], &both)) continue;
      found.push_back(metamodel.finding(model::Code::W_STATE_OVERLAP,
                                        who + " can be " + all[i].state + " and " + all[j].state + " at once: when " + both.told() +
                                            ". The rules of a machine exclude each other",
                                        at(machine_, all[j].line)));
    }
  }
  return found;
}

std::vector<std::string> Machine::events() const {
  std::vector<std::string> out;
  for (const Jump& jump : Jump::of(machine_)) note(out, jump.event);
  return out;
}

std::vector<std::string> Machine::targets(const std::string& event, const std::string& from) const {
  std::vector<std::string> out;
  for (const Jump& jump : Jump::of(machine_))
    if (jump.event == event && jump.from == from) note(out, jump.to + (jump.guard.empty() ? "" : " [" + jump.guard + "]"));
  return out;
}

std::vector<std::string> Machine::sources(const std::string& event) const {
  std::vector<std::string> out;
  for (const Jump& jump : Jump::of(machine_))
    if (jump.event == event) note(out, jump.from);
  return out;
}

std::vector<Call> Machine::calls() const {
  std::vector<Call> out;
  const std::vector<Jump> jumps = Jump::of(machine_);
  if (jumps.empty()) return out;
  const std::vector<std::string> everyState = states();
  const Calls walk(model_, subject_, jumps, everyState);
  for (const auto& pair : model_.entities())
    if (pair.second.kind == model::Kind::Sequence) walk.of(pair.second, out);
  return out;
}

}  // namespace machine
}  // namespace ds
