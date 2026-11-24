#include "ds/bind/Binder.hpp"

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

Binder::Binder(model::Model& model, const model::Metamodel& metamodel)
    : model_(model),
      metamodel_(metamodel),
      binders_([this] {  // the one place that names the concrete binders, each given the parts it uses (DS-365)
        std::vector<std::unique_ptr<INodeBinder>> out;
        out.push_back(std::make_unique<ArtefactBinder>(fields_, annotations_, extensions_));
        out.push_back(std::make_unique<PackageBinder>(fields_, annotations_));
        out.push_back(std::make_unique<ModuleBinder>(fields_, annotations_));
        out.push_back(std::make_unique<InterfaceBinder>(fields_, annotations_, signatures_));
        out.push_back(std::make_unique<EnumBinder>(annotations_));
        out.push_back(std::make_unique<RuleBinder>(fields_, annotations_, formulas_));
        out.push_back(std::make_unique<DiagramBinder>(fields_, formulas_));
        out.push_back(std::make_unique<UseCaseBinder>(fields_, annotations_, types_, signatures_));
        out.push_back(std::make_unique<StateBinder>(fields_, annotations_));
        out.push_back(std::make_unique<ClassBinder>(fields_, annotations_, extensions_, members_));
        out.push_back(std::make_unique<FunctionBinder>(signatures_, bodies_));
        out.push_back(std::make_unique<SequenceBinder>(fields_, annotations_, types_));
        return out;
      }()),
      dispatch_(parse::Parser::language(), [this] {
        std::vector<const INodeBinder*> table;
        for (const auto& binder : binders_) table.push_back(binder.get());
        return table;
      }()) {}

void Binder::bind(const parse::ParsedFile& file) {
  const parse::TreeReader reader(file);
  const BindContext ctx{model_, notes_, reader, metamodel_, dispatch_, "", ""};
  ctx.children(reader.root());
}

}  // namespace bind
}  // namespace ds
