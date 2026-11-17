#include <string>
#include <vector>

#include "ds/check/Steps.hpp"
#include "ds/machine/Machine.hpp"

namespace ds {
namespace check {

namespace {

std::string either(const std::vector<std::string>& names) {
  std::string out;
  for (const auto& name : names) out += (out.empty() ? "" : " or ") + name;
  return out;
}

}  // namespace

std::string CallOrder::name() const { return "order"; }

std::vector<std::string> CallOrder::needs() const { return {"behaviour"}; }

std::vector<model::Finding> CallOrder::check(const Context& context) {
  std::vector<model::Finding> found;
  const model::Model& model = context.model();
  for (const auto& pair : model.entities()) {
    const model::Entity& e = pair.second;
    if (e.kind != model::Kind::State || e.field("transitions").empty()) continue;
    const machine::Machine written(model, e);
    for (const machine::Call& call : written.calls()) {
      if (call.fits) continue;
      const model::Entity* message = model.find(call.message);
      const bool own = call.receiver == "self";  // a behaviour of the subject calling an event of its own
      found.push_back({"W-STATE-ORDER", "warning",
                       call.behaviour + " calls " + (own ? "its own " : call.receiver + ".") + call.event + "() when " +
                           (own ? "it" : call.receiver) + " can only be " + either(call.from) + ": the machine of " + e.name + " lets " +
                           call.event + " happen from " + either(written.sources(call.event)),
                       message ? message->location : e.location});
    }
  }
  return found;
}

}  // namespace check
}  // namespace ds
