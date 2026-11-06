#ifndef DRIFTSPEC_MODEL_METAMODEL_HPP
#define DRIFTSPEC_MODEL_METAMODEL_HPP

#include <string>
#include <utility>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace model {

/// What a field holds. The value is still text in the model; the type says how to read it.
enum class FieldType {
  Text,      ///< prose, kept as written
  Name,      ///< one name or identity
  Names,     ///< names separated by commas
  Type,      ///< a type as written: std::vector<Entity>
  Params,    ///< name: type pairs separated by commas
  Number,    ///< an integer, or * where the field says so
  Status,    ///< draft, approved, obsolete
  Patterns,  ///< identities with * wildcards, separated by commas
};

const char* fieldTypeName(FieldType type);

/// Who sets a field.
enum class FieldSource {
  Written,  ///< the author writes it as key: value
  Syntax,   ///< the binder takes it from the syntax of the declaration
  Derived,  ///< the checker computes it after resolution
};

const char* fieldSourceName(FieldSource source);

struct FieldSpec {
  std::string name;  ///< a family ends in * and matches any suffix: local.*, exit *
  FieldType type = FieldType::Text;
  FieldSource source = FieldSource::Written;
  std::string meaning;
};

struct KindSpec {
  Kind kind = Kind::Package;
  std::string meaning;
  std::vector<FieldSpec> fields;
  std::vector<std::string> flags;
  std::vector<Kind> parents;  ///< the kinds it may be declared inside; empty when it stands at the top
  bool top = false;           ///< it may stand outside any package
};

struct RelationSpec {
  std::string kind;
  std::string meaning;
  std::vector<Kind> from;  ///< empty: any kind
  std::vector<Kind> to;    ///< empty: any kind
  /// When set, the only (from, to) pairs it may join: contains is derived from where each kind may be declared.
  std::vector<std::pair<Kind, Kind>> pairs;
};

/// The metamodel of DriftSpec, declared once: every kind with its fields, flags and
/// parents, and every relation with the kinds it joins. The binder, the checker,
/// the export and the render read the language from here.
const std::vector<KindSpec>& kinds();
const std::vector<RelationSpec>& relations();

const KindSpec& specOf(Kind kind);
/// The field a key names for a kind, families included; nullptr when the kind does not declare it.
const FieldSpec* fieldOf(Kind kind, const std::string& key);
const RelationSpec* relationOf(const std::string& kind);
/// Whether a relation of this kind may go from one kind to another.
bool joins(const RelationSpec& relation, Kind from, Kind to);

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_METAMODEL_HPP
