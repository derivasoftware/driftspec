#ifndef DRIFTSPEC_MODEL_METAMODEL_HPP
#define DRIFTSPEC_MODEL_METAMODEL_HPP

#include <map>
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

/// Who sets a field.
enum class FieldSource {
  Written,  ///< the author writes it as key: value
  Syntax,   ///< the binder takes it from the syntax of the declaration
  Derived,  ///< the checker computes it after resolution
};

struct FieldSpec {
  std::string name;  ///< a family ends in * and matches any suffix: local.*, exit *
  FieldType type = FieldType::Text;
  FieldSource source = FieldSource::Written;
  std::string meaning;
  std::vector<std::string> values = {};  ///< when not empty, the only values it takes (a closed field, DS-211)
};

struct KindSpec {
  Kind kind = Kind::Package;
  std::string meaning;
  std::vector<FieldSpec> fields;
  std::vector<std::string> flags;
  std::vector<Kind> parents;  ///< the kinds it may be declared inside; empty when it stands at the top
  bool top = false;           ///< it may stand outside any package
  std::string syntax = {};    ///< how it is written, for the manual
  std::string example = {};   ///< a minimal workspace that declares one and checks clean
};

/// A finding code ds reports: what it means and what to do about it.
struct CodeSpec {
  std::string code;
  std::string severity;  ///< error, warning, or coverage: something not yet written, answered when asked (DS-216)
  std::string meaning;
  std::string fix;
};

/// An annotation that means something, and the relation it makes; any other @word refers to nothing in particular.
/// The binder reads this table: what an annotation means is declared here once, and an annotation binds
/// the same written in its entity or in an extension block anywhere else.
struct AnnotationSpec {
  std::string name;
  std::string relation;
  std::string meaning;
  std::string onVerification = {};  ///< the relation it makes when a verification writes it; empty: the same
  Kind target = Kind::Class;        ///< what the name it carries is expected to be, when nothing declares it
};

struct RelationSpec {
  std::string kind;
  std::string meaning;
  std::vector<Kind> from;  ///< empty: any kind
  std::vector<Kind> to;    ///< empty: any kind
  /// When set, the only (from, to) pairs it may join: contains is derived from where each kind may be declared.
  std::vector<std::pair<Kind, Kind>> pairs;
};

/// The metamodel of DriftSpec, declared once (DS-341): every kind with its fields, flags and parents, every
/// relation with the kinds it joins, the catalogue of finding codes, the annotations that mean something and
/// how each kind is written. One is made where the engine starts and handed by const reference to whoever
/// reads the language: the model, the binder, the checker and the manual. Its tables are built once, when it
/// is made, and every question reads them.
class Metamodel {
 public:
  Metamodel();

  const std::vector<KindSpec>& kinds() const { return kinds_; }
  const std::vector<RelationSpec>& relations() const { return relations_; }
  const KindSpec& specOf(Kind kind) const;
  /// The kind a word names, as nameOf writes it (class, sequence...); nullptr when it names none.
  const KindSpec* specNamed(const std::string& name) const;
  /// The field a key names for a kind, families included; nullptr when the kind does not declare it.
  const FieldSpec* fieldOf(Kind kind, const std::string& key) const;
  const RelationSpec* relationOf(const std::string& kind) const;
  /// Whether a relation of this kind may go from one kind to another.
  bool joins(const RelationSpec& relation, Kind from, Kind to) const;
  const std::vector<CodeSpec>& codes() const { return codes_; }
  const CodeSpec* codeOf(const std::string& code) const;
  /// Every row the manual lists: the annotations that mean something, then any other word and the use case an
  /// operation names after a colon, which only document.
  const std::vector<AnnotationSpec>& annotations() const { return annotations_; }
  /// The annotations that mean something, the binder's: nothing else binds as one.
  const std::vector<AnnotationSpec>& knownAnnotations() const { return known_; }
  /// What an annotation means, by its word without the @; the one of any other word when the language gives it none.
  const AnnotationSpec& annotationOf(const std::string& word) const;
  /// How a kind is written and a minimal workspace that declares one (the manual's, DS-79).
  std::string syntaxOf(Kind kind) const;
  std::string exampleOf(Kind kind) const;
  /// A kind as the language writes it: class, usecase, sequence; read from a table, never built per call.
  const std::string& nameOf(Kind kind) const;
  /// Whether a kind is an artefact identified by its id with its revision: requirement, verification,
  /// constraint and use case (R5, DS-116).
  bool isArtefact(Kind kind) const;
  std::string fieldTypeName(FieldType type) const;
  std::string fieldSourceName(FieldSource source) const;
  /// The version of the language and its tool, as pyproject.toml keeps it, written in when ds is built (DS-278).
  std::string version() const;

 private:
  /// A field the author writes, one the binder takes from the syntax, one the checker derives.
  static FieldSpec writtenField(const std::string& name, FieldType type, const std::string& meaning);
  static FieldSpec syntax(const std::string& name, FieldType type, const std::string& meaning);
  static FieldSpec derived(const std::string& name, FieldType type, const std::string& meaning);
  /// A field written with one of a closed set of values: any other is W-FIELD-VALUE (DS-211).
  static FieldSpec closed(const std::string& name, FieldType type, const std::string& meaning, std::vector<std::string> values);
  /// The fields every artefact carries (REQ-00045-1).
  static std::vector<FieldSpec> artefact();
  static std::vector<FieldSpec> plus(std::vector<FieldSpec> base, const std::vector<FieldSpec>& more);
  /// Every kind with its fields, flags and parents, as declared; how each is written comes after (describe).
  static std::vector<KindSpec> build();
  static RelationSpec rel(const std::string& kind, const std::string& meaning, std::vector<Kind> from, std::vector<Kind> to);
  /// Every relation: containment first, derived from the kinds, then the others as declared.
  std::vector<RelationSpec> buildRelations() const;
  /// Whether a kind is among `kinds`; an empty list holds any kind.
  static bool among(const std::vector<Kind>& kinds, Kind kind);
  /// Whether a field's name matches a key, a family ending in * by its prefix.
  static bool matches(const std::string& name, const std::string& key);
  /// Each kind's name, by its place in Kind, and last the name of none: "?".
  static std::vector<std::string> buildNames();
  /// How each kind is written and a minimal workspace that declares one, by kind (WrittenForms.cpp).
  static std::map<Kind, std::pair<std::string, std::string>> writtenForms();
  /// The catalogue of finding codes (Codes.cpp).
  static std::vector<CodeSpec> buildCodes();
  /// The annotations that mean something.
  static std::vector<AnnotationSpec> buildKnownAnnotations();
  /// What any other word binds as: a link the language gives no meaning to.
  static AnnotationSpec buildOtherAnnotation();
  /// The rows the manual lists: the known annotations, any other word, and the use case after an operation.
  std::vector<AnnotationSpec> buildAnnotations() const;
  /// The kinds, each with how it is written and its example.
  std::vector<KindSpec> describe(std::vector<KindSpec> kinds) const;
  /// contains, from a parent to a child exactly where the child may be declared.
  RelationSpec containment() const;

  std::vector<std::string> names_;                             ///< each kind's name, by its place in Kind
  std::map<Kind, std::pair<std::string, std::string>> forms_;  ///< how each kind is written, and its example
  std::vector<KindSpec> kinds_;
  std::vector<RelationSpec> relations_;
  std::vector<CodeSpec> codes_;
  std::vector<AnnotationSpec> known_;
  AnnotationSpec other_;
  std::vector<AnnotationSpec> annotations_;
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_METAMODEL_HPP
