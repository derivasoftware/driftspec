#include "ds/model/Index.hpp"

#include <algorithm>

#include "ds/model/Identity.hpp"
#include "ds/model/Metamodel.hpp"
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

std::vector<Relation> Index::atOfKind(const std::map<std::string, std::vector<std::size_t>>& table, const std::string& key,
                                      const std::string& kind) const {
  std::vector<Relation> out;
  const auto found = table.find(key);
  if (found == table.end()) return out;
  for (const std::size_t position : found->second)
    if (model_.relations_[position].kind == kind) out.push_back(model_.relations_[position]);
  return out;
}

std::vector<Relation> Index::from(const std::string& id) const { return at(model_.indexed_.from, id); }

std::vector<Relation> Index::to(const std::string& id) const { return at(model_.indexed_.to, id); }

std::vector<Relation> Index::fromOfKind(const std::string& id, const std::string& kind) const {
  return atOfKind(model_.indexed_.from, id, kind);
}

std::vector<Relation> Index::toOfKind(const std::string& id, const std::string& kind) const {
  return atOfKind(model_.indexed_.to, id, kind);
}

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

std::vector<Relation> Index::ofKinds(const std::vector<std::string>& kinds) const {
  // each kind's positions are in the order stated: together, sorted, they are again
  std::vector<std::size_t> positions;
  for (const auto& kind : kinds) {
    const auto found = model_.indexed_.kind.find(kind);
    if (found != model_.indexed_.kind.end()) positions.insert(positions.end(), found->second.begin(), found->second.end());
  }
  std::sort(positions.begin(), positions.end());
  std::vector<Relation> out;
  out.reserve(positions.size());
  for (const std::size_t position : positions) out.push_back(model_.relations_[position]);
  return out;
}

std::vector<Relation> Index::all() const { return model_.relations_; }

std::size_t Index::count() const { return model_.relations_.size(); }

std::vector<std::string> Index::members(const std::string& scope) const {
  const auto found = model_.indexed_.members.find(scope);
  if (found == model_.indexed_.members.end()) return {};
  return {found->second.begin(), found->second.end()};
}

std::string Index::revisionOf(const std::string& artefact) const {
  const auto found = model_.indexed_.revisions.find(model::ArtefactName(artefact).withoutRevision().text());
  // the last by identity, as a pass over the entities in their order would leave it
  return found == model_.indexed_.revisions.end() || found->second.empty() ? "" : *found->second.rbegin();
}

std::vector<std::string> Index::named(const std::string& package, const std::string& name) const {
  const auto found = model_.indexed_.named.find({package, name});
  if (found == model_.indexed_.named.end()) return {};
  return std::vector<std::string>(found->second.begin(), found->second.end());
}

// What holds what is a tree: a link that would close a cycle (a model a syntax error left half
// bound can say a holds b and b holds a) is left out, so every walk up or down it ends. The
// contains relations say it first, in the order stated; then the scope of what none of them holds.
void Index::tree() const {
  Indexed& indexed = model_.indexed_;
  if (!indexed.treeStale) return;
  indexed.parent.clear();
  indexed.children.clear();
  // what was read from the tree before is dropped with it: the intervals and the chains are numbered again when asked
  indexed.span.clear();
  indexed.holders.clear();
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

std::vector<std::string> Index::childrenOfKind(const std::string& id, const std::string& kind) const {
  tree();
  std::vector<std::string> out;
  const auto found = model_.indexed_.children.find(id);
  if (found == model_.indexed_.children.end()) return out;
  for (const std::string& child : found->second) {
    const Entity* e = model_.find(child);
    if (e && model_.metamodel_->nameOf(e->kind) == kind) out.push_back(child);
  }
  return out;
}

// Every node of the tree numbered once, in pre-order from each top: entering a node, then its children in the order
// the tree holds them, then leaving it. A node's descendants are those entered after it and left no later.
void Index::spans() const {
  tree();
  Indexed& indexed = model_.indexed_;
  if (!indexed.span.empty() || indexed.children.empty()) return;
  std::size_t clock = 0;
  std::vector<std::pair<const std::string*, std::size_t>> stack;  // a node and the next of its children to enter
  for (const auto& top : indexed.children) {
    if (indexed.parent.count(top.first)) continue;  // not a top: entered from what holds it
    indexed.span[top.first].first = clock++;
    stack.push_back({&top.first, 0});
    while (!stack.empty()) {
      auto& [node, next] = stack.back();
      const auto held = indexed.children.find(*node);
      if (held != indexed.children.end() && next < held->second.size()) {
        const std::string& child = held->second[next++];
        indexed.span[child].first = clock++;
        stack.push_back({&child, 0});
      } else {
        indexed.span[*node].second = clock++;
        stack.pop_back();
      }
    }
  }
}

bool Index::descends(const std::string& id, const std::string& ancestor) const {
  if (id == ancestor) return false;
  spans();
  const auto& span = model_.indexed_.span;
  const auto inner = span.find(id);
  const auto outer = span.find(ancestor);
  if (inner == span.end() || outer == span.end()) return false;
  return outer->second.first < inner->second.first && inner->second.second < outer->second.second;
}

const std::vector<std::string>& Index::holders(const std::string& id) const {
  tree();
  Indexed& indexed = model_.indexed_;
  const auto kept = indexed.holders.find(id);
  if (kept != indexed.holders.end()) return kept->second;
  std::vector<std::string> chain;
  for (auto up = indexed.parent.find(id); up != indexed.parent.end(); up = indexed.parent.find(up->second))
    chain.push_back(up->second);  // the tree has no cycle: every walk up ends
  return indexed.holders.emplace(id, std::move(chain)).first->second;
}

}  // namespace model
}  // namespace ds
