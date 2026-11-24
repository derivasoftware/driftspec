#include "ds/query/Query.hpp"

namespace ds {
namespace query {

std::unique_ptr<IQuery> QueryFactory::of(const model::Model& model, const model::Metamodel& metamodel) {
  return std::make_unique<Query>(model, metamodel);
}

}  // namespace query
}  // namespace ds
