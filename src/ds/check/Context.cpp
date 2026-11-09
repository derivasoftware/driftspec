#include "ds/check/Context.hpp"

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace check {

using model::outer;

std::string Context::current(const std::string& id) const {
  for (const auto& r : renamed_) {
    if (id == r.first) return r.second;
    if (model::within(id, r.first)) return r.second + id.substr(r.first.size());
  }
  return id;
}

void Context::checkVisibility(const std::string& from, const std::string& target, const model::Location& location) {
  const std::string hidden = resolver_.privateOnTheWay(from, target);
  if (hidden.empty()) return;
  report({"W-NOT-VISIBLE", "warning",
          from + " reaches " + target + (hidden == target ? ", which is private" : " through " + hidden + ", which is private"), location});
}

void Context::checkLeak(const std::string& from, const std::string& target, const model::Location& location) {
  const model::Entity* type = model_.find(target);
  const model::Entity* member = model_.find(from);
  if (!type || !member || type->expected) return;
  if (member->kind != model::Kind::Method && member->kind != model::Kind::Attribute && member->kind != model::Kind::Function &&
      member->kind != model::Kind::Operation && member->kind != model::Kind::Operator) {  // an operator has a signature too (DS-117)
    return;
  }
  const auto hidden = type->fields.find("visibility");
  if (hidden == type->fields.end() || hidden->second != "-") return;
  // the member shows the type to whoever can see the member: from outside the type's parent, if all is public on the way
  const std::string parent = outer(target);
  for (std::string s = from; s != parent; s = outer(s)) {
    if (s.empty()) return;  // the member lives elsewhere: that is W-NOT-VISIBLE's
    const model::Entity* e = model_.find(s);
    if (!e) return;
    const auto visibility = e->fields.find("visibility");
    if (visibility != e->fields.end() && visibility->second == "-") return;
  }
  report({"W-PRIVATE-LEAK", "warning", from + " is public and shows " + target + ", which is private", location});
}

std::vector<model::Finding> Context::take() {
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

}  // namespace check
}  // namespace ds
