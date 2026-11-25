#include <string>
#include <vector>

#include "ds/check/Steps.hpp"
#include "ds/machine/Machine.hpp"

namespace ds {
namespace check {

std::string CallOrder::name() const { return "order"; }

std::vector<std::string> CallOrder::needs() const { return {"behaviour"}; }

std::string CallOrder::either(const std::vector<std::string>& names) {
  std::string out;
  for (const auto& name : names) out += (out.empty() ? "" : " or ") + name;
  return out;
}

CallOrder::CallOrder(const Context& context) : context_(&context) {}

std::vector<model::Finding> CallOrder::check() {
  std::vector<model::Finding> found;
  const model::Model& model = context_->model();
  for (const auto& pair : model.entities()) {
    const model::Entity& e = pair.second;
    if (e.kind != model::Kind::State || e.field("transitions").empty()) continue;
    const machine::Machine written(model, e);
    for (const machine::Call& call : written.calls()) {
      if (call.fits) continue;
      const model::Entity* message = model.find(call.message);
      const bool own = call.receiver == "self";  // a behaviour of the subject calling an event of its own
      found.push_back(context_->metamodel().finding(model::Code::W_STATE_ORDER,
                                                    call.behaviour + " calls " + (own ? "its own " : call.receiver + ".") + call.event +
                                                        "() when " + (own ? "it" : call.receiver) + " can only be " + either(call.from) +
                                                        ": the machine of " + e.name + " lets " + call.event + " happen from " +
                                                        either(written.sources(call.event)),
                                                    message ? message->location : e.location, call.behaviour));
    }
  }
  return found;
}

}  // namespace check
}  // namespace ds
