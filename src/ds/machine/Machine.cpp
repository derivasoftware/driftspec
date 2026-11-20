#include "ds/machine/Machine.hpp"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <utility>

#include "ds/model/Identity.hpp"

namespace ds {
namespace machine {

namespace {

/// One literal of a rule: a name of the context, alone or compared with a value; `not` turns it round.
struct Literal {
  bool negated = false;
  std::string name;
  std::string op;  ///< "=", "!=", or empty for a name alone
  std::string value;
};

/// One rule: the state it computes and the literals that all hold when the subject is in it.
struct Rule {
  std::string state;
  std::vector<Literal> literals;
  std::string text;  ///< as written
  int line = 0;
};

/// What each name of a context may be worth. A name that is absent may be worth anything.
using Context = std::map<std::string, std::set<std::string>>;

std::vector<std::string> pieces(const std::string& text, char separator) {
  std::vector<std::string> out;
  std::stringstream in(text);
  for (std::string piece; std::getline(in, piece, separator);) out.push_back(piece);
  return out;
}

std::string joined(const std::vector<std::string>& parts, const std::string& with) {
  std::string out;
  for (const auto& p : parts) out += (out.empty() ? "" : with) + p;
  return out;
}

Literal literalOf(std::string text) {
  Literal l;
  if (!text.empty() && text[0] == '!') {  // `not`, kept as a leading !; a name never starts with one
    l.negated = true;
    text.erase(0, 1);
  }
  const auto differs = text.find("!=");
  const auto equals = text.find('=');
  if (differs != std::string::npos) {
    l.name = text.substr(0, differs);
    l.op = "!=";
    l.value = text.substr(differs + 2);
  } else if (equals != std::string::npos) {
    l.name = text.substr(0, equals);
    l.op = "=";
    l.value = text.substr(equals + 1);
  } else {
    l.name = text;
  }
  return l;
}

std::string written(const Literal& l) { return (l.negated ? "not " : "") + l.name + (l.op.empty() ? "" : " " + l.op + " " + l.value); }

std::vector<Rule> rulesOf(const model::Entity& machine) {
  std::vector<Rule> out;
  for (const std::string& line : pieces(machine.field("rules"), '\n')) {
    const std::vector<std::string> parts = pieces(line, '|');
    if (parts.size() < 3) continue;
    Rule rule;
    rule.state = parts[0];
    std::vector<std::string> said;
    for (const std::string& text : pieces(parts[1], ',')) {
      rule.literals.push_back(literalOf(text));
      said.push_back(written(rule.literals.back()));
    }
    rule.text = joined(said, ", ");
    rule.line = std::atoi(parts[2].c_str());
    out.push_back(rule);
  }
  return out;
}

/// The attribute of the subject a name is; null when it is none: then it is a condition with a name.
const model::Entity* attributeOf(const model::Model& model, const std::string& subject, const std::string& name) {
  const model::Entity* e = model.find(subject + "::" + name);
  return e && e->kind == model::Kind::Attribute ? e : nullptr;
}

/// The values a name may take: those of its enum; true and false for a condition with a name, or for an
/// attribute whose initial value is true or false (ds reads no type's name: DS-18); empty when they cannot
/// be listed (a number, a text, a time).
std::vector<std::string> valuesOf(const model::Model& model, const std::string& subject, const std::string& name) {
  const model::Entity* a = attributeOf(model, subject, name);
  if (!a || a->field("default") == "true" || a->field("default") == "false") return {"false", "true"};
  for (const auto& r : model.index().from(a->id)) {
    if (r.kind != "uses" && r.kind != "owns" && r.kind != "refs" && r.kind != "shares") continue;
    const model::Entity* type = model.find(r.to);
    if (!type || type->kind != model::Kind::Enum) continue;
    std::vector<std::string> values;
    for (std::string v : pieces(type->field("values"), ',')) {
      v.erase(0, v.find_first_not_of(' '));
      values.push_back(v);
    }
    return values;
  }
  return {};
}

/// What a rule lets each name be. False when it asks two things that exclude each other.
bool allowed(const model::Model& model, const std::string& subject, const Rule& rule, Context* out) {
  for (const Literal& l : rule.literals) {
    std::vector<std::string> all = valuesOf(model, subject, l.name);
    if (all.empty()) all = {l.value, "(another)"};  // not listable: what it asks, and anything else
    std::set<std::string> ok;
    if (l.op.empty()) ok = {"true"};
    if (l.op == "=") ok = {l.value};
    if (l.op == "!=") {
      ok.insert(all.begin(), all.end());
      ok.erase(l.value);
    }
    if (l.negated) {
      std::set<std::string> rest(all.begin(), all.end());
      for (const auto& v : ok) rest.erase(v);
      ok = rest;
    }
    const auto found = out->find(l.name);
    if (found == out->end()) {
      (*out)[l.name] = ok;
      continue;
    }
    std::set<std::string> both;
    for (const auto& v : found->second)
      if (ok.count(v)) both.insert(v);
    found->second = both;
  }
  return std::all_of(out->begin(), out->end(), [](const auto& entry) { return !entry.second.empty(); });
}

/// Whether two contexts can hold at once, and one in which both do.
bool together(const Context& a, const Context& b, Context* out) {
  *out = a;
  for (const auto& entry : b) {
    const auto found = out->find(entry.first);
    if (found == out->end()) {
      (*out)[entry.first] = entry.second;
      continue;
    }
    std::set<std::string> both;
    for (const auto& v : found->second)
      if (entry.second.count(v)) both.insert(v);
    if (both.empty()) return false;
    found->second = both;
  }
  return true;
}

/// A context as a reader is told it: one value for each name.
std::string told(const Context& context) {
  std::vector<std::string> parts;
  for (const auto& entry : context) parts.push_back(entry.first + " = " + *entry.second.begin());
  return joined(parts, ", ");
}

/// Whether a context, known in full for what a rule reads, is in the state the rule computes.
bool within(const Context& context, const Context& rule) {
  for (const auto& entry : rule) {
    const auto known = context.find(entry.first);
    if (known == context.end()) return false;
    for (const auto& v : known->second)
      if (!entry.second.count(v)) return false;
  }
  return true;
}

model::Location at(const model::Entity& machine, int line) {
  model::Location where = machine.location;
  if (line > 0) {
    where.line = line;
    where.endLine = line;
  }
  return where;
}

void note(std::vector<std::string>& all, const std::string& name) {
  if (!name.empty() && std::find(all.begin(), all.end(), name) == all.end()) all.push_back(name);
}

/// A jump as its author wrote it: from a state to another, on an event; with a guard it may also not happen.
struct Jump {
  std::string from;
  std::string to;
  std::string event;
  std::string guard;  ///< as written; "" when it has none
};

std::vector<Jump> jumpsOf(const model::Entity& machine) {
  std::vector<Jump> out;
  for (const std::string& line : pieces(machine.field("transitions"), '\n')) {
    const auto first = line.find('>');
    const auto second = line.find('>', first + 1);
    if (first == std::string::npos || second == std::string::npos) continue;
    const std::string label = line.substr(second + 1);  // event [guard] / action
    const auto open = label.find('[');
    const auto close = label.find(']', open == std::string::npos ? 0 : open);
    const bool guarded = open != std::string::npos && close != std::string::npos;
    out.push_back({line.substr(0, first), line.substr(first + 1, second - first - 1), label.substr(0, label.find_first_of(" [")),
                   guarded ? label.substr(open + 1, close - open - 1) : ""});
  }
  return out;
}

/// A behaviour as a reader calls it: its subject, and its scenario when it has one.
std::string named(const model::Entity& sequence) {
  const std::string subject = sequence.field("subject").empty() ? sequence.name : sequence.field("subject");
  return subject + (sequence.field("scenario").empty() ? "" : " / " + sequence.field("scenario"));
}

/// The states a receiver may be in, by the name it has in a behaviour; a name that is not there may be in any.
using May = std::map<std::string, std::set<std::string>>;

/// What two ways through a behaviour leave between them: a receiver either way leaves in any state is in any.
May either(const May& a, const May& b) {
  May out;
  for (const auto& pair : a) {
    const auto other = b.find(pair.first);
    if (other == b.end()) continue;
    out[pair.first] = pair.second;
    out[pair.first].insert(other->second.begin(), other->second.end());
  }
  return out;
}

/// One walk of the behaviours written, for the calls of the events of one machine (DS-250).
class Calls {
 public:
  Calls(const model::Model& model, const std::string& subject, const std::vector<Jump>& jumps, const std::vector<std::string>& states)
      : model_(model), subject_(subject), jumps_(jumps), states_(states) {}

  /// The calls a behaviour makes, in the order written, added to what was found.
  void of(const model::Entity& sequence, std::vector<Call>& found) const {
    May may;
    walk(sequence, sequence.id, may, found);
  }

 private:
  /// What a behaviour or a frame holds, in the order written. A frame may not happen: what comes after it
  /// finds its receivers as the frame leaves them or as they were; the sides of an alt are each one way.
  void walk(const model::Entity& sequence, const std::string& holder, May& may, std::vector<Call>& found) const {
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

  /// One message: a call of an event of the machine moves its receiver by the jumps on that event.
  void call(const model::Entity& sequence, const model::Entity& m, May& may, std::vector<Call>& found) const {
    if (m.field("return") == "true" || m.field("op").empty()) return;
    const std::string receiver = m.field("to");
    if ((receiver == "self" ? model::holderOf(model_, sequence.id) : m.field("type")) != subject_) return;
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

  const model::Model& model_;
  const std::string& subject_;
  const std::vector<Jump>& jumps_;
  const std::vector<std::string>& states_;
};

}  // namespace

Machine::Machine(const model::Model& model, const model::Entity& machine) : model_(model), machine_(machine) {
  for (const auto& r : model_.index().from(machine_.id))
    if (r.kind == "behaviour-of") subject_ = r.to;
  if (subject_.empty()) subject_ = model::Identity(machine_.id).machineSubject().text();
}

std::vector<std::string> Machine::states() const {
  std::vector<std::string> out;
  for (const Rule& rule : rulesOf(machine_)) note(out, rule.state);
  note(out, machine_.field("initial"));
  for (const std::string& jump : pieces(machine_.field("transitions"), '\n')) {
    const std::vector<std::string> ends = pieces(jump, '>');
    if (ends.size() < 2) continue;
    note(out, ends[0]);
    note(out, ends[1]);
  }
  return out;
}

std::vector<std::string> Machine::rules(const std::string& state) const {
  std::vector<std::string> out;
  for (const Rule& rule : rulesOf(machine_))
    if (rule.state == state) out.push_back(rule.text);
  return out;
}

std::string Machine::initial() const {
  if (!machine_.field("initial").empty()) return machine_.field("initial");
  const std::vector<Rule> all = rulesOf(machine_);
  Context defaults;
  for (const Rule& rule : all) {
    for (const Literal& l : rule.literals) {
      const model::Entity* a = attributeOf(model_, subject_, l.name);
      if (a && !a->field("default").empty()) defaults[l.name] = {a->field("default")};
    }
  }
  std::vector<std::string> starts;
  for (const Rule& rule : all) {
    Context asks;
    if (allowed(model_, subject_, rule, &asks) && within(defaults, asks)) note(starts, rule.state);
  }
  return starts.size() == 1 ? starts[0] : "";
}

std::vector<model::Finding> Machine::faults() const {
  std::vector<model::Finding> found;
  const model::Entity* owner = model_.find(subject_);
  const std::string who = owner ? owner->name : machine_.name;
  const std::vector<Rule> all = rulesOf(machine_);
  std::vector<Context> asks(all.size());
  std::vector<bool> can(all.size(), true);
  for (std::size_t i = 0; i < all.size(); ++i) {
    bool sound = true;  // a rule that names what is not there is said once: it is not held to the others
    for (const Literal& l : all[i].literals) {
      if (l.op.empty()) continue;  // a name alone that the subject does not hold is a condition with a name
      const std::vector<std::string> values = valuesOf(model_, subject_, l.name);
      if (!attributeOf(model_, subject_, l.name)) {
        sound = false;
        found.push_back({"W-STATE-UNDECLARED", "warning",
                         "the state " + all[i].state + " of " + who + " asks " + written(l) + ", and " + who + " holds no " + l.name,
                         at(machine_, all[i].line)});
      } else if (!values.empty() && std::find(values.begin(), values.end(), l.value) == values.end()) {
        sound = false;
        found.push_back({"W-STATE-VALUE", "warning",
                         "the state " + all[i].state + " of " + who + " asks " + written(l) + ", and " + l.name + " is never " + l.value +
                             ": it is one of " + joined(values, ", "),
                         at(machine_, all[i].line)});
      }
    }
    can[i] = allowed(model_, subject_, all[i], &asks[i]);
    if (!sound) {
      can[i] = false;
    } else if (!can[i]) {
      found.push_back({"W-STATE-IMPOSSIBLE", "warning",
                       "the state " + all[i].state + " of " + who + " can never be: its rule (" + all[i].text +
                           ") asks two things that exclude each other",
                       at(machine_, all[i].line)});
    }
  }
  for (std::size_t i = 0; i < all.size(); ++i) {
    for (std::size_t j = i + 1; j < all.size(); ++j) {
      if (all[i].state == all[j].state || !can[i] || !can[j]) continue;
      Context both;
      if (!together(asks[i], asks[j], &both)) continue;
      found.push_back({"W-STATE-OVERLAP", "warning",
                       who + " can be " + all[i].state + " and " + all[j].state + " at once: when " + told(both) +
                           ". The rules of a machine exclude each other",
                       at(machine_, all[j].line)});
    }
  }
  return found;
}

std::vector<std::string> Machine::events() const {
  std::vector<std::string> out;
  for (const Jump& jump : jumpsOf(machine_)) note(out, jump.event);
  return out;
}

std::vector<std::string> Machine::targets(const std::string& event, const std::string& from) const {
  std::vector<std::string> out;
  for (const Jump& jump : jumpsOf(machine_))
    if (jump.event == event && jump.from == from) note(out, jump.to + (jump.guard.empty() ? "" : " [" + jump.guard + "]"));
  return out;
}

std::vector<std::string> Machine::sources(const std::string& event) const {
  std::vector<std::string> out;
  for (const Jump& jump : jumpsOf(machine_))
    if (jump.event == event) note(out, jump.from);
  return out;
}

std::vector<Call> Machine::calls() const {
  std::vector<Call> out;
  const std::vector<Jump> jumps = jumpsOf(machine_);
  if (jumps.empty()) return out;
  const std::vector<std::string> everyState = states();
  const Calls walk(model_, subject_, jumps, everyState);
  for (const auto& pair : model_.entities())
    if (pair.second.kind == model::Kind::Sequence) walk.of(pair.second, out);
  return out;
}

}  // namespace machine
}  // namespace ds
