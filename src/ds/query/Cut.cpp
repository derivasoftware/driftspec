#include "ds/query/Cut.hpp"

#include <vector>

#include "ds/model/Identity.hpp"
#include "ds/model/Words.hpp"

namespace ds {
namespace query {

namespace {

std::string outerOf(const std::string& id) { return model::Scope(model::Scope(id).outer().text()).box().text(); }  // the box outside

std::vector<std::string> pathTo(const std::string& id) {  // from the outermost box down to the id
  std::vector<std::string> path;
  for (std::string s = id; !s.empty(); s = outerOf(s)) path.insert(path.begin(), s);
  return path;
}

/// A box that holds others and can close: a module, or a package below the outermost one.
bool closes(const model::Entity& e) {
  return !e.expected && (e.kind == model::Kind::Module || (e.kind == model::Kind::Package && !e.scope.empty()));
}

/// Whether a pattern names a box: its identity, a prefix of it with *, or its kind.
bool names(const std::string& pattern, const model::Entity& box) {
  if (pattern == box.id || pattern == model::kindName(box.kind)) return true;
  return !pattern.empty() && pattern.back() == '*' && box.id.compare(0, pattern.size() - 1, pattern, 0, pattern.size() - 1) == 0;
}

bool namedBy(const std::vector<std::string>& patterns, const model::Entity& box) {
  for (const auto& p : patterns)
    if (names(p, box)) return true;
  return false;
}

}  // namespace

Cut::Cut(const model::Model& model, const model::Entity& diagram) {
  // what the diagram writes, and what the diagram it takes its cut from writes, and so on: each once
  std::vector<std::string> open, close;
  bool opens = false, closesSome = false;
  std::set<std::string> taken;
  for (const model::Entity* d = &diagram; d && d->kind == model::Kind::Diagram && taken.insert(d->id).second;
       d = d->fields.count("cut") ? model.find("diagram:" + d->field("cut")) : nullptr) {
    opens = opens || d->fields.count("open") > 0;
    closesSome = closesSome || d->fields.count("close") > 0;
    for (const auto& p : model::split(d->field("open"), ',')) open.push_back(p);
    for (const auto& p : model::split(d->field("close"), ',')) close.push_back(p);
  }
  written_ = opens || closesSome;
  if (!written_) return;

  for (const auto& pair : model.entities()) {
    const model::Entity& box = pair.second;
    // `close:` draws the line; with none written, `open:` alone closes every box
    if (closes(box) && (closesSome ? namedBy(close, box) : true)) closed_.insert(box.id);
  }
  // `open:` goes through it: the box it names stays open, and so does every box on the way to it
  for (const auto& pair : model.entities()) {
    if (!closes(pair.second) || !namedBy(open, pair.second)) continue;
    for (const auto& step : pathTo(pair.first)) closed_.erase(step);
  }
}

std::string Cut::standsFor(const std::string& id) const {
  for (const auto& step : pathTo(id)) {
    if (closed_.count(step)) return step;
  }
  return id;
}

}  // namespace query
}  // namespace ds
