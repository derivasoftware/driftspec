#include "ds/check/Variables.hpp"

#include <algorithm>

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
