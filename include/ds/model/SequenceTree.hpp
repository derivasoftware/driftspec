#ifndef DRIFTSPEC_MODEL_SEQUENCETREE_HPP
#define DRIFTSPEC_MODEL_SEQUENCETREE_HPP

#include <map>
#include <string>
#include <vector>

namespace ds {
namespace model {

class Model;
struct Entity;

/// One reading of a behaviour's steps (DS-172). The model keeps them as the tree they are written in:
/// each message and frame contained by what encloses it, the sequence or a frame, in the order
/// written, and an alt holding its sides as frames of kind branch. Read once here, so that whoever
/// asks the order of a message, the frame around a step or where an alt's else begins asks the same
/// tree, instead of each rebuilding it.
class SequenceTree {
 public:
  SequenceTree(const Model& model, const std::string& sequence);
  /// The model it reads.
  const Model& model() const { return model_; }
  /// Its messages, by identity, in the order written.
  const std::vector<std::string>& messages() const { return messages_; }
  /// Its loops, alts and opts (not the branches), by identity, in the order they open.
  const std::vector<std::string>& frames() const { return frames_; }
  /// Where a message stands in the order written; -1 when it is not one of this behaviour's.
  int orderOf(const std::string& message) const;
  /// The innermost loop, alt or opt around a step (a branch is a side of its alt); "" at the top.
  std::string frameOf(const std::string& id) const;
  /// How many messages come before a frame opens: where it is drawn when it holds none.
  int at(const std::string& frame) const;
  /// The first message a frame holds, at any depth; "" when it holds none.
  std::string first(const std::string& frame) const;
  /// The first message of an alt's else side, at any depth; "" when it has none.
  std::string elseOf(const std::string& frame) const;
  /// Whether an alt's else side says anything: a message, or a scenario name.
  bool twoSides(const std::string& frame) const;
  /// Whether a step sits on the else side of an alt around it.
  bool onElse(const std::string& frame, const std::string& id) const;
  /// The scenario one side of a frame names: a loop's or an opt's own, an alt's first side or its else.
  std::string scenario(const std::string& frame, bool elseSide) const;
  /// The frames directly inside one side of a frame, by identity, in the order written.
  std::vector<std::string> inside(const std::string& frame, bool elseSide) const;

 private:
  /// A branch: a side of the alt around it, never a frame of its own.
  static bool isBranch(const Entity& e);
  void walk(const std::string& holder, const std::string& frame);
  const Model& model_;
  std::vector<std::string> messages_;
  std::vector<std::string> frames_;
  std::map<std::string, int> order_;
  std::map<std::string, std::string> frameOf_;  ///< step -> innermost loop, alt or opt
  std::map<std::string, int> at_;
  std::map<std::string, std::string> first_;
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_SEQUENCETREE_HPP
