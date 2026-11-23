#ifndef DRIFTSPEC_MODEL_DETAIL_HPP
#define DRIFTSPEC_MODEL_DETAIL_HPP

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
#include "ds/model/SequenceTree.hpp"

// The helpers the model package shares between its free functions and its classes (DS-332). Internal:
// no header under include/ names them, and one header declares them all, so a name taken twice is a
// compile error and not a silent clash at link time.
namespace ds {
namespace model {
namespace detail {

// ---- Identity.cpp ------------------------------------------------------------------------------

/// The scope outside `text`, of an identity or of a scope: both step out the same way.
std::string outerOf(const std::string& text);

// ---- Model.cpp ---------------------------------------------------------------------------------

/// The value of a field of an entity; "" when it does not carry it.
std::string fieldIn(const Entity& e, const std::string& key);

/// The frames of one behaviour, read from its tree, and for each what its two sides name.
struct Branches {
  const SequenceTree& tree;

  /// Then, Else: the path takes that side, drawn inline. Skip: it does not. Keep: it cannot tell, the frame is
  /// drawn. Enter: a loop the path goes into, drawn as the loop it is.
  enum class Way { Then, Else, Skip, Keep, Enter };

  /// The scenarios one side of a frame names, its own and those of the frames inside it.
  std::set<std::string> names(const Entity& f, bool elseSide) const;
  /// Which way the path named `scenario` goes at a frame.
  Way decide(const Entity& f, const std::string& scenario) const;
};

// ---- Metamodel.cpp -----------------------------------------------------------------------------

/// A field the author writes, one the binder takes from the syntax, one the checker derives.
FieldSpec writtenField(const char* name, FieldType type, const char* meaning);
FieldSpec syntax(const char* name, FieldType type, const char* meaning);
FieldSpec derived(const char* name, FieldType type, const char* meaning);
/// A field written with one of a closed set of values: any other is W-FIELD-VALUE (DS-211).
FieldSpec closed(const char* name, FieldType type, const char* meaning, std::vector<std::string> values);
/// The fields every artefact carries (REQ-00045-1).
std::vector<FieldSpec> artefact();
std::vector<FieldSpec> plus(std::vector<FieldSpec> base, const std::vector<FieldSpec>& more);

extern const FieldSpec kVisibility;
extern const FieldSpec kParams;
extern const FieldSpec kReturns;
extern const FieldSpec kNote;
extern const FieldSpec kFails;
extern const FieldSpec kEscapes;
extern const FieldSpec kEscapePath;
extern const FieldSpec kLanguage;

/// The kinds with how each is written (WrittenForms.cpp).
std::vector<KindSpec> describe(std::vector<KindSpec> kinds);
std::vector<KindSpec> build();
RelationSpec rel(const char* kind, const char* meaning, std::vector<Kind> from, std::vector<Kind> to);
std::vector<RelationSpec> buildRelations();
/// Whether a kind is among `kinds`; an empty list holds any kind.
bool among(const std::vector<Kind>& kinds, Kind kind);
/// Whether a field's name matches a key, a family ending in * by its prefix.
bool matches(const std::string& name, const std::string& key);
/// contains, from a parent to a child exactly where the child may be declared.
RelationSpec containment();

// ---- WrittenForms.cpp, the metamodel's (DS-340) -------------------------------------------------

extern const std::string kShopClass;
extern const std::string kShopInterface;
extern const std::string kShopSequence;
extern const std::string kShopUseCase;

/// How each kind is written and a minimal workspace that declares one, by kind.
const std::map<Kind, std::pair<std::string, std::string>>& writtenForms();

// ---- Codes.cpp, the metamodel's (DS-340) --------------------------------------------------------

std::vector<CodeSpec> buildCodes();

// ---- Manual.cpp --------------------------------------------------------------------------------

std::string indent(const std::string& text, const std::string& by);
std::string pad(const std::string& text, size_t width);
std::string names(const std::vector<Kind>& kinds);
std::string kindPage(const KindSpec& k);
std::string indexPage();
std::string grammarPage();

}  // namespace detail
}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_DETAIL_HPP
