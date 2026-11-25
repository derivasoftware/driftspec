#ifndef DRIFTSPEC_APP_COMMANDS_HPP
#define DRIFTSPEC_APP_COMMANDS_HPP

#include <string>

#include "ds/api/Workspace.hpp"
#include "ds/app/ICommand.hpp"
#include "ds/format/Format.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"
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
/// the scene (json) and a diagram's layout and cells. What see-design does not offer yet is the door's still: the
/// viewer's page around the scene with its focus, a diagram drawn in svg and text (until DS-400) or written as its IR,
/// the files a diagram reads that did not read whole (DS-433), and a render-IR document someone else produced, which
/// has no workspace. -o writes to a file.
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
  /// The viewer's scene, described by see-design into the JSON sink, indented for a reader (DS-398).
  service::Outcome scene(api::Workspace& workspace) const;
  /// The viewer as one page around the scene, framed on the diagram named (DS-126).
  service::Outcome page(const api::Workspace& workspace) const;
  /// One diagram: its layout and its cells described by see-design, its drawings and its IR written by their writers.
  service::Outcome diagram(api::Workspace& workspace) const;
  /// What a diagram reads that did not read whole (DS-433), said finding by finding: "" when it read whole.
  std::string unread(const model::Model& model) const;
  /// A diagram's IR drawn as -f names; `workspace` when the IR is a workspace's, which is written indented.
  std::string draw(const render::IrText& ir, bool workspace) const;
  format::FormatOptions drawingOptions(bool workspace) const;
  /// The outcome, once the text is written where -o says; unwritten when the file cannot be.
  service::Outcome written(const std::string& text, const std::string& end, const service::Outcome& outcome) const;

  const model::Metamodel& metamodel_;
  const theme::Theme& theme_;  ///< the style every drawing is written in, made once where the command starts (DS-393)
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
