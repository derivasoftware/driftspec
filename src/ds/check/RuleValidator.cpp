#include "ds/check/RuleValidator.hpp"

#include <algorithm>
#include <map>

#include "ds/check/Solver.hpp"

namespace ds {
namespace check {

using Form = model::LiteralForm;

void RuleValidator::report(model::Code code, const std::string& owner, const std::string& message, const model::Location& at) {
  found_.push_back(metamodel_.finding(code, owner + " " + message, at));
}

std::vector<std::pair<std::string, std::regex>> RuleValidator::takePatterns() {
  std::vector<std::pair<std::string, std::regex>> out;
  out.swap(patterns_);
  return out;
}

std::vector<model::Finding> RuleValidator::take() {
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

// Everything a formula names exists in the metamodel or the model, every definition it applies takes
// that many arguments, and every variable is bound by a positive literal before anything tests it.
bool RuleValidator::validate(const std::string& id, const model::Formula& formula,
                             std::vector<std::pair<std::string, std::string>>& out_resolved) {
  model::Kind kind;
  if (!facts_.kindOf(id, kind)) return false;
  const std::string written = facts_.scopeOf(id);
  bool ok = true;
  std::set<std::string> bound;
  for (const auto& range : formula.ranges) {
    if (!metamodel_.specNamed(range.kind)) {
      report(model::Code::E_RULE_UNKNOWN, id, "ranges over '" + range.kind + "', which is no kind of the language", range.location);
      ok = false;
    }
    if (!range.scope.empty()) {
      const std::string scope = resolver_.resolve(range.scope, written);
      if (scope.empty()) {
        report(model::Code::E_RULE_UNKNOWN, id, "ranges inside '" + range.scope + "', which nothing declares", range.location);
        ok = false;
      }
      out_resolved.push_back({range.scope, scope});
    }
    bound.insert(range.variable);
  }
  if (kind == model::Kind::Diagram) {
    ok = validateBody(id, formula.where, bound, out_resolved) && ok;
    for (const auto& column : formula.columns) {
      std::set<std::string> inColumn = bound;
      if (column.form != "field" && column.form != "property") ok = validateBody(id, column.body, inColumn, out_resolved) && ok;
    }
  } else if (kind == model::Kind::Rule) {
    ok = validateBody(id, formula.where, bound, out_resolved) && ok;
    for (const auto& claim : formula.claims) {
      std::set<std::string> inClaim = bound;
      ok = validateBody(id, claim, inClaim, out_resolved) && ok;
    }
  } else {
    const std::string name = facts_.fieldOf(id, "name");
    if (name == "name" || name == "id" || name == "kind") {  // what a rule reads of an entity: never a definition (DS-403)
      report(model::Code::E_RULE_RESERVED, id, "takes the name '" + name + "', which a rule reads as " + name + "(x): name it otherwise",
             facts_.locationOf(id));
      ok = false;
    }
    for (const auto& c : formula.cases) {
      std::set<std::string> inCase;
      ok = validateBody(id, c, inCase, out_resolved) && ok;
      for (const auto& p : formula.parameters) {
        if (inCase.count(p) || c.empty()) continue;
        report(model::Code::E_RULE_UNSAFE, id,
               "leaves its parameter '" + p + "' unbound in one of its cases: name it in a positive literal", c.front().location);
        ok = false;
      }
    }
  }
  return ok;
}

// The pattern a value matches is a string, compiled here once for every binding the solver will test (DS-403); one
// that does not compile is a finding at the literal, and the formula is not evaluated.
bool RuleValidator::validatePattern(const std::string& id, const model::Literal& l) {
  const model::Term& pattern = l.terms[1];
  const std::string& written = pattern.text;
  if (!pattern.value || written.size() < 2 || written.front() != '"' || written.back() != '"') {
    report(model::Code::E_RULE_PATTERN, id, "matches against '" + written + "', which is no string: write the pattern in quotes",
           l.location);
    return false;
  }
  try {
    patterns_.push_back({written, std::regex(written.substr(1, written.size() - 2), std::regex::ECMAScript)});
  } catch (const std::regex_error& e) {
    report(model::Code::E_RULE_PATTERN, id, "matches against " + written + ", which does not compile: " + e.what(), l.location);
    return false;
  }
  return true;
}

bool RuleValidator::joinsAtEnd(const model::RelationSpec& spec, model::Kind kind, bool from) {
  if (!spec.pairs.empty()) {
    for (const auto& p : spec.pairs)
      if ((from ? p.first : p.second) == kind) return true;
    return false;
  }
  const auto& ends = from ? spec.from : spec.to;
  return ends.empty() || std::find(ends.begin(), ends.end(), kind) != ends.end();
}

// A relation literal whose end is bound to a kind the relation never leaves or reaches can never match: a rule
// that rests on it is decided by vacuity, always holding or never broken (DS-213). Reported, not refused.
void RuleValidator::checkVacuity(const std::string& id, const model::Formula& formula) {
  std::map<std::string, model::Kind> kindOf;  // variable -> the kind its range or a kind literal binds it to
  for (const auto& r : formula.ranges)
    if (const model::KindSpec* k = metamodel_.specNamed(r.kind)) kindOf[r.variable] = k->kind;
  std::vector<const std::vector<model::Literal>*> bodies{&formula.where};
  for (const auto& c : formula.claims) bodies.push_back(&c);
  for (const auto& c : formula.cases) bodies.push_back(&c);
  for (const auto* body : bodies)
    for (const auto& l : *body)
      if (l.form == Form::Kind && !l.negated && !l.terms.empty() && l.terms[0].variable)
        if (const model::KindSpec* k = metamodel_.specNamed(l.name)) kindOf.emplace(l.terms[0].text, k->kind);
  const std::string scope = facts_.scopeOf(id);
  const auto kindOfTerm = [&](const model::Term& t, model::Kind* out) {
    if (t.value) return false;
    if (t.variable) {
      const auto it = kindOf.find(t.text);
      if (it == kindOf.end()) return false;
      *out = it->second;
      return true;
    }
    return facts_.kindOf(resolver_.resolve(t.text, scope), *out);
  };
  for (const auto* body : bodies)
    for (const auto& l : *body) {
      if (l.form != Form::Relation || l.terms.size() != 2) continue;
      const model::RelationSpec* spec = metamodel_.relationOf(l.name);
      if (!spec) continue;
      for (int end = 0; end < 2; ++end) {
        model::Kind kind;
        if (!kindOfTerm(l.terms[end], &kind) || joinsAtEnd(*spec, kind, end == 0)) continue;
        found_.push_back(metamodel_.finding(model::Code::W_RULE_NEVER_MATCHES,
                                            id + " writes '" + l.terms[0].text + " " + l.name + " " + l.terms[1].text + "', and " + l.name +
                                                (end == 0 ? " never leaves " : " never reaches ") + metamodel_.nameOf(kind) + " (" +
                                                l.terms[end].text + "): the literal matches nothing",
                                            l.location));
        break;
      }
    }
}

bool RuleValidator::validateBody(const std::string& id, const std::vector<model::Literal>& body, std::set<std::string>& out_bound,
                                 std::vector<std::pair<std::string, std::string>>& out_resolved) {
  bool ok = true;
  const std::string scope = facts_.scopeOf(id);
  for (const auto& l : body) {
    if (l.form == Form::Relation && !metamodel_.relationOf(l.name)) {
      report(model::Code::E_RULE_UNKNOWN, id, "names the relation '" + l.name + "', which the metamodel does not declare", l.location);
      ok = false;
    } else if (l.form == Form::Kind && !metamodel_.specNamed(l.name)) {
      report(model::Code::E_RULE_UNKNOWN, id, "names the kind '" + l.name + "', which is no kind of the language", l.location);
      ok = false;
    } else if (l.form == Form::Matches) {
      ok = validatePattern(id, l) && ok;
    } else if (l.form == Form::Call) {
      const std::string d = resolver_.resolve(l.name, scope);
      if (d.empty() || !facts_.kindIs(d, model::Kind::Definition)) {
        report(model::Code::E_RULE_UNKNOWN, id, "applies '" + l.name + "', which no definition in scope declares", l.location);
        ok = false;
      } else {
        out_resolved.push_back({"call:" + l.name, d});
        const auto it = facts_.formulas().find(d);
        const std::size_t arity = it == facts_.formulas().end() ? 0 : it->second.parameters.size();
        if (arity != l.terms.size()) {
          report(model::Code::E_RULE_ARITY, id,
                 "applies " + d + " to " + std::to_string(l.terms.size()) + " arguments; it takes " + std::to_string(arity), l.location);
          ok = false;
        }
      }
    }
    for (const auto& t : l.terms) {
      if (t.variable || t.value) continue;
      const std::string entity = resolver_.resolve(t.text, scope);
      if (entity.empty()) {
        report(model::Code::E_RULE_UNKNOWN, id, "names '" + t.text + "', which nothing declares", l.location);
        ok = false;
      }
      out_resolved.push_back({t.text, entity});
    }
  }
  // safety: what positive literals bind, to a fixpoint; any variable left is never out_bound
  const auto known = [&](const model::Term& t) { return !t.variable || out_bound.count(t.text); };
  for (bool changed = true; changed;) {
    changed = false;
    for (const auto& l : body) {
      if (l.negated || Solver::compares(l.form)) continue;
      const bool binds = l.form == Form::Relation || l.form == Form::Kind || l.form == Form::Call || l.form == Form::Arg ||
                         std::any_of(l.terms.begin(), l.terms.end(), known);  // in and = bind one side from the other
      if (!binds) continue;
      // name(x) reads what x is bound to and never binds x: the entity comes from a positive literal (DS-403)
      for (const auto& t : l.terms)
        if (t.variable && t.property.empty() && out_bound.insert(t.text).second) changed = true;
    }
  }
  std::set<std::string> told;
  for (const auto& l : body)
    for (const auto& t : l.terms)
      if (!known(t) && told.insert(t.text).second) {
        report(model::Code::E_RULE_UNSAFE, id,
               "leaves the variable '" + t.text + "' unbound: a negation or a comparison tests only what a positive literal binds",
               l.location);
        ok = false;
      }
  return ok;
}

}  // namespace check
}  // namespace ds
