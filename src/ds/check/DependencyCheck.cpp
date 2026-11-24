#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <set>
#include <sstream>
#include <vector>

#include "Shared.hpp"
#include "ds/check/Evaluator.hpp"
#include "ds/check/Steps.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

std::string DependencyCheck::name() const { return "dependencies"; }

std::vector<std::string> DependencyCheck::needs() const {
  // the uses the linker made and the calls behaviour typing made
  return {"linker", "behaviour"};
}

DependencyCheck::DependencyCheck(const Context& context) : context_(&context), model_(&context.model()), resolver_(&context.resolver()) {}

std::vector<model::Finding> DependencyCheck::check() {
  found_.clear();
  checkDependencies();
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

// A box says what it depends on (@consumes <interface>, REQ-00088-1) and its behaviours say what it
// calls; the two are compared (REQ-00094-2). A call to an interface offered by another box needs a
// @consumes on the caller or on something it lives in; a @consumes that none of the box's behaviours calls is
// dead, said only once the box has behaviours; a call to a class that realises an interface, from
// outside its module, goes around the contract. An interface's own sequences realise it, and a box
// calling an interface it offers itself depends on nothing.
void DependencyCheck::checkDependencies() {
  std::map<std::string, std::set<std::string>> declared;  // a box and the interfaces it uses
  std::map<std::pair<std::string, std::string>, model::Location> written;
  std::map<std::string, std::string> offeredBy;           // an interface and the box that offers it
  std::map<std::string, std::set<std::string>> realised;  // a class and the interfaces it realises
  for (const auto& r : model_->index().ofKinds({"consumes", "exposes", "realises"})) {
    if (r.kind == "consumes") {
      declared[r.from].insert(r.to);
      written[{r.from, r.to}] = r.location;
    } else if (r.kind == "exposes") {
      offeredBy[r.to] = r.from;
    } else if (r.kind == "realises") {
      const model::Entity* from = model_->find(r.from);
      const model::Entity* to = model_->find(r.to);
      if (from && to && from->kind == model::Kind::Class && to->kind == model::Kind::Interface) realised[r.from].insert(r.to);
    }
  }
  const auto chain = [](const std::string& id) {  // the identity and every box it lives in: p@m::C::run, p@m::C, p@m, p
    std::vector<std::string> out;
    for (std::string s = id; !s.empty(); s = model::Scope(s).outer().text()) {
      const std::string box = model::Scope(s).box().text();
      if (out.empty() || out.back() != box) out.push_back(box);
    }
    return out;
  };
  const auto inside = [&chain](const std::string& id, const std::string& box) {
    const auto boxes = chain(id);
    return std::find(boxes.begin(), boxes.end(), box) != boxes.end();
  };
  std::map<std::string, std::set<std::string>> called;  // a box that declares, and the interfaces its behaviours call
  for (const auto& r : model_->index().ofKind("calls")) {
    std::string subject = model::Identity(r.from).subject().text();
    const model::Entity* op = model_->find(r.to);
    if (subject.empty() || !op) continue;
    const model::Entity* msg = model_->find(r.from);
    // who calls is the sender written (DS-232); in a scenario of a use case, with none, it is whoever asks
    // from outside, who declares nothing
    if (msg && !msg->field("from-type").empty()) {
      subject = msg->field("from-type");
    } else if (msg) {
      bool outside = false;
      for (const auto& b : model_->index().from(msg->scope)) {
        const model::Entity* of = b.kind == "behaviour-of" ? model_->find(b.to) : nullptr;
        outside = outside || (of && of->kind == model::Kind::UseCase);
      }
      if (outside) continue;
    }
    const model::Location location = msg ? msg->location : r.location;
    if (op->kind == model::Kind::Operation) {
      const std::string& iface = op->scope;
      bool declares = false;
      for (const auto& box : chain(subject)) {
        const auto d = declared.find(box);
        if (d == declared.end() || !d->second.count(iface)) continue;
        declares = true;
        called[box].insert(iface);
      }
      const auto offered = offeredBy.find(iface);
      if (declares || inside(subject, iface) || (offered != offeredBy.end() && inside(subject, offered->second))) continue;
      found_.push_back(context_->metamodel().finding(model::Code::W_CONSUMES_UNDECLARED,
                                                     subject + " calls " + op->id + ", and nothing it lives in declares @consumes " + iface,
                                                     location));
    } else if (op->kind == model::Kind::Method) {
      const auto r2 = realised.find(op->scope);
      const model::Entity* cls = model_->find(op->scope);
      if (r2 == realised.end() || !cls || inside(subject, cls->scope)) continue;  // inside the realiser's module: its own business
      bool realising = false;
      for (const auto& iface : r2->second) realising = realising || inside(subject, iface);  // the interface's own sequence
      if (realising) continue;
      // only a method that answers an operation of the interface goes around it; one the interface does
      // not offer cannot be called through it (DS-106)
      std::string iface;
      for (const auto& i : r2->second) {
        const model::Entity* operation = model_->find(i + "::" + op->name);
        if (iface.empty() && operation && operation->kind == model::Kind::Operation) iface = i;
      }
      if (iface.empty()) continue;
      found_.push_back(context_->metamodel().finding(model::Code::W_CONSUMES_BYPASS,
                                                     subject + " calls " + op->id + ", and " + cls->id + " realises " + iface +
                                                         ": call the interface (a local or a parameter typed " + iface +
                                                         ") and declare @consumes " + iface,
                                                     location));
    }
  }
  for (const auto& d : declared) {
    bool behaves = false;
    for (const auto& pair : model_->entities()) {
      const std::string subject = model::Identity(pair.first).subject().text();
      if (pair.second.kind == model::Kind::Sequence && !subject.empty() && inside(subject, d.first)) {
        behaves = true;
        break;
      }
    }
    if (!behaves) continue;  // a box drawn before its behaviour: its uses are a plan, not yet a claim
    for (const auto& iface : d.second) {
      if (called[d.first].count(iface)) continue;
      found_.push_back(context_->metamodel().finding(model::Code::W_CONSUMES_UNUSED,
                                                     d.first + " declares @consumes " + iface + ", and none of its behaviours calls it",
                                                     written[{d.first, iface}]));
    }
  }
}

}  // namespace check
}  // namespace ds
