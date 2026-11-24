#ifndef DRIFTSPEC_AXIS_CUT_HPP
#define DRIFTSPEC_AXIS_CUT_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace axis {

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
  /// The cut `diagram` writes over `model`; `metamodel` names the kinds a pattern may close (DS-351).
  Cut(const model::Model& model, const model::Metamodel& metamodel, const model::Entity& diagram);

  /// Whether the diagram writes a cut at all: `open:` or `close:`, its own or the one it takes.
  bool written() const { return written_; }
  /// Whether a box that holds others is closed.
  bool closed(const std::string& box) const { return closed_.count(box) > 0; }
  /// What stands for an id on the cut: the outermost closed box around it, or the id itself.
  std::string standsFor(const std::string& id) const;
  /// Whether a sequence follows a call into the behaviour of `operation`, `left` levels of its depth being left.
  bool follows(const std::string& operation, int left) const;
  /// What stands for an id among a set of boxes: the outermost of them around it, or the id when it is one of
  /// them; empty when none of them holds it. The one way anything is lifted onto a cut: the cut of a diagram,
  /// and the participants of a scenario written between boxes (DS-233).
  static std::string standsAmong(const std::string& id, const std::set<std::string>& boxes);

  /// How a relation reads between two boxes, once what they hold is not looked at (DS-230): "structural" (one
  /// owns, refers to, derives from or realises something of the other), "uses" (it names a type of the other,
  /// calls it, sends it a message or declares its use), or empty when it is no relation between boxes. Two
  /// bonds and no more: lifted, the kinds of a relation between classes do not weigh the same, and more than
  /// two would be noise between boxes.
  static std::string bondOf(const std::string& relation);

 private:
  /// The box outside an id.
  static std::string outerOf(const std::string& id);
  /// The boxes from the outermost one down to the id.
  static std::vector<std::string> pathTo(const std::string& id);
  /// A box that holds others and can close: a module, or a package below the outermost one.
  static bool closes(const model::Entity& e);
  /// Whether a pattern names a box: its identity, a prefix of it with *, or its kind.
  static bool names(const model::Metamodel& metamodel, const std::string& pattern, const model::Entity& box);
  /// Whether one of the patterns names the box.
  static bool namedBy(const model::Metamodel& metamodel, const std::vector<std::string>& patterns, const model::Entity& box);
  /// Whether a pattern reaches a call: its operation, a prefix of it with * (`*` alone is every call), or a box
  /// or a class the operation lives in.
  static bool reaches(const std::vector<std::string>& patterns, const std::string& operation);

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
