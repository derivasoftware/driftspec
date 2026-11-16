#ifndef DRIFTSPEC_QUERY_CUT_HPP
#define DRIFTSPEC_QUERY_CUT_HPP

#include <set>
#include <string>

#include "ds/model/Model.hpp"

namespace ds {
namespace query {

/// The cut of a diagram (DS-229): a set of boxes none of which holds another. Every entity below it
/// has one representative on it, the outermost closed box around it, and what is above it stays open.
/// It is written as what closes and what opens, never as its frontier, so whatever is written is a
/// cut: `close:` names the boxes that close (a name, a prefix with *, or a kind: module, package);
/// `open:` names the boxes that stay open, with the way to each, whatever `close:` says; `open:`
/// written alone closes every other box; `cut:` adds what another diagram writes. The outermost
/// package is the drawing itself and never closes.
class Cut {
 public:
  Cut(const model::Model& model, const model::Entity& diagram);

  /// Whether the diagram writes a cut at all: `open:` or `close:`, its own or the one it takes.
  bool written() const { return written_; }
  /// Whether a box that holds others is closed.
  bool closed(const std::string& box) const { return closed_.count(box) > 0; }
  /// What stands for an id on the cut: the outermost closed box around it, or the id itself.
  std::string standsFor(const std::string& id) const;

 private:
  bool written_ = false;
  std::set<std::string> closed_;
};

}  // namespace query
}  // namespace ds

#endif  // DRIFTSPEC_QUERY_CUT_HPP
