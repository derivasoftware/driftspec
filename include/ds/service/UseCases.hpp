#ifndef DRIFTSPEC_SERVICE_USECASES_HPP
#define DRIFTSPEC_SERVICE_USECASES_HPP

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/query/Query.hpp"
#include "ds/render/Ir.hpp"
#include "ds/render/IrExport.hpp"
#include "ds/scene/Scene.hpp"
#include "ds/service/UseCase.hpp"
#include "ds/theme/Theme.hpp"

namespace ds {
namespace service {

/// UC-00001-1, check a design: its report, as interface check-design's report() promises.
class CheckDesign : public IUseCase {
 public:
  explicit CheckDesign(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  check::Report report(const model::Model& model) const;
  std::string name() const override { return "check-design"; }
  check::Selection needs(const model::Model& bound, const Request& request) const override;
  Outcome run(const model::Model& model, const Request& request, emit::ISink& sink) const override;

 private:
  const model::Metamodel& metamodel_;
};

/// UC-00002-1, see a design: the viewer's scene or its page, or one diagram drawn, as interface see-design's viewer()
/// and diagram() promise (DS-474). Asked format json (the scene) or html (the page, framed on what the diagram asked
/// shows, or on the identities of focus), or a diagram's layout, cells, ir, svg or text. Every drawing is drawn here,
/// so no door draws: the structure is described into the sink lent, a drawing and the page handed to it as a verbatim
/// text, written in its technology.
class SeeDesign : public IUseCase {
 public:
  explicit SeeDesign(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  scene::SceneDocument viewer(const model::Model& model) const;
  /// The diagram's render-IR; empty when the workspace has no diagram of that name.
  std::optional<render::IrDocument> diagram(const model::Model& model, const std::string& name) const;
  /// What the diagram reads that did not read whole (DS-433), said finding by finding and then why it is not drawn,
  /// naming `workspace` as it was given; "" when it read whole.
  std::string unread(const model::Model& model, const std::string& name, const std::string& workspace) const;
  std::string name() const override { return "see-design"; }
  check::Selection needs(const model::Model& bound, const Request& request) const override;
  Outcome run(const model::Model& model, const Request& request, emit::ISink& sink) const override;

 private:
  /// The viewer as one page around the scene (DS-126), handed to the sink as a verbatim text: framed on what the
  /// diagram asked shows (no-diagram when there is none) or on the identities of focus, titled, with a reader's
  /// stylesheet, embedded and how tall as the request says.
  Outcome page(const model::Model& model, const Request& request, emit::ISink& sink) const;
  /// One diagram in the format asked: its IR, layout and cells described into the sink, its drawing in svg or text
  /// handed to it as a verbatim text. Asked whole, one that reads a file that did not read whole is not drawn (DS-433).
  Outcome drawn(const model::Model& model, const std::string& format, const Request& request, emit::ISink& sink) const;
  /// A diagram drawn in svg, through render's Drawings (DS-470), or in text, wrapped to the width asked and in colour
  /// when asked; none when the workspace has no diagram of that name.
  std::optional<std::string> picture(const model::Model& model, const std::string& format, const std::string& name,
                                     const Request& request) const;
  /// The formats it is asked in, for a message.
  static std::vector<std::string> formats();

  const model::Metamodel& metamodel_;
  const theme::Theme theme_;  ///< the style its drawings are measured and drawn in, made once with it (DS-393)
};

/// UC-00004-1, ask the model: the answers to a question about a subject, as interface ask-model's ask() promises.
class AskModel : public IUseCase {
 public:
  explicit AskModel(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  std::vector<query::Answer> ask(const model::Model& model, const std::string& question, const std::string& subject) const;
  std::string name() const override { return "ask-model"; }
  check::Selection needs(const model::Model& bound, const Request& request) const override;
  Outcome run(const model::Model& model, const Request& request, emit::ISink& sink) const override;

 private:
  const model::Metamodel& metamodel_;
};

/// UC-00005-1, hand the model to another tool: the whole model, as interface hand-model's model() promises.
class HandModel : public IUseCase {
 public:
  explicit HandModel(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  std::string name() const override { return "hand-model"; }
  check::Selection needs(const model::Model& bound, const Request& request) const override;
  Outcome run(const model::Model& model, const Request& request, emit::ISink& sink) const override;

 private:
  const model::Metamodel& metamodel_;
};

/// UC-00006-1, learn the language: a page of the manual, as interface learn-language's page() promises. It reads no
/// design: the model it is given is not read.
class LearnLanguage : public IUseCase {
 public:
  explicit LearnLanguage(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  /// The page of `topic` ("" for the index); empty when the manual has none.
  std::string page(const std::string& topic) const;
  std::string name() const override { return "learn-language"; }
  check::Selection needs(const model::Model& bound, const Request& request) const override;
  Outcome run(const model::Model& model, const Request& request, emit::ISink& sink) const override;

 private:
  const model::Metamodel& metamodel_;
};

/// UC-00007-1, edit a design: what the editor asks the engine, the words that may come at a place of a text, as
/// interface edit-design's expectedAt() promises. It reads no model.
class EditDesign : public IUseCase {
 public:
  std::vector<std::string> expectedAt(const std::string& text, int line, int column) const;
  std::string name() const override { return "edit-design"; }
  check::Selection needs(const model::Model& bound, const Request& request) const override;
  Outcome run(const model::Model& model, const Request& request, emit::ISink& sink) const override;
};

/// UC-00008-1, play a design in the browser (DS-460): what the playground receives for each edit, one document. Asked
/// for the design (part "design", the default), the files' artefacts as rows, their diagrams in the order written, each
/// drawn as render draws it in svg with what it takes, the identities declared and where each is written, and the report
/// as ds check --json writes it; asked for the manual (part "manual"), a page of it or, for "", its guides and topics,
/// in the playground's own record. Every record's fields in byte order.
class PlayDesign : public IUseCase {
 public:
  explicit PlayDesign(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  std::string name() const override { return "play-design"; }
  check::Selection needs(const model::Model& bound, const Request& request) const override;
  Outcome run(const model::Model& model, const Request& request, emit::ISink& sink) const override;

 private:
  /// A page of the manual, or with "" its guides and topics, as the playground reads it (DS-272).
  void manual(const std::string& topic, emit::ISink& sink) const;
  /// The artefacts as rows: what each one refines, what keeps it, what proves it; each kind named by the metamodel.
  void artefacts(const model::Model& model, emit::ISink& sink) const;
  /// The diagrams in the order they are written, file then line, each drawn as svg with what it takes.
  void diagrams(const model::Model& model, int width, emit::ISink& sink) const;

  const model::Metamodel& metamodel_;
  const theme::Theme theme_;  ///< the style its drawings are drawn in, made once with it (DS-393)
};

/// The one place that names the use cases (DS-407): made over the metamodel the use cases are written in and handed,
/// by reference, to what asks for them by name (a door, the workspace, DS-408): its dependencies are aggregated where
/// it is made, as the Owner's pattern has it.
class UseCaseFactory {
 public:
  /// Over `metamodel`, which outlives it and every use case it makes.
  explicit UseCaseFactory(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  /// The use case of that name; emit::NotImplemented when there is none.
  std::unique_ptr<IUseCase> of(const std::string& name) const;
  /// The metamodel its use cases are written in.
  const model::Metamodel& metamodel() const { return metamodel_; }
  /// The names of every use case, in the order of their UCs.
  static std::vector<std::string> names();

 private:
  const model::Metamodel& metamodel_;
};

}  // namespace service
}  // namespace ds

#endif  // DRIFTSPEC_SERVICE_USECASES_HPP
