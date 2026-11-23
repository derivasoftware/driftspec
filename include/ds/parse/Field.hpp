#ifndef DRIFTSPEC_PARSE_FIELD_HPP
#define DRIFTSPEC_PARSE_FIELD_HPP

#include <cstdint>

namespace ds {
namespace parse {

/// A field of the DriftSpec grammar (DS-360): what a child is to the node that holds it, as grammar.js names it. The
/// tree reader turns each into the grammar's field id once, so reading a field compares no name. Every field of the
/// grammar is here, in its order; a C++ keyword takes a trailing underscore (operator_, default_).
enum class Field : std::uint8_t {
  action,
  base,
  body,
  bound,
  call,
  code,
  condition,
  default_,
  definition,
  else_scenario,
  entity,
  event,
  fails,
  failure,
  family,
  field,
  from,
  guard,
  id,
  key,
  kind,
  left,
  many,
  modifier,
  name,
  negated,
  of,
  op,
  operator_,
  operators,
  optional,
  otherwise,
  ownership,
  package,
  parameter,
  params,
  prefix,
  realises,
  relation,
  result,
  returns,
  right,
  scenario,
  scope,
  show,
  subject,
  symbol,
  target,
  text,
  title,
  to,
  type,
  value,
  variable,
  visibility,
};

}  // namespace parse
}  // namespace ds

#endif  // DRIFTSPEC_PARSE_FIELD_HPP
