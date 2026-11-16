#ifndef DRIFTSPEC_AXIS_CUT_HPP
#define DRIFTSPEC_AXIS_CUT_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace axis {

/// What stands for an id among a set of boxes: the outermost of them around it, or the id when it is one of
/// them; empty when none of them holds it. The one way anything is lifted onto a cut: the cut of a diagram,
/// and the participants of a scenario written between boxes (DS-233).
std::string standsAmong(const std::string& id, const std::set<std::string>& boxes);

/// The cut of a diagram (DS-229): a set of boxes none of which holds another. Every entity below it
/// has one representative on it, the outermost closed box around it, and what is above it stays open.
/// It is written as what closes and what opens, never as its frontier, so whatever is written is a
/// cut: `close:` names the boxes that close (a name, a prefix with *, or a kind: module, package);
/// `open:` names the boxes that stay open, with the way to each, whatever `close:` says; `open:`
/// written alone closes every other box; `cut:` adds what another diagram writes. The outermost
/// package is the drawing itself and never closes.
///
/// A sequence has a second cut, over the calls (DS-231): where it stops following them. The same
/// shape: `fold:` names the calls it does not follow (`*`, a box, an operation) and every other is
/// followed; `unfold:` names those it follows, whatever `fold:` says; `unfold:` written alone
/// folds every other call; `depth:` is the uniform case for what neither names; and with none of
/// them a sequence that stands on a cut follows every call, since what a closed box does inside
/// is seen only through what crosses out of it.
class Cut {
 public:
  Cut(const model::Model& model, const model::Entity& diagram);

  /// Whether the diagram writes a cut at all: `open:` or `close:`, its own or the one it takes.
  bool written() const { return written_; }
  /// Whether a box that holds others is closed.
  bool closed(const std::string& box) const { return closed_.count(box) > 0; }
  /// What stands for an id on the cut: the outermost closed box around it, or the id itself.
  std::string standsFor(const std::string& id) const;
  /// Whether a sequence follows a call into the behaviour of `operation`, `left` levels of its depth being left.
  bool follows(const std::string& operation, int left) const;

 private:
  bool written_ = false;
  std::set<std::string> closed_;
  std::vector<std::string> unfold_;
  std::vector<std::string> fold_;
  bool unfolds_ = false;  // unfold: is written
  bool folds_ = false;    // fold: is written
  bool deep_ = false;     // depth: is written
};

}  // namespace axis
}  // namespace ds

#endif  // DRIFTSPEC_AXIS_CUT_HPP
