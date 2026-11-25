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

/// UC-00002-1, see a design: the viewer's scene, or one diagram drawn (its layout or its cells), as interface
/// see-design's viewer() and diagram() promise. Asked format json, layout or cells; the diagram and the width.
class SeeDesign : public IUseCase {
 public:
  explicit SeeDesign(const model::Metamodel& metamodel) : metamodel_(metamodel) {}
  scene::SceneDocument viewer(const model::Model& model) const;
  /// The diagram's render-IR; empty when the workspace has no diagram of that name.
  std::optional<render::IrDocument> diagram(const model::Model& model, const std::string& name) const;
  std::string name() const override { return "see-design"; }
  check::Selection needs(const model::Model& bound, const Request& request) const override;
  Outcome run(const model::Model& model, const Request& request, emit::ISink& sink) const override;

 private:
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
