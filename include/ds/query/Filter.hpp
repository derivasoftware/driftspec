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
/// and the modules and packages around what it holds; when it closes boxes (`open:` written),
/// what a closed box holds is not shown, the box stands for it.
class ClassFilter : public IFilter {
 public:
  ClassFilter(std::vector<std::string> include, std::vector<std::string> exclude, int neighbours, bool closes = false,
              std::vector<std::string> open = {});
  std::set<std::string> select(const model::Model& model) const override;
  /// What the patterns and the neighbours take, without the boxes around it.
  std::set<std::string> boxes(const model::Model& model) const;
  /// The modules and nested packages that hold what is taken: drawn around it.
  static std::set<std::string> containers(const model::Model& model, const std::set<std::string>& taken);
  /// The boxes that stay open: those `open` names, and every box on the way to one.
  static std::set<std::string> opened(const std::set<std::string>& containers, const std::vector<std::string>& open);
  /// What stands for an id: the outermost closed box around it, or the id itself.
  static std::string standsFor(const std::string& id, const std::set<std::string>& containers, const std::set<std::string>& opened);

 private:
  bool matches(const model::Model& model, const std::string& pattern, const model::Entity& entity) const;
  std::vector<std::string> include_;
  std::vector<std::string> exclude_;
  int neighbours_ = 0;
  bool closes_ = false;
  std::vector<std::string> open_;
};

/// A behaviour a sequence diagram takes, and the scenario it follows ("*" for every branch).
struct Behaviour {
  const model::Entity* sequence = nullptr;
  std::string path;
};

/// A sequence diagram: the behaviours its patterns name (a subject, a scenario, a prefix), and
/// who takes part in them, walked into what they call down to a depth.
class SequenceFilter : public IFilter {
 public:
  SequenceFilter(std::vector<std::string> include, int depth, bool modules);
  std::set<std::string> select(const model::Model& model) const override;
  /// The behaviours taken, in the order the patterns name them.
  std::vector<Behaviour> behaviours(const model::Model& model) const;

 private:
  std::vector<std::string> include_;
  int depth_ = 0;
  bool modules_ = false;
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
/// and, as shown, what verifies and what realises them.
class RequirementsFilter : public IFilter {
 public:
  RequirementsFilter(std::vector<std::string> include, bool design, bool verifications);
  std::set<std::string> select(const model::Model& model) const override;

 private:
  std::vector<std::string> include_;
  bool design_ = true;
  bool verifications_ = true;
};

/// The one place that names the filter of each kind of diagram, built from the diagram's fields;
/// null when the entity is no diagram.
std::unique_ptr<IFilter> filterFor(const model::Entity& diagram);

}  // namespace query
}  // namespace ds

#endif  // DRIFTSPEC_QUERY_FILTER_HPP
