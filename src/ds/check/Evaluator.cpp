#include "ds/check/Evaluator.hpp"

#include <algorithm>
#include <cstdlib>
#include <functional>

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace check {

namespace {

using Form = model::LiteralForm;
using Bindings = std::map<std::string, std::string>;

std::string key(const std::string& owner, const std::string& written) { return owner + "\n" + written; }

std::string describe(const std::map<std::string, std::string>& bindings) {
  std::string out;
  for (const auto& b : bindings) out += (out.empty() ? "" : ", ") + b.first + " = " + b.second;
  return out;
}

const std::size_t kReportsPerRule = 10;

bool compares(Form form) {
  return form == Form::NotEqual || form == Form::Less || form == Form::LessEqual || form == Form::Greater || form == Form::GreaterEqual;
}

/// A number written as such, read whole: 150, -2.5; anything else is not one.
bool number(const std::string& text, double& out_number) {
  if (text.empty()) return false;
  char* end = nullptr;
  out_number = std::strtod(text.c_str(), &end);
  return end == text.c_str() + text.size();
}

/// A value written in a sequence: a number or a string, not a name.
bool literal(const std::string& text) {
  double ignored = 0;
  return number(text, ignored) || (text.size() >= 2 && text.front() == '"' && text.back() == '"');
}

/// Whether `a` and `z` stand in the comparison: numbers as numbers, anything else equal or not as written.
bool compare(Form form, const std::string& a, const std::string& z) {
  double x = 0, y = 0;
  const bool numbers = number(a, x) && number(z, y);
  switch (form) {
    case Form::Equal:
      return numbers ? x == y : a == z;
    case Form::NotEqual:
      return numbers ? x != y : a != z;
    case Form::Less:
      return numbers && x < y;
    case Form::LessEqual:
      return numbers && x <= y;
    case Form::Greater:
      return numbers && x > y;
    case Form::GreaterEqual:
      return numbers && x >= y;
    default:
      return false;
  }
}

}  // namespace

Evaluator::Evaluator(model::Model& model, const Resolver& resolver) : model_(model), resolver_(resolver) {}

std::vector<model::Finding> Evaluator::run() {
  if (model_.formulas().empty()) return {};
  index();
  for (const auto& f : model_.formulas()) {
    if (!validate(f.first, f.second)) invalid_.insert(f.first);
    checkVacuity(f.first, f.second);
  }
  stratify();
  computeDefinitions();
  for (const auto& f : model_.formulas()) {
    const model::Entity* e = model_.find(f.first);
    if (e && e->kind == model::Kind::Rule && !invalid_.count(f.first)) evaluateRule(f.first, f.second);
  }
  for (const auto& f : model_.formulas()) {  // after the rules: a table may say which of them hold
    const model::Entity* e = model_.find(f.first);
    if (e && e->kind == model::Kind::Diagram && !invalid_.count(f.first)) tabulate(f.first, f.second);
  }
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

// What holds what (contains, else the declared scope), the entities of each kind and every relation
// both ways: the facts a rule is evaluated over.
// The joins a rule asks for: its relations by their ends, and the declared entities of each kind.
// What holds what is the model's own index (DS-171).
void Evaluator::index() {
  for (const auto& r : model_.index().all()) {
    forward_[r.kind].insert({r.from, r.to});
    backward_[r.kind].insert({r.to, r.from});
  }
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (!e.expected) byKind_[model_.metamodel().nameOf(e.kind)].push_back(e.id);
  }
}

std::string Evaluator::scopeOf(const std::string& owner) const {
  const model::Entity* e = model_.find(owner);
  return e ? e->scope : "";
}

void Evaluator::report(const std::string& code, const std::string& owner, const std::string& message, const model::Location& at) {
  found_.push_back({code, "error", owner + " " + message, at});
}

// Everything a formula names exists in the metamodel or the model, every definition it applies takes
// that many arguments, and every variable is bound by a positive literal before anything tests it.
bool Evaluator::validate(const std::string& id, const model::Formula& formula) {
  const model::Entity* e = model_.find(id);
  if (!e) return false;
  bool ok = true;
  std::set<std::string> bound;
  for (const auto& range : formula.ranges) {
    if (!model_.metamodel().specNamed(range.kind)) {
      report("E-RULE-UNKNOWN", id, "ranges over '" + range.kind + "', which is no kind of the language", range.location);
      ok = false;
    }
    if (!range.scope.empty()) {
      const std::string scope = resolver_.resolve(range.scope, e->scope);
      if (scope.empty()) {
        report("E-RULE-UNKNOWN", id, "ranges inside '" + range.scope + "', which nothing declares", range.location);
        ok = false;
      }
      constants_[key(id, range.scope)] = scope;
    }
    bound.insert(range.variable);
  }
  if (e->kind == model::Kind::Diagram) {
    ok = validateBody(id, formula.where, bound) && ok;
    for (const auto& column : formula.columns) {
      std::set<std::string> inColumn = bound;
      if (column.form != "field") ok = validateBody(id, column.body, inColumn) && ok;
    }
  } else if (e->kind == model::Kind::Rule) {
    ok = validateBody(id, formula.where, bound) && ok;
    for (const auto& claim : formula.claims) {
      std::set<std::string> inClaim = bound;
      ok = validateBody(id, claim, inClaim) && ok;
    }
  } else {
    for (const auto& c : formula.cases) {
      std::set<std::string> inCase;
      ok = validateBody(id, c, inCase) && ok;
      for (const auto& p : formula.parameters) {
        if (inCase.count(p) || c.empty()) continue;
        report("E-RULE-UNSAFE", id, "leaves its parameter '" + p + "' unbound in one of its cases: name it in a positive literal",
               c.front().location);
        ok = false;
      }
    }
  }
  return ok;
}

namespace {

/// Whether a relation of the metamodel may leave (or reach) an entity of a kind.
bool joinsAtEnd(const model::RelationSpec& spec, model::Kind kind, bool from) {
  if (!spec.pairs.empty()) {
    for (const auto& p : spec.pairs)
      if ((from ? p.first : p.second) == kind) return true;
    return false;
  }
  const auto& ends = from ? spec.from : spec.to;
  return ends.empty() || std::find(ends.begin(), ends.end(), kind) != ends.end();
}

}  // namespace

// A relation literal whose end is bound to a kind the relation never leaves or reaches can never match: a rule
// that rests on it is decided by vacuity, always holding or never broken (DS-213). Reported, not refused.
void Evaluator::checkVacuity(const std::string& id, const model::Formula& formula) {
  std::map<std::string, model::Kind> kindOf;  // variable -> the kind its range or a kind literal binds it to
  for (const auto& r : formula.ranges)
    if (const model::KindSpec* k = model_.metamodel().specNamed(r.kind)) kindOf[r.variable] = k->kind;
  std::vector<const std::vector<model::Literal>*> bodies{&formula.where};
  for (const auto& c : formula.claims) bodies.push_back(&c);
  for (const auto& c : formula.cases) bodies.push_back(&c);
  for (const auto* body : bodies)
    for (const auto& l : *body)
      if (l.form == Form::Kind && !l.negated && !l.terms.empty() && l.terms[0].variable)
        if (const model::KindSpec* k = model_.metamodel().specNamed(l.name)) kindOf.emplace(l.terms[0].text, k->kind);
  const std::string scope = scopeOf(id);
  const auto kindOfTerm = [&](const model::Term& t, model::Kind* out) {
    if (t.value) return false;
    if (t.variable) {
      const auto it = kindOf.find(t.text);
      if (it == kindOf.end()) return false;
      *out = it->second;
      return true;
    }
    const model::Entity* e = model_.find(resolver_.resolve(t.text, scope));
    if (!e) return false;
    *out = e->kind;
    return true;
  };
  for (const auto* body : bodies)
    for (const auto& l : *body) {
      if (l.form != Form::Relation || l.terms.size() != 2) continue;
      const model::RelationSpec* spec = model_.metamodel().relationOf(l.name);
      if (!spec) continue;
      for (int end = 0; end < 2; ++end) {
        model::Kind kind;
        if (!kindOfTerm(l.terms[end], &kind) || joinsAtEnd(*spec, kind, end == 0)) continue;
        found_.push_back({"W-RULE-NEVER-MATCHES", "warning",
                          id + " writes '" + l.terms[0].text + " " + l.name + " " + l.terms[1].text + "', and " + l.name +
                              (end == 0 ? " never leaves " : " never reaches ") + model_.metamodel().nameOf(kind) + " (" +
                              l.terms[end].text + "): the literal matches nothing",
                          l.location});
        break;
      }
    }
}

bool Evaluator::validateBody(const std::string& id, const std::vector<model::Literal>& body, std::set<std::string>& out_bound) {
  bool ok = true;
  const std::string scope = scopeOf(id);
  for (const auto& l : body) {
    if (l.form == Form::Relation && !model_.metamodel().relationOf(l.name)) {
      report("E-RULE-UNKNOWN", id, "names the relation '" + l.name + "', which the metamodel does not declare", l.location);
      ok = false;
    } else if (l.form == Form::Kind && !model_.metamodel().specNamed(l.name)) {
      report("E-RULE-UNKNOWN", id, "names the kind '" + l.name + "', which is no kind of the language", l.location);
      ok = false;
    } else if (l.form == Form::Call) {
      const std::string d = resolver_.resolve(l.name, scope);
      const model::Entity* def = d.empty() ? nullptr : model_.find(d);
      if (!def || def->kind != model::Kind::Definition) {
        report("E-RULE-UNKNOWN", id, "applies '" + l.name + "', which no definition in scope declares", l.location);
        ok = false;
      } else {
        constants_[key(id, "call:" + l.name)] = d;
        const auto it = model_.formulas().find(d);
        const std::size_t arity = it == model_.formulas().end() ? 0 : it->second.parameters.size();
        if (arity != l.terms.size()) {
          report("E-RULE-ARITY", id,
                 "applies " + d + " to " + std::to_string(l.terms.size()) + " arguments; it takes " + std::to_string(arity), l.location);
          ok = false;
        }
      }
    }
    for (const auto& t : l.terms) {
      if (t.variable || t.value) continue;
      const std::string entity = resolver_.resolve(t.text, scope);
      if (entity.empty()) {
        report("E-RULE-UNKNOWN", id, "names '" + t.text + "', which nothing declares", l.location);
        ok = false;
      }
      constants_[key(id, t.text)] = entity;
    }
  }
  // safety: what positive literals bind, to a fixpoint; any variable left is never out_bound
  const auto known = [&](const model::Term& t) { return !t.variable || out_bound.count(t.text); };
  for (bool changed = true; changed;) {
    changed = false;
    for (const auto& l : body) {
      if (l.negated || compares(l.form)) continue;
      const bool binds = l.form == Form::Relation || l.form == Form::Kind || l.form == Form::Call || l.form == Form::Arg ||
                         std::any_of(l.terms.begin(), l.terms.end(), known);  // in and = bind one side from the other
      if (!binds) continue;
      for (const auto& t : l.terms)
        if (t.variable && out_bound.insert(t.text).second) changed = true;
    }
  }
  std::set<std::string> told;
  for (const auto& l : body)
    for (const auto& t : l.terms)
      if (!known(t) && told.insert(t.text).second) {
        report("E-RULE-UNSAFE", id,
               "leaves the variable '" + t.text + "' unbound: a negation or a comparison tests only what a positive literal binds",
               l.location);
        ok = false;
      }
  return ok;
}

// A definition sits one stratum above every definition it negates; a negation through recursion has
// no stratum and is an error (the evaluation would not be decided).
bool Evaluator::stratify() {
  std::vector<std::string> definitions;
  for (const auto& f : model_.formulas()) {
    const model::Entity* e = model_.find(f.first);
    if (e && e->kind == model::Kind::Definition) {
      definitions.push_back(f.first);
      stratum_[f.first] = 0;
    }
  }
  const int ceiling = static_cast<int>(definitions.size());
  bool ok = true;
  for (bool changed = true; changed;) {
    changed = false;
    for (const auto& d : definitions) {
      for (const auto& c : model_.formulas().at(d).cases)
        for (const auto& l : c) {
          if (l.form != Form::Call) continue;
          const std::string callee = definitionOf(d, l.name);
          if (callee.empty()) continue;
          if (invalid_.count(callee) && !invalid_.count(d)) {
            invalid_.insert(d);  // what rests on a broken definition is not evaluated either
            changed = true;
          }
          const int need = stratum_[callee] + (l.negated ? 1 : 0);
          if (stratum_[d] < need && stratum_[d] <= ceiling) {
            stratum_[d] = need;
            changed = true;
          }
        }
    }
  }
  for (const auto& d : definitions)
    if (stratum_[d] > ceiling) {
      const model::Entity* e = model_.find(d);
      report("E-RULE-UNSTRATIFIED", d, "negates itself through recursion: no order of evaluation decides it", e->location);
      invalid_.insert(d);
      ok = false;
    }
  // a rule applying an invalid definition is not evaluated (the definition carries the finding)
  for (const auto& f : model_.formulas()) {
    const auto rests = [&](const std::vector<model::Literal>& body) {
      for (const auto& l : body)
        if (l.form == Form::Call && invalid_.count(definitionOf(f.first, l.name))) return true;
      return false;
    };
    bool broken = rests(f.second.where);
    for (const auto& c : f.second.claims) broken = broken || rests(c);
    if (broken) invalid_.insert(f.first);
  }
  return ok;
}

std::vector<std::string> Evaluator::dependencyOrder(const std::vector<std::string>& definitions) const {
  const std::set<std::string> level(definitions.begin(), definitions.end());
  std::map<std::string, std::vector<std::string>> applies;  // what each definition applies within the stratum
  for (const auto& d : definitions)
    for (const auto& c : model_.formulas().at(d).cases)
      for (const auto& l : c)
        if (l.form == Form::Call) {
          const std::string callee = definitionOf(d, l.name);
          if (level.count(callee)) applies[d].push_back(callee);
        }
  std::vector<std::string> order;
  std::map<std::string, int> state;  // 1 while its callees are visited, 2 once placed
  bool cycle = false;
  std::function<void(const std::string&)> visit = [&](const std::string& d) {
    if (state[d] == 2 || cycle) return;
    if (state[d] == 1) {
      cycle = true;  // it reaches itself: recursion
      return;
    }
    state[d] = 1;
    for (const auto& callee : applies[d]) visit(callee);
    state[d] = 2;
    order.push_back(d);
  };
  for (const auto& d : definitions) visit(d);
  return cycle ? std::vector<std::string>{} : order;
}

void Evaluator::computeDefinitions() {
  std::map<int, std::vector<std::string>> strata;
  for (const auto& s : stratum_)
    if (!invalid_.count(s.first)) strata[s.second].push_back(s.first);
  for (const auto& level : strata) {
    // a stratum where no definition reaches itself through the others is fixed in one pass, each definition after
    // those of its stratum it applies; one with recursion keeps the passes to its fixpoint (DS-338)
    const std::vector<std::string> order = dependencyOrder(level.second);
    const bool recursive = order.empty() && !level.second.empty();
    const std::vector<std::string>& definitions = recursive ? level.second : order;
    for (bool changed = true; changed;) {
      changed = false;
      for (const auto& d : definitions) {
        const model::Formula& f = model_.formulas().at(d);
        for (const auto& c : f.cases) {
          for (const auto& b : solve(d, c, {}, false)) {
            std::vector<std::string> t;
            for (const auto& p : f.parameters) t.push_back(b.at(p));
            if (extension_[d].insert(t).second) changed = true;
          }
        }
      }
      if (!recursive) break;  // every definition was computed after what it applies: a second pass adds nothing
    }
  }
}

void Evaluator::tabulate(const std::string& id, const model::Formula& formula) {
  std::set<Bindings> subjects;
  std::function<void(std::size_t, const Bindings&)> range = [&](std::size_t i, const Bindings& b) {
    if (i == formula.ranges.size()) {
      for (const auto& w : solve(id, formula.where, b, false)) subjects.insert(w);
      return;
    }
    const model::Range& r = formula.ranges[i];
    const std::string scope = r.scope.empty() ? "" : constants_[key(id, r.scope)];
    const auto it = byKind_.find(r.kind);
    if (it == byKind_.end()) return;
    for (const auto& entity : it->second) {
      if (!scope.empty() && !contains(scope, entity)) continue;
      Bindings next = b;
      next[r.variable] = entity;
      range(i + 1, next);
    }
  };
  range(0, {});
  model::Tabulated table;
  for (const auto& column : formula.columns) table.columns.push_back(column.name);
  const auto shown = [&](const std::string& entity) {  // an artefact by its id, anything else by its name
    const model::Entity* e = model_.find(entity);
    return e && !e->name.empty() ? e->name : entity;
  };
  for (const auto& subject : subjects) {
    std::vector<std::string> cells, kinds;
    for (const auto& column : formula.columns) {
      std::string text, kind;
      if (column.form == "field") {
        const auto bound = subject.find(column.variable);
        const model::Entity* e = bound == subject.end() ? nullptr : model_.find(bound->second);
        if (e && column.field == "name") {
          text = e->name;
        } else if (e && column.field == "id") {
          text = e->id;
        } else if (e && column.field == "holds" && e->kind == model::Kind::Rule) {  // a rule: whether the model keeps it
          const auto broken = model_.breaches().find(e->id);
          const bool holds = broken == model_.breaches().end() || broken->second.empty();
          text = holds ? "holds" : "broken";
          kind = holds ? "" : "against";
        } else if (e) {
          text = e->field(column.field);
        }
      } else if (column.form == "related") {
        std::set<std::string> seen;
        for (const auto& answer : solve(id, column.body, subject, false)) {
          const auto at = answer.find(column.variable);
          if (at == answer.end() || !seen.insert(at->second).second) continue;
          text += (text.empty() ? "" : ", ") + shown(at->second);
        }
      } else {
        const bool yes = !solve(id, column.body, subject, true).empty();
        text = yes ? "yes" : "no";
        kind = yes ? "" : "against";
      }
      if (text.empty()) kind = "none";
      cells.push_back(text);
      kinds.push_back(kind);
    }
    table.rows.push_back(cells);
    table.kinds.push_back(kinds);
  }
  model_.tabulate(id, table);
}

void Evaluator::evaluateRule(const std::string& id, const model::Formula& formula) {
  if (formula.claims.size() != 1) return;  // the checker says so (W-RULE-NO-CLAIM)
  const model::Entity* rule = model_.find(id);
  const auto& claim = formula.claims.front();
  std::set<Bindings> subjects;
  // every binding of the ranges, then of where
  std::function<void(std::size_t, const Bindings&)> range = [&](std::size_t i, const Bindings& b) {
    if (i == formula.ranges.size()) {
      for (const auto& w : solve(id, formula.where, b, false)) subjects.insert(w);
      return;
    }
    const model::Range& r = formula.ranges[i];
    const std::string scope = r.scope.empty() ? "" : constants_[key(id, r.scope)];
    const auto it = byKind_.find(r.kind);
    if (it == byKind_.end()) return;
    for (const auto& entity : it->second) {
      if (!scope.empty() && !contains(scope, entity)) continue;
      const auto already = b.find(r.variable);
      if (already != b.end() && already->second != entity) continue;
      Bindings next = b;
      next[r.variable] = entity;
      range(i + 1, next);
    }
  };
  range(0, {});
  std::size_t broken = 0;
  for (const auto& subject : subjects) {
    Bindings witness;
    const auto found = solve(id, claim, subject, true);
    const bool answered = !found.empty();
    if (answered)
      for (const auto& v : found.front())
        if (!subject.count(v.first)) witness.insert(v);
    if (answered != formula.never) continue;
    const model::Entity* first = formula.ranges.empty() ? nullptr : model_.find(subject.at(formula.ranges.front().variable));
    const model::Location at = first && !first->location.file.empty() ? first->location : rule->location;
    model_.breach(id, {{subject.begin(), subject.end()},
                       formula.never ? std::vector<std::pair<std::string, std::string>>(witness.begin(), witness.end())
                                     : std::vector<std::pair<std::string, std::string>>{},
                       at});  // every breach is kept, the findings stop at a few
    if (++broken > kReportsPerRule) continue;
    std::string message = "is broken";
    if (!subject.empty()) message += " for " + describe(subject);
    if (formula.never && !witness.empty()) message += ": " + describe(witness);
    // the finding sits on what breaks the rule, the first thing it ranges over (a call in its sequence,
    // a class in its file), so the reader lands there; the message names the rule
    report("E-RULE-BROKEN", id, message, at);
  }
  if (broken > kReportsPerRule)
    report("E-RULE-BROKEN", id, "is broken " + std::to_string(broken - kReportsPerRule) + " more times", rule->location);
}

bool Evaluator::value(const std::string& owner, const model::Term& term, const Bindings& bindings, std::string& out_value) const {
  if (term.value) {
    out_value = term.text;
    return true;
  }
  if (term.variable) {
    const auto it = bindings.find(term.text);
    if (it == bindings.end()) return false;
    out_value = it->second;
    if (!term.field.empty()) {  // a field of what it is bound to (DS-268)
      const model::Entity* e = model_.find(it->second);
      out_value = !e ? "" : term.field == "name" ? e->name : term.field == "id" ? e->id : e->field(term.field);
      double ignored = 0;
      if (!number(out_value, ignored)) out_value = "\"" + out_value + "\"";  // a text compares with a string as written
    }
    return true;
  }
  const auto it = constants_.find(key(owner, term.text));
  out_value = it == constants_.end() ? "" : it->second;
  return true;
}

std::string Evaluator::definitionOf(const std::string& owner, const std::string& name) const {
  const auto it = constants_.find(key(owner, "call:" + name));
  return it == constants_.end() ? "" : it->second;
}

bool Evaluator::contains(const std::string& outer, const std::string& inner) const {
  const model::Index index = model_.index();
  std::string up = index.parent(inner);
  if (up.empty()) {  // an artefact hangs from no box by `contains`: it is in the box it was written in (DS-268)
    const model::Entity* e = model_.find(inner);
    up = e ? e->scope : "";
    while (!up.empty() && (up.back() == '@' || up.back() == ':')) up.pop_back();
  }
  for (; !up.empty(); up = index.parent(up))
    if (up == outer) return true;
  return false;
}

// a literal can be answered once what it tests is bound: a negation or != needs every term, in and =
// one side; a relation, a kind or a definition generates what it does not have
bool Evaluator::ready(const model::Literal& literal, const Bindings& bindings) const {
  const auto known = [&](const model::Term& t) { return !t.variable || bindings.count(t.text); };
  if (literal.negated || compares(literal.form)) return std::all_of(literal.terms.begin(), literal.terms.end(), known);
  if (literal.form == Form::In || literal.form == Form::Equal) return std::any_of(literal.terms.begin(), literal.terms.end(), known);
  return true;
}

std::vector<Bindings> Evaluator::solve(const std::string& owner, const std::vector<model::Literal>& body, const Bindings& bindings,
                                       bool first) const {
  std::vector<Bindings> out;
  solveFrom(owner, body, std::vector<bool>(body.size(), false), bindings, first, out);
  return out;
}

void Evaluator::solveFrom(const std::string& owner, const std::vector<model::Literal>& body, std::vector<bool> done,
                          const Bindings& bindings, bool first, std::vector<Bindings>& out_answers) const {
  if (first && !out_answers.empty()) return;
  // the most bound ready literal first: tests prune before generators multiply
  int best = -1;
  std::size_t bestKnown = 0;
  for (std::size_t i = 0; i < body.size(); ++i) {
    if (done[i] || !ready(body[i], bindings)) continue;
    std::size_t known = 0;
    for (const auto& t : body[i].terms) known += (!t.variable || bindings.count(t.text)) ? 1 : 0;
    if (body[i].negated) known += body[i].terms.size() + 1;
    if (best < 0 || known > bestKnown) {
      best = static_cast<int>(i);
      bestKnown = known;
    }
  }
  if (best < 0) {
    if (std::find(done.begin(), done.end(), false) == done.end()) out_answers.push_back(bindings);
    return;
  }
  done[best] = true;
  const model::Literal& l = body[best];
  if (l.negated) {
    model::Literal positive = l;
    positive.negated = false;
    if (answers(owner, positive, bindings).empty()) solveFrom(owner, body, done, bindings, first, out_answers);
    return;
  }
  for (const auto& b : answers(owner, l, bindings)) {
    solveFrom(owner, body, done, b, first, out_answers);
    if (first && !out_answers.empty()) return;
  }
}

bool Evaluator::unify(const std::string& owner, const model::Term& term, const std::string& entity, Bindings& out_bindings) const {
  std::string have;
  if (value(owner, term, out_bindings, have)) return have == entity;
  out_bindings[term.text] = entity;
  return true;
}

std::vector<Bindings> Evaluator::answers(const std::string& owner, const model::Literal& l, const Bindings& bindings) const {
  std::vector<Bindings> out;
  // bind the terms to these entities on top of `bindings`, keeping the result when they fit
  const auto offer = [&](std::initializer_list<std::pair<std::size_t, std::string>> values) {
    Bindings b = bindings;
    for (const auto& v : values)
      if (!unify(owner, l.terms[v.first], v.second, b)) return;
    out.push_back(b);
  };
  std::string a, z;
  const bool hasA = !l.terms.empty() && value(owner, l.terms[0], bindings, a);
  const bool hasZ = l.terms.size() > 1 && value(owner, l.terms[1], bindings, z);
  switch (l.form) {
    case Form::Relation: {
      const auto fw = forward_.find(l.name);
      if (fw == forward_.end()) break;
      if (hasA) {
        const auto range = fw->second.equal_range(a);
        for (auto it = range.first; it != range.second; ++it) offer({{1, it->second}});
      } else if (hasZ) {
        const auto range = backward_.at(l.name).equal_range(z);
        for (auto it = range.first; it != range.second; ++it) offer({{0, it->second}});
      } else {
        for (const auto& pair : fw->second) offer({{0, pair.first}, {1, pair.second}});
      }
      break;
    }
    case Form::Kind: {
      if (hasA) {
        const model::Entity* e = model_.find(a);
        if (e && !e->expected && l.name == model_.metamodel().nameOf(e->kind)) out.push_back(bindings);
        break;
      }
      const auto it = byKind_.find(l.name);
      if (it != byKind_.end())
        for (const auto& id : it->second) offer({{0, id}});
      break;
    }
    case Form::In: {
      if (hasA) {  // what holds a, outward
        for (std::string up = model_.index().parent(a); !up.empty(); up = model_.index().parent(up)) offer({{1, up}});
        break;
      }
      std::vector<std::string> stack{z};  // what z holds, inward
      while (!stack.empty()) {
        const std::string at = stack.back();
        stack.pop_back();
        for (const auto& kid : model_.index().children(at)) {
          offer({{0, kid}});
          stack.push_back(kid);
        }
      }
      break;
    }
    case Form::Equal:
      if (hasA && hasZ) {
        if (compare(Form::Equal, a, z)) out.push_back(bindings);
      } else if (hasA) {
        offer({{1, a}});
      } else {
        offer({{0, z}});
      }
      break;
    case Form::NotEqual:
    case Form::Less:
    case Form::LessEqual:
    case Form::Greater:
    case Form::GreaterEqual:
      if (compare(l.form, a, z)) out.push_back(bindings);
      break;
    case Form::Arg: {  // the literal a call passes to the parameter named, as the sequence writes it
      std::vector<std::string> calls;
      if (hasA) {
        calls.push_back(a);
      } else {
        const auto it = byKind_.find("message");
        if (it != byKind_.end()) calls = it->second;
      }
      const auto called = forward_.find("calls");
      if (called == forward_.end()) break;
      for (const auto& call : calls) {
        const model::Entity* message = model_.find(call);
        const auto target = called->second.find(call);
        if (!message || target == called->second.end()) continue;
        const model::Entity* op = model_.find(target->second);
        if (!op) continue;
        for (std::size_t i = 0; i < op->params.size() && i < message->args.size(); ++i)
          if (op->params[i].name == l.name && literal(message->args[i])) offer({{0, call}, {1, message->args[i]}});
      }
      break;
    }
    case Form::Call: {
      const auto ext = extension_.find(definitionOf(owner, l.name));
      if (ext == extension_.end()) break;
      // what the bindings already fix is tested before anything is copied, and a fixed first argument visits only
      // the tuples that begin with it: they lie together in the ordered extension, in the order a full pass meets
      // them (DS-338)
      std::vector<std::pair<std::size_t, std::string>> fixed;
      for (std::size_t i = 0; i < l.terms.size(); ++i) {
        std::string v;
        if (value(owner, l.terms[i], bindings, v)) fixed.push_back({i, v});
      }
      const bool byFirst = !fixed.empty() && fixed.front().first == 0;
      auto tuple = byFirst ? ext->second.lower_bound({fixed.front().second}) : ext->second.begin();
      for (; tuple != ext->second.end(); ++tuple) {
        if (byFirst && ((*tuple).empty() || (*tuple)[0] != fixed.front().second)) break;
        if (tuple->size() != l.terms.size()) continue;
        bool fits = true;
        for (std::size_t k = 0; k < fixed.size() && fits; ++k) fits = (*tuple)[fixed[k].first] == fixed[k].second;
        if (!fits) continue;
        Bindings b = bindings;
        for (std::size_t i = 0; i < tuple->size() && fits; ++i) fits = unify(owner, l.terms[i], (*tuple)[i], b);
        if (fits) out.push_back(b);
      }
      break;
    }
  }
  return out;
}

}  // namespace check
}  // namespace ds
