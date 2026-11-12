#include "ds/model/Index.hpp"

#include "ds/model/Identity.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace model {

std::vector<Relation> Index::at(const std::map<std::string, std::vector<std::size_t>>& table, const std::string& key) const {
  std::vector<Relation> out;
  const auto found = table.find(key);
  if (found == table.end()) return out;
  out.reserve(found->second.size());
  for (const std::size_t position : found->second) out.push_back(model_.relations_[position]);
  return out;
}

std::vector<Relation> Index::from(const std::string& id) const { return at(model_.indexed_.from, id); }

std::vector<Relation> Index::to(const std::string& id) const { return at(model_.indexed_.to, id); }

std::vector<Relation> Index::touching(const std::string& id) const {
  // the two lists are in the order stated: merged, a relation from an entity to itself comes once
  static const std::vector<std::size_t> kNone;
  const auto leaving = model_.indexed_.from.find(id);
  const auto reaching = model_.indexed_.to.find(id);
  const std::vector<std::size_t>& a = leaving == model_.indexed_.from.end() ? kNone : leaving->second;
  const std::vector<std::size_t>& b = reaching == model_.indexed_.to.end() ? kNone : reaching->second;
  std::vector<Relation> out;
  out.reserve(a.size() + b.size());
  std::size_t i = 0, j = 0;
  while (i < a.size() || j < b.size()) {
    if (j == b.size() || (i < a.size() && a[i] < b[j])) {
      out.push_back(model_.relations_[a[i++]]);
    } else if (i == a.size() || b[j] < a[i]) {
      out.push_back(model_.relations_[b[j++]]);
    } else {  // the same relation, by both ends
      out.push_back(model_.relations_[a[i++]]);
      ++j;
    }
  }
  return out;
}

std::vector<Relation> Index::ofKind(const std::string& kind) const { return at(model_.indexed_.kind, kind); }

std::vector<std::string> Index::members(const std::string& scope) const {
  const auto found = model_.indexed_.members.find(scope);
  if (found == model_.indexed_.members.end()) return {};
  return {found->second.begin(), found->second.end()};
}

std::string Index::revisionOf(const std::string& artefact) const {
  const auto found = model_.indexed_.revisions.find(withoutRevision(artefact));
  // the last by identity, as a pass over the entities in their order would leave it
  return found == model_.indexed_.revisions.end() || found->second.empty() ? "" : *found->second.rbegin();
}

// What holds what is a tree: a link that would close a cycle (a model a syntax error left half
// bound can say a holds b and b holds a) is left out, so every walk up or down it ends. The
// contains relations say it first, in the order stated; then the scope of what none of them holds.
void Index::tree() const {
  Indexed& indexed = model_.indexed_;
  if (!indexed.treeStale) return;
  indexed.parent.clear();
  indexed.children.clear();
  const auto above = [&indexed](const std::string& start, const std::string& id) {
    std::set<std::string> seen;
    for (auto it = indexed.parent.find(start); it != indexed.parent.end() && seen.insert(it->first).second;
         it = indexed.parent.find(it->second))
      if (it->second == id) return true;
    return start == id;
  };
  const auto contains = indexed.kind.find("contains");
  if (contains != indexed.kind.end()) {
    for (const std::size_t position : contains->second) {
      const Relation& r = model_.relations_[position];
      if (!indexed.parent.count(r.to) && !above(r.from, r.to)) {
        indexed.parent[r.to] = r.from;
        indexed.children[r.from].push_back(r.to);
      }
    }
  }
  for (const auto& pair : model_.entities_) {
    const Entity& e = pair.second;
    if (e.expected) continue;
    if (!indexed.parent.count(e.id) && !e.scope.empty() && e.scope != e.id && model_.find(e.scope) && !above(e.scope, e.id)) {
      indexed.parent[e.id] = e.scope;
      indexed.children[e.scope].push_back(e.id);
    }
  }
  indexed.treeStale = false;
}

std::string Index::parent(const std::string& id) const {
  tree();
  const auto found = model_.indexed_.parent.find(id);
  return found == model_.indexed_.parent.end() ? "" : found->second;
}

std::vector<std::string> Index::children(const std::string& id) const {
  tree();
  const auto found = model_.indexed_.children.find(id);
  return found == model_.indexed_.children.end() ? std::vector<std::string>{} : found->second;
}

}  // namespace model
}  // namespace ds
