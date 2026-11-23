#include "ds/bind/Dispatch.hpp"

#include <cstring>
#include <stdexcept>
#include <string>

#include "ds/bind/BindContext.hpp"

namespace ds {
namespace bind {

Dispatch::Dispatch(const TSLanguage* language, const std::vector<const INodeBinder*>& binders)
    : bySymbol_(ts_language_symbol_count(language), nullptr) {
  for (const INodeBinder* binder : binders) {
    for (const std::string& kind : binder->kinds()) {
      // the named node of that name; ts_node_symbol gives the same public symbol for every node of the kind
      const TSSymbol symbol = ts_language_symbol_for_name(language, kind.c_str(), static_cast<uint32_t>(kind.size()), true);
      if (symbol == 0 || std::strcmp(ts_language_symbol_name(language, symbol), kind.c_str()) != 0)
        throw std::logic_error("no node of the grammar is named " + kind);
      if (bySymbol_[symbol] && bySymbol_[symbol] != binder) throw std::logic_error("two binders take the node " + kind);
      bySymbol_[symbol] = binder;
    }
  }
}

void Dispatch::bindChildren(TSNode node, const BindContext& ctx) const {
  const uint32_t count = ts_node_named_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    const TSNode child = ts_node_named_child(node, i);
    if (const INodeBinder* binder = of(child)) binder->bind(child, ctx);
  }
}

}  // namespace bind
}  // namespace ds
