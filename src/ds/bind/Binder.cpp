#include "ds/bind/Binder.hpp"

#include "ds/bind/NodeBinderFactory.hpp"

namespace ds {
namespace bind {

Binder::Binder(model::Model& model, const model::Metamodel& metamodel)
    : model_(model),
      metamodel_(metamodel),
      binders_(NodeBinderFactory::make(fields_, annotations_, extensions_, types_, signatures_, formulas_, bodies_, members_)) {}

void Binder::bind(const parse::ParsedFile& file) {
  const parse::TreeReader reader(file);
  if (!dispatch_) {  // the table, from the grammar the first file was parsed with: a workspace is read by one grammar (DS-374)
    std::vector<const INodeBinder*> table;
    for (const auto& binder : binders_) table.push_back(binder.get());
    dispatch_.emplace(reader.language(), table);
  }
  const BindContext ctx{model_, notes_, reader, metamodel_, *dispatch_, "", ""};
  ctx.children(reader.root());
}

}  // namespace bind
}  // namespace ds
