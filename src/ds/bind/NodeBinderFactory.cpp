#include "ds/bind/NodeBinderFactory.hpp"

#include "ds/bind/ArtefactBinder.hpp"
#include "ds/bind/ClassBinder.hpp"
#include "ds/bind/DiagramBinder.hpp"
#include "ds/bind/EnumBinder.hpp"
#include "ds/bind/FunctionBinder.hpp"
#include "ds/bind/InterfaceBinder.hpp"
#include "ds/bind/ModuleBinder.hpp"
#include "ds/bind/PackageBinder.hpp"
#include "ds/bind/RuleBinder.hpp"
#include "ds/bind/SequenceBinder.hpp"
#include "ds/bind/StateBinder.hpp"
#include "ds/bind/UseCaseBinder.hpp"

namespace ds {
namespace bind {

// the one place that names the concrete binders, each given the parts it uses (DS-365, DS-404)
std::vector<std::unique_ptr<INodeBinder>> NodeBinderFactory::make(const FieldBinder& fields, const AnnotationBinder& annotations,
                                                                  const ExtensionBinder& extensions, const TypeBinder& types,
                                                                  const SignatureBinder& signatures, const FormulaBinder& formulas,
                                                                  const MemberBodyBinder& bodies, const MemberBinder& members) {
  std::vector<std::unique_ptr<INodeBinder>> out;
  out.push_back(std::make_unique<ArtefactBinder>(fields, annotations, extensions));
  out.push_back(std::make_unique<PackageBinder>(fields, annotations));
  out.push_back(std::make_unique<ModuleBinder>(fields, annotations));
  out.push_back(std::make_unique<InterfaceBinder>(fields, annotations, signatures));
  out.push_back(std::make_unique<EnumBinder>(annotations));
  out.push_back(std::make_unique<RuleBinder>(fields, annotations, formulas));
  out.push_back(std::make_unique<DiagramBinder>(fields, formulas));
  out.push_back(std::make_unique<UseCaseBinder>(fields, annotations, types, signatures));
  out.push_back(std::make_unique<StateBinder>(fields, annotations));
  out.push_back(std::make_unique<ClassBinder>(fields, annotations, extensions, members));
  out.push_back(std::make_unique<FunctionBinder>(signatures, bodies));
  out.push_back(std::make_unique<SequenceBinder>(fields, annotations, types));
  return out;
}

}  // namespace bind
}  // namespace ds
