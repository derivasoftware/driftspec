#include "ds/check/Variables.hpp"

#include <algorithm>
#include <utility>

namespace ds {
namespace check {

Variables::Variables(const model::Formula& formula) {
  for (const auto& r : formula.ranges) names_.push_back(r.variable);
  collect(formula.where);
  for (const auto& c : formula.claims) collect(c);
  for (const auto& c : formula.cases) collect(c);
  for (const auto& p : formula.parameters) names_.push_back(p);
  for (const auto& column : formula.columns) {
    if (!column.variable.empty()) names_.push_back(column.variable);
    collect(column.body);
  }
  std::sort(names_.begin(), names_.end());
  names_.erase(std::unique(names_.begin(), names_.end()), names_.end());
  // every body of the formula compiled once, now its names are known (DS-507)
  const auto keep = [this](const std::vector<model::Literal>& body) { bodies_.emplace(&body, compile(body)); };
  keep(formula.where);
  for (const auto& c : formula.claims) keep(c);
  for (const auto& c : formula.cases) keep(c);
  for (const auto& column : formula.columns) keep(column.body);
}

Variables::Slots Variables::compile(const std::vector<model::Literal>& body) const {
  Slots out;
  out.reserve(body.size());
  for (const auto& l : body) {
    std::vector<int> slots;
    slots.reserve(l.terms.size());
    for (const auto& t : l.terms) slots.push_back(t.variable ? slotOf(t.text) : -1);
    out.push_back(std::move(slots));
  }
  return out;
}

const Variables::Slots& Variables::slotsOf(const std::vector<model::Literal>& body) const {
  const auto it = bodies_.find(&body);
  if (it != bodies_.end()) return it->second;
  return bodies_.emplace(&body, compile(body)).first->second;
}

void Variables::collect(const std::vector<model::Literal>& body) {
  for (const auto& l : body)
    for (const auto& t : l.terms)
      if (t.variable) names_.push_back(t.text);
}

bool Variables::before(const std::vector<const std::string*>& a, const std::vector<const std::string*>& b) const {
  const std::size_t n = names_.size();
  for (std::size_t i = 0, j = 0;; ++i, ++j) {
    while (i < n && !a[i]) ++i;
    while (j < n && !b[j]) ++j;
    if (j == n) return false;  // b has no pair left: a is not before it
    if (i == n) return true;   // a ran out first
    if (i != j) return i < j;  // the names differ: the slots are in the order of the names
    if (*a[i] != *b[j]) return *a[i] < *b[j];
  }
}

std::vector<std::pair<std::string, std::string>> Variables::pairs(const std::vector<const std::string*>& bindings) const {
  std::vector<std::pair<std::string, std::string>> out;
  for (std::size_t i = 0; i < names_.size(); ++i)
    if (bindings[i]) out.push_back({names_[i], *bindings[i]});
  return out;
}

int Variables::slotOf(const std::string& name) const {
  const auto it = std::lower_bound(names_.begin(), names_.end(), name);
  return it == names_.end() || *it != name ? -1 : static_cast<int>(it - names_.begin());
}

}  // namespace check
}  // namespace ds
