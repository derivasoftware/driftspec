#ifndef DRIFTSPEC_QUERY_FILTER_HPP
#define DRIFTSPEC_QUERY_FILTER_HPP

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "ds/model/Model.hpp"

namespace ds {
namespace query {

/// What to take from the model (DS-127): a filter carries its own criteria, given when it is
/// built, and reads the model it is handed; it gives back identities and never draws. The query
/// applies any filter it is given; a filter of our own is a class with select(), built and passed.
class IFilter {
 public:
  virtual ~IFilter() = default;
  virtual std::set<std::string> select(const model::Model& model) const = 0;
};

/// A class diagram: its include patterns minus its exclude patterns, grown by its neighbours,
/// and the modules and packages around what it holds; when the diagram it is built for writes a
/// cut (`open:`, `close:`, `cut:`), what a closed box holds is not shown, the box stands for it.
class ClassFilter : public IFilter {
 public:
  /// `cutOf` is the identity of the diagram whose cut it applies; empty for none. The cut is read in `metamodel`,
  /// the language of the models it is handed (DS-351).
  ClassFilter(const model::Metamodel& metamodel, std::vector<std::string> include, std::vector<std::string> exclude, int neighbours,
              std::string cutOf = "");
  std::set<std::string> select(const model::Model& model) const override;
  /// What the patterns and the neighbours take, without the boxes around it.
  std::set<std::string> boxes(const model::Model& model) const;
  /// The modules and nested packages that hold what is taken: drawn around it.
  static std::set<std::string> containers(const model::Model& model, const std::set<std::string>& taken);
  /// The box a class diagram draws a relation of `entity` from (DS-219): a box, a module or a package is itself; a
  /// member is its class; a behaviour is its subject's box (a method's class, a function, an operation's interface),
  /// not the module it is written in; anything else is its scope.
  static std::string drawnFrom(const model::Model& model, const model::Entity& entity);

 private:
  static bool isBox(const model::Entity& entity);        // a class, an enum, an interface or a function of a module
  static bool isContainer(const model::Entity& entity);  // a module, or a package below the system
  static bool drawable(const model::Entity& entity);     // a box, or a box that holds others
  static bool drawnAsEdge(const std::string& relation);  // the relations drawn as edges, the ones the neighbours follow
  const model::Metamodel& metamodel_;
  std::vector<std::string> include_;
  std::vector<std::string> exclude_;
  int neighbours_ = 0;
  std::string cutOf_;
};

/// A behaviour a sequence diagram takes, and the scenario it follows ("*" for every branch).
struct Behaviour {
  const model::Entity* sequence = nullptr;
  std::string path;
};

/// A sequence diagram: the behaviours its patterns name (a subject, a scenario, a prefix), and
/// who takes part in them, walked into what they call as far as the diagram says; on a cut, each
/// one is the box that stands for it.
class SequenceFilter : public IFilter {
 public:
  /// `cutOf` is the identity of the diagram whose cut it applies; empty for none. The cut is read in `metamodel`,
  /// the language of the models it is handed (DS-351).
  SequenceFilter(const model::Metamodel& metamodel, std::vector<std::string> include, int depth, std::string cutOf = "");
  std::set<std::string> select(const model::Model& model) const override;
  /// The behaviours taken, in the order the patterns name them.
  std::vector<Behaviour> behaviours(const model::Model& model) const;

 private:
  const model::Metamodel& metamodel_;
  std::vector<std::string> include_;
  int depth_ = 0;
  std::string cutOf_;
};

/// A state diagram: the machines it names and the states of each that the design describes.
class StateFilter : public IFilter {
 public:
  explicit StateFilter(std::vector<std::string> include);
  std::set<std::string> select(const model::Model& model) const override;
  /// The machines taken, in the order the patterns name them.
  std::vector<std::string> machines(const model::Model& model) const;

 private:
  std::vector<std::string> include_;
};

/// A requirements diagram: the artefacts its patterns name and their umbrella: what refines them,
/// and, as shown, what verifies and what realises them; on a cut, what realises is the box that
/// stands for it.
class RequirementsFilter : public IFilter {
 public:
  /// `cutOf` is the identity of the diagram whose cut it applies; empty for none. The cut is read in `metamodel`,
  /// the language of the models it is handed (DS-351).
  RequirementsFilter(const model::Metamodel& metamodel, std::vector<std::string> include, bool design, bool verifications,
                     std::string cutOf = "");
  std::set<std::string> select(const model::Model& model) const override;

 private:
  static bool isArtefact(const model::Entity& entity);  // a requirement, a constraint or a verification
  const model::Metamodel& metamodel_;
  std::vector<std::string> include_;
  bool design_ = true;
  bool verifications_ = true;
  std::string cutOf_;
};

/// The one place that names the filter of each kind of diagram (DS-385), as Steps names the steps of the check
/// (DS-382): what asks what a diagram takes asks it here, and never names a filter itself.
class Filters {
 public:
  /// The filter of `diagram`'s kind, built from its fields and read in `metamodel` (DS-351); null when the entity is
  /// no diagram.
  static std::unique_ptr<IFilter> of(const model::Metamodel& metamodel, const model::Entity& diagram);

 private:
  /// A number field of a diagram: `every` when it is written `*`, 0 when it is not written or is no number an int holds.
  static int number(const model::Entity& diagram, const std::string& key, int every);
};

}  // namespace query
}  // namespace ds

#endif  // DRIFTSPEC_QUERY_FILTER_HPP
