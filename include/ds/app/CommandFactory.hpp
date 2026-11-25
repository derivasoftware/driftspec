#ifndef DRIFTSPEC_APP_COMMANDFACTORY_HPP
#define DRIFTSPEC_APP_COMMANDFACTORY_HPP

#include <memory>
#include <string>
#include <vector>

#include "ds/app/ICommand.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace app {

/// The one place that names the verbs of ds (DS-409), as UseCaseFactory names the use cases: made over what the
/// command line makes once where it starts, and handing each verb it makes what that verb refers to, so a new verb is
/// a class and a line here.
class CommandFactory {
 public:
  /// Over the language and the style of the drawings, which outlive it and every command it makes.
  CommandFactory(const model::Metamodel& metamodel, const theme::Theme& theme) : metamodel_(metamodel), theme_(theme) {}
  /// The verb of that name; emit::NotImplemented when ds has none.
  std::unique_ptr<ICommand> of(const std::string& name) const;
  /// Every verb, in the order ds --help lists them.
  static std::vector<std::string> names();

 private:
  const model::Metamodel& metamodel_;
  const theme::Theme& theme_;
};

}  // namespace app
}  // namespace ds

#endif  // DRIFTSPEC_APP_COMMANDFACTORY_HPP
