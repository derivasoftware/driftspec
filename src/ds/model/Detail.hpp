#ifndef DRIFTSPEC_MODEL_DETAIL_HPP
#define DRIFTSPEC_MODEL_DETAIL_HPP

#include <cstddef>
#include <set>
#include <string>
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

// ---- Manual.cpp --------------------------------------------------------------------------------

std::string indent(const std::string& text, const std::string& by);
std::string pad(const std::string& text, size_t width);
// the manual's pages, written from the metamodel they are given (DS-342 makes them the Manual's)
std::string names(const Metamodel& metamodel, const std::vector<Kind>& kinds);
std::string kindPage(const Metamodel& metamodel, const KindSpec& k);
std::string indexPage(const Metamodel& metamodel);
std::string grammarPage(const Metamodel& metamodel);

}  // namespace detail
}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_DETAIL_HPP
