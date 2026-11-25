#ifndef DRIFTSPEC_SERVICE_USECASE_HPP
#define DRIFTSPEC_SERVICE_USECASE_HPP

#include <map>
#include <stdexcept>
#include <string>

#include "ds/check/Selection.hpp"
#include "ds/emit/Sink.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace service {

/// What a use case is asked (DS-407): its arguments by name, the diagram, the question, the topic, the line...
struct Request {
  std::map<std::string, std::string> arguments;

  /// The argument of that name; BadRequest when the request does not carry it.
  const std::string& get(const std::string& name) const;
  /// The argument of that name, or `fallback` when the request does not carry it.
  std::string getOr(const std::string& name, const std::string& fallback) const;
};

/// How a use case ended: one of the outcomes its use case declares in design/usecases.ds (default, errors...).
struct Outcome {
  std::string name;
};

/// A request that is not the use case's: an argument it needs and is not given, or one it cannot read.
class BadRequest : public std::invalid_argument {
 public:
  using std::invalid_argument::invalid_argument;
};

/// One use case of DriftSpec (DS-406, DS-407): what one interface operation realising a UC does, apart from any door.
/// It is given the loaded model, a request and the sink the door lends it (by reference: nothing is handed over); it
/// computes its document as a local value, describes it into the sink and says how it ended. It knows no door: the
/// command line, the engine, the browser and the Python face each translate its outcome and its failures.
class IUseCase {
 public:
  virtual ~IUseCase() = default;
  /// Its name, as UseCaseFactory makes it: check-design, see-design...
  virtual std::string name() const = 0;
  /// The check steps it reads of a model bound from the files; a table's diagram needs its rules, so the bound model
  /// is read too.
  virtual check::Selection needs(const model::Model& bound, const Request& request) const = 0;
  /// Describe the answer to `request` over `model` into `sink`; BadRequest for a request that is not its own,
  /// emit::NotImplemented for what the sink's technology cannot write.
  virtual Outcome run(const model::Model& model, const Request& request, emit::ISink& sink) const = 0;
};

}  // namespace service
}  // namespace ds

#endif  // DRIFTSPEC_SERVICE_USECASE_HPP
