#ifndef DRIFTSPEC_RENDER_SEQUENCEBUILDER_HPP
#define DRIFTSPEC_RENDER_SEQUENCEBUILDER_HPP

#include <map>
#include <set>
#include <string>
#include <vector>

#include "ds/axis/Cut.hpp"
#include "ds/model/Model.hpp"
#include "ds/render/Ir.hpp"

namespace ds {
namespace render {

/// What a sequence diagram is built of, behaviour by behaviour (DS-369): its lifelines and messages, as the sequence
/// projector follows the calls.
class SequenceBuilder {
 public:
  /// Builds over `model`, its lifelines standing on `cut` and following the calls it says (DS-231).
  SequenceBuilder(const model::Model& model, const axis::Cut& cut) : model_(model), cut_(cut) {}

  std::vector<IrNode> nodes;  ///< the lifelines, in the order they are met
  std::vector<IrEdge> edges;  ///< the messages, each with its order
  int order = 0;              ///< the order the next message takes

  /// The closed box that stands for a type on the cut; empty when the type draws a lifeline of its own.
  std::string closedAround(const std::string& typeId) const;

  /// The lifeline of a closed box: one for everything inside it.
  std::string boxLine(const std::string& box);

  /// The lifeline of a participant (DS-232): the closed box that stands for it, the box itself when a box is
  /// named as a participant, or one lifeline for each object of the context.
  std::string lineOf(const std::string& typeId, const std::string& name, const std::string& behaviour);

  /// Whether the call that enters `behaviour` is followed into it.
  bool followed(const std::string& behaviour, int depth) const;

  std::string lifeline(const std::string& key, const std::string& label, const std::string& classifier, const std::string& typeId = "");

  std::string shortName(const std::string& id);

  /// Emits the messages of one behaviour; `selfLine` is the lifeline its subject runs on.
  /// `backLine` is whoever called it, where an early return goes. `path` is the scenario it follows: its name,
  /// "" for the default path that takes no named branch, or query::kEveryBranch for every branch, as written.
  void emit(const model::Entity& seq, const std::string& selfLine, const std::string& backLine, const std::string& path, int depth,
            std::set<std::string>& stack);

 private:
  const model::Model& model_;
  const axis::Cut& cut_;                           // the cut the lifelines stand on, and the calls it follows (DS-231)
  std::map<std::string, std::string> lifelineOf_;  // key → lifeline id
  std::set<std::string> boxLines_;                 // the lifelines that are closed boxes
};

}  // namespace render
}  // namespace ds

#endif  // DRIFTSPEC_RENDER_SEQUENCEBUILDER_HPP
