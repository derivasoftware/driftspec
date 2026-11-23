#include "ds/bind/Binder.hpp"

namespace ds {
namespace bind {

Binder::Binder(model::Model& model, const model::Metamodel& metamodel)
    : model_(model), metamodel_(metamodel), dispatch_(parse::Parser::language(), {&artefacts_, &legacy_}) {}

void Binder::bind(const parse::ParsedFile& file) {
  const parse::TreeReader reader(file);
  const BindContext ctx{model_, notes_, reader, metamodel_, dispatch_, "", ""};
  ctx.children(reader.root());
}

}  // namespace bind
}  // namespace ds
