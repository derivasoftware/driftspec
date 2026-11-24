#include "ds/query/Pattern.hpp"

#include <utility>

namespace ds {
namespace query {

// what the pattern is written as, read once: a prefix keeps what comes before its *, and the module a trailing ::
// names; a name the model declares as a module keeps the module with its ::
Pattern::Pattern(const model::Model& model, std::string text) : text_(std::move(text)) {
  if (!text_.empty() && text_.back() == '*') {
    prefix_ = true;
    start_ = text_.substr(0, text_.size() - 1);
    if (start_.size() > 2 && start_.compare(start_.size() - 2, 2, "::") == 0) whole_ = start_.substr(0, start_.size() - 2);
    return;
  }
  const model::Entity* named = model.find(text_);
  if (named && named->kind == model::Kind::Module) {
    module_ = true;
    start_ = text_ + "::";
  }
}

bool Pattern::takes(const model::Entity& entity) const {
  if (entity.id == text_) return true;  // its identity
  // a prefix, over the identity or the scope; one ending in :: takes what is written in the module before it
  if (prefix_) return entity.id.rfind(start_, 0) == 0 || entity.scope.rfind(start_, 0) == 0 || (!whole_.empty() && entity.scope == whole_);
  // a module, what it holds at any depth
  if (module_) return entity.scope == text_ || entity.scope.rfind(start_, 0) == 0;
  // a template written without its arity
  const auto lt = entity.id.find('<');
  return lt == text_.size() && entity.id.compare(0, lt, text_) == 0;
}

}  // namespace query
}  // namespace ds
