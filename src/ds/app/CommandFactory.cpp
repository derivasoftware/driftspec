#include "ds/app/CommandFactory.hpp"

#include "ds/app/Commands.hpp"
#include "ds/emit/NotImplemented.hpp"

namespace ds {
namespace app {

std::unique_ptr<ICommand> CommandFactory::of(const std::string& name) const {
  if (name == "check") return std::make_unique<CheckCommand>(metamodel_);
  if (name == "export") return std::make_unique<ExportCommand>(metamodel_);
  if (name == "render") return std::make_unique<RenderCommand>(metamodel_, theme_);
  if (name == "query") return std::make_unique<QueryCommand>(metamodel_);
  if (name == "manual") return std::make_unique<ManualCommand>(metamodel_);
  throw emit::NotImplemented("", name, names());
}

std::vector<std::string> CommandFactory::names() { return {"check", "export", "render", "query", "manual"}; }

}  // namespace app
}  // namespace ds
