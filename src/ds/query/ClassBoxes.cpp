#include "ds/query/ClassBoxes.hpp"

#include "ds/model/Identity.hpp"

namespace ds {
namespace query {

// what a class diagram draws as a box: a class, an enum, an interface or a function of a module
bool ClassBoxes::isBox(const model::Entity& entity) {
  return entity.kind == model::Kind::Class || entity.kind == model::Kind::Enum || entity.kind == model::Kind::Function ||
         entity.kind == model::Kind::Interface;
}

// a box that holds others: a module, or a package below the system (the outermost package is the drawing)
bool ClassBoxes::isContainer(const model::Entity& entity) {
  return entity.kind == model::Kind::Module || (entity.kind == model::Kind::Package && !entity.scope.empty());
}

bool ClassBoxes::drawable(const model::Entity& entity) { return isBox(entity) || isContainer(entity); }

// the relations a class diagram draws as edges, the ones its neighbours follow
bool ClassBoxes::drawnAsEdge(const std::string& relation) {
  return relation == "inherits" || relation == "owns" || relation == "refs" || relation == "shares" || relation == "uses" ||
         relation == "bounded-by" || relation == "realises" || relation == "consumes" || relation == "sends";
}

std::string ClassBoxes::drawnFrom(const model::Model& model, const model::Entity& entity) {
  if (isBox(entity) || entity.kind == model::Kind::Module || entity.kind == model::Kind::Package)
    return entity.id;  // a box's own @consumes
  if (entity.kind == model::Kind::Sequence || entity.kind == model::Kind::State) {
    const model::Entity* subject = model.find(model::Identity(entity.id).subject().text());
    if (!subject && entity.kind == model::Kind::State) subject = model.find(model::Identity(entity.id).machineSubject().text());
    if (subject) return isBox(*subject) ? subject->id : subject->scope;
  }
  return entity.scope;
}

std::set<std::string> ClassBoxes::containers(const model::Model& model, const std::set<std::string>& taken) {
  // modules of the selected classes become containers (R2), and so do nested packages (the HLD
  // boxes); the outermost package is the drawing itself
  std::set<std::string> out;
  auto up = [](const std::string& s) {
    return model::Scope(model::Scope(s).outer().text()).box().text();
  };  // ds@ds::b → ds@ds, app@core@model → app@core
  // every box is drawn, with or without children (DS-218): a container taken is drawn itself, and the boxes
  // around anything taken are drawn around it; a package scope (p@q@) names the package p@q
  for (const auto& id : taken) {
    const model::Entity* e = model.find(id);
    if (!e || (!isBox(*e) && !isContainer(*e))) continue;
    const std::string start = isContainer(*e) ? id : model::Scope(e->scope).box().text();
    for (std::string s = start; !s.empty(); s = up(s)) {
      const model::Entity* m = model.find(s);
      if (m && isContainer(*m)) out.insert(s);
    }
  }
  return out;
}

}  // namespace query
}  // namespace ds
