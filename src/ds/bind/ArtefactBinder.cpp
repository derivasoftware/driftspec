#include "ds/bind/ArtefactBinder.hpp"

#include "ds/text/Text.hpp"

namespace ds {
namespace bind {

using parse::Field;
using parse::TreeReader;

std::vector<std::string> ArtefactBinder::kinds() const { return {"requirement", "verification", "constraint"}; }

void ArtefactBinder::bind(TSNode node, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(node);
  if (ExtensionBinder::extends(node, ctx.reader.child(node, Field::id))) {
    const model::Kind kind = TreeReader::is(node, "requirement")    ? model::Kind::Requirement
                             : TreeReader::is(node, "verification") ? model::Kind::Verification
                                                                    : model::Kind::Constraint;
    extensions_.bind(node, ctx, ctx.reader.fieldText(node, Field::id), kind, ctx.reader.fieldText(node, Field::id), scope, loc);
    return;
  }
  model::Entity e;
  e.kind = TreeReader::is(node, "requirement")    ? model::Kind::Requirement
           : TreeReader::is(node, "verification") ? model::Kind::Verification
                                                  : model::Kind::Constraint;
  e.name = ctx.reader.fieldText(node, Field::id);
  e.id = e.name;  // process artefacts are identified by id (R5)
  e.scope = scope;
  e.location = loc;
  fields_.bind(node, ctx, e);
  const std::string title = ctx.reader.fieldText(node, Field::title);
  if (!title.empty()) e.fields["title"] = std::string(text::trimWhitespace(title));  // the one-line form
  ctx.declare(e);
  if (!owner.empty()) {
    // nesting gives the relation (R3): a verification verifies the requirement it is in,
    // a requirement refines the requirement it is in, anything else is about its owner
    const model::Entity* ownerEntity = ctx.find(owner);
    const bool inRequirement = ownerEntity && ownerEntity->kind == model::Kind::Requirement;
    // nesting an artefact in a class, a member or a function says what annotating it says: the entity answers
    // to the requirement or obeys the constraint (Owner, 2026-11-15; DS-217). In a box it gives its level
    const bool inDesign = ownerEntity && ownerEntity->kind != model::Kind::Package && ownerEntity->kind != model::Kind::Module &&
                          ownerEntity->kind != model::Kind::Requirement;
    if (inDesign && e.kind == model::Kind::Requirement) {
      ctx.relate({"satisfies", owner, e.id, loc, ""});
    } else if (inDesign && e.kind == model::Kind::Constraint) {
      ctx.relate({"constrained-by", owner, e.id, loc, ""});
    } else {
      std::string relation = "about";
      if (inRequirement && e.kind == model::Kind::Verification) relation = "verifies";
      if (inRequirement && e.kind == model::Kind::Requirement) relation = "refines";
      ctx.relate({relation, e.id, owner, loc, ""});
    }
  }
  if (e.fields.count("parent")) {
    // one representation per concept: nesting or @refines, never a field
    ctx.report(ctx.metamodel.finding(model::Code::W_PARENT_IGNORED,
                                     e.id + " carries a parent: field; nest it in its parent or write @refines <id-revision>", loc, e.id));
  }
  annotations_.bind(node, ctx, e.id, scope);
  ctx.within(scope, e.id).children(node);  // nested verifications and constraints
}

}  // namespace bind
}  // namespace ds
