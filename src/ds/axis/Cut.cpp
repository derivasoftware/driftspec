#include "ds/axis/Cut.hpp"

#include <vector>

#include "ds/model/Identity.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace axis {

std::string Cut::outerOf(const std::string& id) { return model::Scope(model::Scope(id).outer().text()).box().text(); }

std::vector<std::string> Cut::pathTo(const std::string& id) {
  std::vector<std::string> path;
  for (std::string s = id; !s.empty(); s = outerOf(s)) path.insert(path.begin(), s);
  return path;
}

bool Cut::closes(const model::Entity& e) {
  return !e.expected && (e.kind == model::Kind::Module || (e.kind == model::Kind::Package && !e.scope.empty()));
}

bool Cut::names(const model::Metamodel& metamodel, const std::string& pattern, const model::Entity& box) {
  if (pattern == box.id || pattern == metamodel.nameOf(box.kind)) return true;
  return !pattern.empty() && pattern.back() == '*' && box.id.compare(0, pattern.size() - 1, pattern, 0, pattern.size() - 1) == 0;
}

bool Cut::namedBy(const model::Metamodel& metamodel, const std::vector<std::string>& patterns, const model::Entity& box) {
  for (const auto& p : patterns)
    if (names(metamodel, p, box)) return true;
  return false;
}

bool Cut::reaches(const std::vector<std::string>& patterns, const std::string& operation) {
  for (const auto& p : patterns) {
    if (p == operation) return true;
    if (!p.empty() && p.back() == '*' && operation.compare(0, p.size() - 1, p, 0, p.size() - 1) == 0) return true;
    if (operation.rfind(p + "::", 0) == 0 || operation.rfind(p + "@", 0) == 0) return true;
  }
  return false;
}

Cut::Cut(const model::Model& model, const model::Metamodel& metamodel, const model::Entity& diagram)
    : unfold_(text::splitTrimmed(diagram.field("unfold"), ',')),
      fold_(text::splitTrimmed(diagram.field("fold"), ',')),
      unfolds_(diagram.fields.count("unfold") > 0),
      folds_(diagram.fields.count("fold") > 0),
      deep_(diagram.fields.count("depth") > 0) {
  // what the diagram writes, and what the diagram it takes its cut from writes, and so on: each once
  std::vector<std::string> open, close;
  bool opens = false, closesSome = false;
  std::set<std::string> taken;
  for (const model::Entity* d = &diagram; d && d->kind == model::Kind::Diagram && taken.insert(d->id).second;
       d = d->fields.count("cut") ? model.find("diagram:" + d->field("cut")) : nullptr) {
    opens = opens || d->fields.count("open") > 0;
    closesSome = closesSome || d->fields.count("close") > 0;
    for (const auto& p : text::splitTrimmed(d->field("open"), ',')) open.push_back(p);
    for (const auto& p : text::splitTrimmed(d->field("close"), ',')) close.push_back(p);
  }
  written_ = opens || closesSome;
  if (!written_) return;

  for (const auto& pair : model.entities()) {
    const model::Entity& box = pair.second;
    // `close:` draws the line; with none written, `open:` alone closes every box
    if (closes(box) && (closesSome ? namedBy(metamodel, close, box) : true)) closed_.insert(box.id);
  }
  // `open:` goes through it: the box it names stays open, and so does every box on the way to it
  for (const auto& pair : model.entities()) {
    if (!closes(pair.second) || !namedBy(metamodel, open, pair.second)) continue;
    for (const auto& step : pathTo(pair.first)) closed_.erase(step);
  }
}

std::string Cut::bondOf(const std::string& relation) {
  for (const char* structural : {"owns", "refs", "shares", "inherits", "realises"})
    if (relation == structural) return "structural";
  for (const char* use : {"uses", "bounded-by", "consumes", "calls", "sends", "couples-transient", "couples-structural"})
    if (relation == use) return "uses";
  return "";
}

std::string Cut::standsAmong(const std::string& id, const std::set<std::string>& boxes) {
  for (const auto& step : pathTo(id)) {
    if (boxes.count(step)) return step;
  }
  return "";
}

std::string Cut::standsFor(const std::string& id) const {
  const std::string box = standsAmong(id, closed_);
  return box.empty() ? id : box;
}

bool Cut::follows(const std::string& operation, int left) const {
  if (reaches(unfold_, operation)) return true;  // unfold: goes through fold:
  if (reaches(fold_, operation)) return false;
  if (deep_) return left > 0;  // the uniform case
  if (folds_) return true;     // fold: draws the line: every other call is followed
  if (unfolds_) return false;  // unfold: alone folds every other call
  return written_;             // with a cut to stand on, every call is followed
}

}  // namespace axis
}  // namespace ds
