#include "ds/model/Paths.hpp"

#include <set>
#include <string>
#include <vector>

#include "ds/model/Identity.hpp"
#include "ds/model/SequenceTree.hpp"

namespace ds {
namespace model {

/// The frames of one behaviour, read from its tree, and for each what its two sides name. Paths's own: declared
/// in no header, made by pathOf over the tree it walks.
class Branches {
 public:
  /// Then, Else: the path takes that side, drawn inline. Skip: it does not. Keep: it cannot tell, the frame is
  /// drawn. Enter: a loop the path goes into, drawn as the loop it is.
  enum class Way { Then, Else, Skip, Keep, Enter };

  explicit Branches(const SequenceTree& tree) : tree_(tree) {}
  /// The scenarios one side of a frame names, its own and those of the frames inside it.
  std::set<std::string> names(const Entity& f, bool elseSide) const;
  /// Which way the path named `scenario` goes at a frame.
  Way decide(const Entity& f, const std::string& scenario) const;

 private:
  const SequenceTree& tree_;
};

std::set<std::string> Branches::names(const Entity& f, bool elseSide) const {
  std::set<std::string> out;
  const std::string own = tree_.scenario(f.id, elseSide);
  if (!own.empty()) out.insert(own);
  for (const auto& child : tree_.inside(f.id, elseSide)) {
    for (const bool side : {false, true}) {
      for (const auto& n : names(*tree_.model().find(child), side)) out.insert(n);
    }
  }
  return out;
}

Branches::Way Branches::decide(const Entity& f, const std::string& scenario) const {
  const std::set<std::string> thenNames = names(f, false);
  const std::set<std::string> elseNames = names(f, true);
  // a loop is not a choice: every path goes through it, and one that names a branch inside goes into it
  if (Paths::fieldIn(f, "kind") == "loop") return !scenario.empty() && thenNames.count(scenario) ? Way::Enter : Way::Keep;
  // an attempt is not a choice either: its steps run on every path (DS-187)
  if (Paths::fieldIn(f, "kind") == "attempt") return Way::Enter;
  // a handler runs only on a path that fails as it handles: one that names it; the default path does not fail
  if (Paths::fieldIn(f, "kind") == "handler") return !scenario.empty() && thenNames.count(scenario) ? Way::Then : Way::Skip;
  if (!scenario.empty() && thenNames.count(scenario)) return Way::Then;
  if (!scenario.empty() && elseNames.count(scenario)) return Way::Else;
  if (tree_.twoSides(f.id)) {
    if (!thenNames.empty() && elseNames.empty()) return Way::Else;
    if (thenNames.empty() && !elseNames.empty()) return Way::Then;
    return Way::Keep;
  }
  return thenNames.empty() ? Way::Keep : Way::Skip;  // a branch nobody asked for is not taken
}

std::string Paths::fieldIn(const Entity& e, const std::string& key) {
  const auto f = e.fields.find(key);
  return f == e.fields.end() ? "" : f->second;
}

bool Paths::hasScenario(const std::string& sequence, const std::string& scenario) const {
  // a loop, an opt or a side of an alt that names it
  for (const auto& id : model_.index().members(sequence)) {
    const Entity* f = model_.find(id);
    if (f && f->kind == Kind::Frame && fieldIn(*f, "scenario") == scenario) return true;
  }
  return false;
}

std::vector<const Entity*> Paths::pathOf(const std::string& sequence, const std::string& scenario, std::set<std::string>* kept) const {
  const SequenceTree tree(model_, sequence);
  const Branches branches(tree);
  std::vector<const Entity*> out;
  for (const auto& id : tree.messages()) {
    const Entity* m = model_.find(id);
    bool taken = true;
    bool certain = true;  // every frame around it is a side the path takes, none it cannot tell
    std::vector<std::string> around;
    for (std::string frame = tree.frameOf(m->id); !frame.empty() && taken; frame = tree.frameOf(frame)) {
      const Entity& f = *model_.find(frame);
      const Branches::Way way = branches.decide(f, scenario);
      const bool elseSide = tree.onElse(f.id, m->id);
      if (way == Branches::Way::Skip || (way == Branches::Way::Then && elseSide) || (way == Branches::Way::Else && !elseSide)) {
        taken = false;
      } else if (way == Branches::Way::Keep) {
        certain = false;
        around.push_back(f.id);
      } else if (way == Branches::Way::Enter) {
        around.push_back(f.id);
      }
    }
    if (!taken) continue;
    if (kept) kept->insert(around.begin(), around.end());
    out.push_back(m);
    if (certain && fieldIn(*m, "return") == "true") break;  // the path ends here
  }
  return out;
}

std::string Paths::holderOf(const std::string& sequence) const {
  const Entity* subject = model_.find(Identity(sequence).subject().text());
  if (!subject) return "";
  const bool member = subject->kind == Kind::Method || subject->kind == Kind::Function || subject->kind == Kind::Operation;
  return member ? subject->scope : subject->id;
}

}  // namespace model
}  // namespace ds
