#ifndef DRIFTSPEC_BIND_FORMULABINDER_HPP
#define DRIFTSPEC_BIND_FORMULABINDER_HPP

#include <string>
#include <vector>

#include "ds/bind/BindContext.hpp"

namespace ds {
namespace bind {

/// What a rule, a definition or a table says (DS-94, DS-268, DS-356): its clauses read into the model's formulas.
class FormulaBinder {
 public:
  /// The clauses written in `node`, stated for `out_entity`.
  void bind(TSNode node, const BindContext& ctx, model::Entity& out_entity) const;

 private:
  /// A term of a literal: a field of an entity, a variable or a value, as the form writes it.
  static model::Term termOf(const parse::TreeReader& reader, TSNode node);
  /// The text of a clause after its keyword, for the card and the export.
  static std::string clauseText(const parse::TreeReader& reader, TSNode clause);
};

}  // namespace bind
}  // namespace ds

#endif  // DRIFTSPEC_BIND_FORMULABINDER_HPP
