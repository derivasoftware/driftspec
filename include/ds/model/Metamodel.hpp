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

/// A command of ds: how it is invoked, what it does, what each exit code says.
struct CommandSpec {
  std::string name;
  std::string synopsis;
  std::string does;
  std::vector<std::pair<int, std::string>> exits;
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
const std::vector<CodeSpec>& codes();
const CodeSpec* codeOf(const std::string& code);
const std::vector<AnnotationSpec>& annotations();
/// What an annotation means, by its word without the @; the one of any other word when the language gives it none.
const AnnotationSpec& annotationOf(const std::string& word);
const std::vector<CommandSpec>& commands();
const CommandSpec* commandOf(const std::string& name);
/// How a kind is written and a minimal workspace that declares one (the manual's, DS-79).
std::string syntaxOf(Kind kind);
std::string exampleOf(Kind kind);
/// The manual ds embeds: the index ("" topic), a kind, a finding code, commands, annotations or codes; "" for no such topic.
std::string manualPage(const std::string& topic);
std::vector<std::string> manualTopics();
/// The guides of the manual, written as Markdown in doc/manual and embedded when ds is built (DS-272): each by its
/// topic, in the order they are read. The same text is what ds manual <topic> prints and what the site draws.
const std::vector<std::pair<std::string, std::string>>& manualGuides();
/// The version of the tool, as pyproject.toml keeps it, written in when ds is built (DS-278).
std::string version();
/// Whether a relation of this kind may go from one kind to another.
bool joins(const RelationSpec& relation, Kind from, Kind to);

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_METAMODEL_HPP
