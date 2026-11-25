#ifndef DRIFTSPEC_APP_COMMANDS_HPP
#define DRIFTSPEC_APP_COMMANDS_HPP

#include <string>

#include "ds/api/Workspace.hpp"
#include "ds/app/ICommand.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/render/Ir.hpp"
#include "ds/service/UseCase.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace app {

/// ds check <workspace> [--json] (UC-00001-1): the workspace opened for its findings and check-design run on it, the
/// report in the sink of the technology asked (DS-399), indented as ds check --json has always printed it (DS-398).
class CheckCommand : public ICommand {
 public:
  explicit CheckCommand(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  std::string name() const override { return "check"; }
  std::string summary() const override;
  void declare(CLI::App& sub) override;
  service::Outcome run() const override;

 private:
  const model::Metamodel& metamodel_;
  std::string workspace_;
  bool json_ = false;
};

/// ds export <workspace> (UC-00005-1): the workspace opened whole and hand-model run on it, the model as indented
/// JSON, written whole or not at all.
class ExportCommand : public ICommand {
 public:
  explicit ExportCommand(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  std::string name() const override { return "export"; }
  std::string summary() const override;
  void declare(CLI::App& sub) override;
  service::Outcome run() const override;

 private:
  const model::Metamodel& metamodel_;
  std::string workspace_;
};

/// ds render <workspace|ir.json> [diagram] [-f ...] (UC-00002-1): the workspace opened for what the format shows of
/// it (the viewer reads every step of the check, a drawing only what it draws, DS-376) and see-design run on it for
/// every format: the scene and the page, a diagram's IR, layout and cells, and its drawings in svg and text, each in
/// the sink of its technology, with every option the command line was given (DS-474). The door draws nothing of a
/// workspace and holds no model; what it draws itself is a render-IR document someone else produced, which has no
/// workspace and no use case. -o writes to a file.
class RenderCommand : public ICommand {
 public:
  RenderCommand(const model::Metamodel& metamodel, const theme::Theme& theme) : metamodel_(metamodel), theme_(theme) {}
  std::string name() const override { return "render"; }
  std::string summary() const override;
  void declare(CLI::App& sub) override;
  service::Outcome run() const override;

 private:
  /// A render-IR document, drawn as -f names: it has no workspace, so the viewer has none to show.
  service::Outcome document() const;
  /// What see-design is asked: the format, the diagram, the width, whether the terminal shows colour, the workspace as
  /// it was given and that a diagram is drawn only of what read whole (DS-433), and for the page its title, a reader's
  /// stylesheet and whether it is embedded and how tall.
  service::Request request() const;
  /// A render-IR document drawn as -f names: its layout, its cells, or a drawing in svg or text (DS-458).
  std::string draw(const render::IrDocument& ir) const;
  /// The outcome, once the text is written where -o says; unwritten when the file cannot be.
  service::Outcome written(const std::string& text, const std::string& end, const service::Outcome& outcome) const;

  const model::Metamodel& metamodel_;
  const theme::Theme& theme_;  ///< the style a render-IR document is drawn in, made once where the command starts (DS-393)
  std::string input_;
  std::string diagram_;
  std::string format_ = "html";
  std::string output_;
  std::string css_;
  int width_ = 0;
  int height_ = 640;
  bool embed_ = false;
};

/// ds query <workspace> <question> [subject] [--json] (UC-00004-1): the workspace opened for its answers and ask-model
/// run on it, the answers in the sink of the technology asked; asked for JSON, an error is an answer too.
class QueryCommand : public ICommand {
 public:
  explicit QueryCommand(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  std::string name() const override { return "query"; }
  std::string summary() const override;
  void declare(CLI::App& sub) override;
  service::Outcome run() const override;

 private:
  const model::Metamodel& metamodel_;
  std::string workspace_;
  std::string question_;
  std::string subject_;
  bool json_ = false;
};

/// ds manual [topic] [--json] (UC-00006-1): learn-language run over no design, the page as it is printed or as JSON.
class ManualCommand : public ICommand {
 public:
  explicit ManualCommand(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  std::string name() const override { return "manual"; }
  std::string summary() const override;
  void declare(CLI::App& sub) override;
  service::Outcome run() const override;

 private:
  const model::Metamodel& metamodel_;
  std::string topic_;
  bool json_ = false;
};

}  // namespace app
}  // namespace ds

#endif  // DRIFTSPEC_APP_COMMANDS_HPP
