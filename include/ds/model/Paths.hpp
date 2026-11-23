#ifndef DRIFTSPEC_MODEL_PATHS_HPP
#define DRIFTSPEC_MODEL_PATHS_HPP

#include <set>
#include <string>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace model {

/// The walks of a model's behaviours (DS-343): which scenarios a behaviour names, the steps the path of one
/// takes and who a behaviour runs on. Made over a model that outlives it, which it reads and never changes;
/// it holds nothing else, so whoever asks makes one where it asks.
class Paths {
 public:
  explicit Paths(const Model& model) : model_(model) {}

  /// Whether a branch of the behaviour `sequence` is named `scenario` (`opt ... / scenario {`).
  bool hasScenario(const std::string& sequence, const std::string& scenario) const;

  /// The messages of `sequence` that the path named `scenario` takes, in order, up to the return that ends it.
  /// A named branch is taken when it is asked for and skipped otherwise; of an alt with one named side, the
  /// other side is taken; a branch it cannot tell is kept, and its frame is added to `kept`. An empty
  /// `scenario` is the default path: no named branch taken.
  std::vector<const Entity*> pathOf(const std::string& sequence, const std::string& scenario, std::set<std::string>* kept) const;

  /// Who a behaviour runs on: a method's class, a function's module, an operation's interface, or its subject;
  /// "" when its subject is declared nowhere.
  std::string holderOf(const std::string& sequence) const;

 private:
  /// How the frames of one behaviour are told apart, in Paths.cpp: it reads the fields as Paths does.
  friend class Branches;
  /// The value of a field of an entity; "" when it does not carry it.
  static std::string fieldIn(const Entity& e, const std::string& key);
  const Model& model_;
};

}  // namespace model
}  // namespace ds

#endif  // DRIFTSPEC_MODEL_PATHS_HPP
