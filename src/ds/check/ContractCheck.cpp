#include <algorithm>
#include <cctype>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <vector>

#include "Shared.hpp"
#include "ds/check/Evaluator.hpp"
#include "ds/check/Steps.hpp"
#include "ds/machine/Machine.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Model.hpp"

namespace ds {
namespace check {

namespace {

// whether a pattern of a diagram names something the workspace declares: an identity, a prefix of one, a
// template written without its arity, the subject of a scenario (DS-212)
bool namesSomething(const model::Model& model, const std::string& written) {
  std::string pattern = written;
  const auto slash = pattern.find(" / ");
  if (slash != std::string::npos) pattern = pattern.substr(0, slash);  // a scenario: its behaviour's subject
  if (pattern.empty()) return true;
  std::string base = pattern;
  if (base.back() == '*') base.pop_back();
  while (!base.empty() && (base.back() == '@' || base.back() == ':')) base.pop_back();
  if (base.empty() || model.find(base)) return true;
  for (const auto& pair : model.entities()) {
    if (pair.second.expected) continue;
    const std::string& id = pair.first;
    if (pattern.back() == '*' && id.rfind(base, 0) == 0) return true;                              // a prefix of something declared
    if (id.size() > base.size() && id.rfind(base, 0) == 0 && id[base.size()] == '<') return true;  // a template
  }
  return false;
}

}  // namespace

std::string ContractCheck::name() const { return "contracts"; }

std::vector<std::string> ContractCheck::needs() const {
  // the expansions behaviour typing made, over the links the linker made, and what escapes each operation
  return {"linker", "behaviour", "failures"};
}

std::vector<model::Finding> ContractCheck::check(const Context& context) {
  context_ = &context;
  model_ = &context.model();
  resolver_ = &context.resolver();
  found_.clear();
  checkInterfaces();
  checkRealisations();
  checkUseCases();
  checkDiagrams();
  checkScenarios();
  checkHandlers();
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    const bool operation = e.kind == model::Kind::Operation || e.kind == model::Kind::Method || e.kind == model::Kind::Function;
    if (operation && !e.expected) checkFailures(e);
  }
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

std::string ContractCheck::outcomeOf(const std::string& sequence, const std::string& path, std::set<std::string>* seen) const {
  if (!seen->insert(model::Identity(sequence).withScenario(path).text()).second) return "";
  const std::vector<const model::Entity*> messages = model::pathOf(*model_, sequence, path, nullptr);
  std::string wanted;  // the name of the result the behaviour returns; empty: its last call decides
  for (auto it = messages.rbegin(); it != messages.rend(); ++it) {
    const model::Entity& m = **it;
    const auto result = m.fields.find("result");
    if (m.fields.count("return")) {
      const std::string value = result == m.fields.end() ? "" : result->second;
      if (value.empty()) return "";
      if (!(std::isdigit(static_cast<unsigned char>(value.front())) || value.front() == '-')) {
        wanted = value;
        continue;
      }
      return value;
    }
    if (!wanted.empty() && (result == m.fields.end() || result->second != wanted)) continue;
    const auto expands = m.fields.find("expands");
    const auto into = m.fields.find("expands-path");
    return expands == m.fields.end() ? "" : outcomeOf(expands->second, into == m.fields.end() ? "" : into->second, seen);
  }
  return "";
}

// A use case is a contract (DS-77): what goes in, what comes out, the outcomes it ends in. A
// specific one keeps its base's signature and outcomes; an interface operation that realises it
// takes and returns the same and handles every outcome as a scenario of its behaviour; a scenario
// no outcome names is behaviour no use case asked for.
void ContractCheck::checkUseCases() {
  const auto isUseCase = [this](const std::string& id) {
    const model::Entity* e = model_->find(id);
    return e && !e->expected && e->kind == model::Kind::UseCase;
  };
  std::map<std::string, std::string> base;
  std::map<std::string, std::vector<model::Relation>> realisedBy;  // copies: the index answers with copies
  for (const auto& r : model_->index().all()) {
    const model::Entity* from = model_->find(r.from);
    if (!from) continue;
    if (r.kind == "inherits" && from->kind == model::Kind::UseCase) {
      if (isUseCase(r.to))
        base[r.from] = r.to;
      else
        found_.push_back(
            {"W-USECASE-UNDECLARED", "warning", r.from + " specialises '" + r.to + "', which is no use case of the workspace", r.location});
    } else if (r.kind == "realises" && from->kind == model::Kind::Operation) {
      if (isUseCase(r.to))
        realisedBy[r.to].push_back(r);
      else
        found_.push_back(
            {"W-USECASE-UNDECLARED", "warning", r.from + " realises '" + r.to + "', which is no use case of the workspace", r.location});
    }
  }
  // the chain of a use case up to its most generic, never round a cycle
  const auto chain = [&base](const std::string& id) {
    std::vector<std::string> out{id};
    std::set<std::string> seen{id};
    for (auto it = base.find(id); it != base.end() && seen.insert(it->second).second; it = base.find(it->second)) out.push_back(it->second);
    return out;
  };
  const auto outcomes = [this, &chain](const std::string& id) {
    std::set<std::string> out;
    for (const auto& uc : chain(id))
      for (const auto& member : model_->index().members(uc)) {
        const model::Entity& held = *model_->find(member);
        if (held.kind == model::Kind::Outcome) out.insert(held.name);
      }
    return out;
  };
  const auto contract = [this, &chain](const std::string& id) -> const model::Entity* {  // the nearest that writes a signature
    for (const auto& uc : chain(id)) {
      const model::Entity* e = model_->find(uc);
      if (e && e->fields.count("operation")) return e;
    }
    return nullptr;
  };
  const auto signature = [](const model::Entity& e) {
    std::string out = "(";
    for (size_t i = 0; i < e.params.size(); ++i) out += (i ? ", " : "") + model::Identity(e.params[i].type).lastName();
    const auto returns = e.fields.find("returns");
    return out + ")" + (returns == e.fields.end() ? std::string() : " -> " + model::Identity(returns->second).lastName());
  };
  for (const auto& pair : base) {  // a specific use case keeps the contract of its base
    const model::Entity* own = model_->find(pair.first);
    const model::Entity* inherited = contract(pair.second);
    if (own && inherited && own->fields.count("operation") && signature(*own) != signature(*inherited)) {
      found_.push_back(
          {"W-USECASE-SIGNATURE", "warning",
           pair.first + " specialises " + pair.second + " but takes and gives " + signature(*own) + ", not " + signature(*inherited),
           own->location});
    }
  }
  std::set<std::string> realised;
  for (const auto& pair : realisedBy) {
    for (const auto& uc : chain(pair.first)) realised.insert(uc);
    const model::Entity* promised = contract(pair.first);
    const std::set<std::string> ends = outcomes(pair.first);
    for (const model::Relation& r : pair.second) {
      const model::Entity* op = model_->find(r.from);
      if (promised && signature(*op) != signature(*promised)) {
        found_.push_back({"W-USECASE-SIGNATURE", "warning",
                          op->id + " realises " + pair.first + " but takes and gives " + signature(*op) + ", not " + signature(*promised),
                          r.location});
      }
      const std::string behaviour = model::Identity(op->id).sequence().text();
      for (const auto& end : ends) {
        if (end == "default" || model_->find(model::Identity(behaviour).withScenario(end).text()) ||
            model::hasScenario(*model_, behaviour, end))
          continue;
        found_.push_back({"W-USECASE-OUTCOME-UNHANDLED", "warning",
                          op->id + " realises " + pair.first + " and has no scenario for its outcome '" + end + "'", r.location});
      }
      for (const auto& other : model_->entities()) {  // a scenario written apart that no outcome names
        const model::Entity& s = other.second;
        if (s.kind != model::Kind::Sequence || model::Identity(model::Identity(s.id).subject().text()).sequence().text() != behaviour ||
            model::Identity(s.id).scenario().empty())
          continue;
        const std::string scenario = s.id.substr(behaviour.size() + 1);
        if (!ends.count(scenario)) {
          found_.push_back({"W-SCENARIO-NO-OUTCOME", "warning",
                            op->id + " handles '" + scenario + "', and " + pair.first + " names no such outcome", s.location});
        }
      }
    }
  }
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.kind != model::Kind::UseCase || e.expected || realised.count(e.id)) continue;
    found_.push_back({"W-USECASE-UNREALISED", "warning",
                      e.id + " is realised by no interface operation, nor is any use case that specialises it", e.location});
  }
}

void ContractCheck::checkInterfaces() {
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.kind == model::Kind::State) {
      const std::string subjectId = model::Identity(e.id).machineSubject().text();  // e is a state machine: its id names it
      const model::Entity* subject = subjectId.empty() ? nullptr : model_->find(subjectId);
      if (subject && subject->kind == model::Kind::Interface) checkViews(e);
      if (subject && subject->kind == model::Kind::Class) checkStates(e);
    }
    if (e.kind != model::Kind::Operation) continue;
    const std::string behaviour = model::Identity(e.id).sequence().text();
    if (!model_->find(behaviour)) {
      found_.push_back({"W-OPERATION-NO-BEHAVIOUR", "warning", e.id + " has no sequence that says how it is handled", e.location});
    }
    for (const auto& field : e.fields) {
      if (field.first.rfind("exit ", 0) != 0) continue;
      const std::string code = field.first.substr(5);
      std::string id = field.second == "default" ? behaviour : model::Identity(behaviour).withScenario(field.second).text();
      std::string path;
      if (!model_->find(id) && model::hasScenario(*model_, behaviour, field.second)) {
        id = behaviour;  // a named branch of the one behaviour
        path = field.second;
      }
      if (!model_->find(id)) {
        found_.push_back({"W-SCENARIO-UNDECLARED", "warning",
                          e.id + " exits " + code + " on '" + field.second + "', and it has no such scenario", e.location});
        continue;
      }
      std::set<std::string> seen;
      const std::string outcome = outcomeOf(id, path, &seen);
      if (!outcome.empty() && outcome != code) {
        found_.push_back({"W-OUTCOME-MISMATCH", "warning",
                          e.id + " promises exit " + code + " on '" + field.second + "', and that trace ends in " + outcome, e.location});
      }
    }
  }
}

// A class that realises an interface and declares one of its operations declares it with the same
// signature: two written facts that must agree (DS-215). What the class does not declare is no finding.
void ContractCheck::checkRealisations() {
  // a type as the idea it commits to: no qualifiers and no spaces; the rest is the code's text (DS-18)
  const auto idea = [](const std::string& text) {
    static const std::regex qualifier(R"(\b[A-Za-z_][A-Za-z0-9_]*::)");  // built once, not on every call (DS-335)
    static const std::regex spaces(R"(\s+)");
    return std::regex_replace(std::regex_replace(text, qualifier, ""), spaces, "");
  };
  const auto signature = [&idea](const model::Entity& e) {
    std::string out = "(";
    for (size_t i = 0; i < e.params.size(); ++i) out += (i ? ", " : "") + idea(e.params[i].type);
    const auto returns = e.fields.find("returns");
    return out + ") -> " + idea(returns == e.fields.end() ? "" : returns->second);
  };
  for (const auto& r : model_->index().ofKind("realises")) {
    const model::Entity* cls = model_->find(r.from);
    const model::Entity* iface = model_->find(r.to);
    if (!cls || !iface || cls->kind != model::Kind::Class || iface->kind != model::Kind::Interface || iface->expected) continue;
    for (const auto& id : model_->index().members(iface->id)) {
      const model::Entity* op = model_->find(id);
      if (!op || op->kind != model::Kind::Operation) continue;
      const model::Entity* method = resolver_->member(cls->id, op->name);
      if (!method || method->kind != model::Kind::Method) continue;
      if (signature(*method) == signature(*op)) continue;
      found_.push_back({"W-INTERFACE-SIGNATURE", "warning",
                        method->id + " takes and gives " + signature(*method) + ", and " + op->id + ", which " + cls->id +
                            " realises, takes and gives " + signature(*op),
                        method->location});
    }
  }
}

void ContractCheck::checkViews(const model::Entity& state) {
  std::set<std::string> views;
  for (const auto& member : model_->index().members(state.id)) {
    const model::Entity& held = *model_->find(member);
    if (held.kind == model::Kind::View) views.insert(held.name);
  }
  if (views.empty()) return;  // the details of a view are optional: a bare machine asks nothing of them
  std::map<std::string, std::set<std::string>> next;
  const auto moves = state.fields.find("transitions");
  if (moves != state.fields.end()) {
    for (const auto& move : splitTrimmed(moves->second, '\n')) {
      const auto gt = move.find('>');
      const std::string from = move.substr(0, gt);
      const std::string to = move.substr(gt + 1, move.find('>', gt + 1) - gt - 1);
      for (const std::string& name : {from, to}) {
        if (!views.count(name)) {
          found_.push_back({"W-VIEW-UNDECLARED", "warning",
                            "the navigation of " + state.name + " names '" + name + "', which is not one of its views", state.location});
        }
      }
      if (from != to) next[from].insert(to);  // staying where you are is no way in or out
    }
  }
  const auto initial = state.fields.find("initial");
  std::set<std::string> reached;
  std::vector<std::string> pending;
  if (initial != state.fields.end()) pending.push_back(initial->second);
  while (!pending.empty()) {
    const std::string view = pending.back();
    pending.pop_back();
    if (!reached.insert(view).second) continue;
    for (const auto& to : next[view]) pending.push_back(to);
  }
  for (const auto& view : views) {
    const model::Entity* v = model_->find(state.id + "::" + view);
    if (!reached.count(view)) {
      found_.push_back({"W-VIEW-UNREACHABLE", "warning", "nothing leads to the view " + view + " of " + state.name, v->location});
    } else if (views.size() > 1 && next[view].empty()) {
      found_.push_back({"W-VIEW-NO-WAY-OUT", "warning", "nothing leaves the view " + view + " of " + state.name, v->location});
    }
  }
}

void ContractCheck::checkStates(const model::Entity& state) {
  // a state of a class's machine that no transition leads to from the initial one is never entered (DS-53).
  // The machine starts where it is written to, or in the state the defaults of its subject are in (DS-253);
  // with neither, where it starts cannot be told
  const auto moves = state.fields.find("transitions");
  if (moves == state.fields.end()) return;
  const std::string initial = machine::Machine(*model_, state).initial();
  if (initial.empty()) return;
  std::vector<std::string> states{initial};
  std::map<std::string, std::set<std::string>> next;
  for (const auto& move : splitTrimmed(moves->second, '\n')) {
    const auto gt = move.find('>');
    if (gt == std::string::npos) continue;
    const std::string from = move.substr(0, gt);
    const std::string to = move.substr(gt + 1, move.find('>', gt + 1) - gt - 1);
    for (const std::string& name : {from, to})
      if (std::find(states.begin(), states.end(), name) == states.end()) states.push_back(name);
    next[from].insert(to);
  }
  std::set<std::string> reached;
  std::vector<std::string> pending{initial};
  while (!pending.empty()) {
    const std::string at = pending.back();
    pending.pop_back();
    if (!reached.insert(at).second) continue;
    for (const auto& to : next[at]) pending.push_back(to);
  }
  for (const auto& name : states) {
    if (reached.count(name)) continue;
    found_.push_back({"W-STATE-UNREACHABLE", "warning",
                      "nothing leads to the state " + name + " of " + state.name + " from its initial state " + initial, state.location});
  }
}

void ContractCheck::checkFailures(const model::Entity& op) {
  // an interface operation is a boundary: what escapes it is said in its signature, or nothing may; any
  // other operation is held to its signature only when it writes one (DS-189)
  const bool boundary = op.kind == model::Kind::Operation;
  const bool promises = op.fields.count("fails") > 0;
  if (!promises && !boundary) return;
  std::set<std::string> declared;
  for (const auto& name : text::splitTrimmed(op.field("fails"), ','))
    if (name != "nothing") declared.insert(name);
  std::set<std::string> escaping;
  for (const auto& name : text::splitTrimmed(op.field("escapes"), ',')) {
    escaping.insert(name);
    if (declared.count(name)) continue;
    const std::string how = op.field("escape." + name);
    found_.push_back({"W-FAILURE-UNDECLARED", "warning",
                      op.id + " lets " + name + " escape (" + how + "), and " +
                          (promises ? "its signature does not say it may fail with it" : "a boundary must say what it fails with"),
                      op.location});
  }
  bool behaviour = false;
  for (const auto& r : model_->index().to(op.id)) behaviour |= r.kind == "behaviour-of";
  if (!behaviour) return;  // with no behaviour, the signature is what it fails with
  for (const auto& name : declared) {
    if (escaping.count(name)) continue;
    found_.push_back(
        {"W-FAILURE-UNREACHED", "warning", op.id + " says it may fail with " + name + ", and no path of its behaviour does", op.location});
  }
}

// A handler names a failure that none of the steps of its attempt can fail with: it handles nothing, most often
// a misspelt name (DS-224). Said only when every call in the attempt says what it fails with.
void ContractCheck::checkHandlers() {
  const model::Index index = model_->index();
  for (const auto& pair : model_->entities()) {
    const model::Entity& handler = pair.second;
    if (handler.kind != model::Kind::Frame || handler.field("kind") != "handler") continue;
    const std::string attempt = index.parent(handler.id);
    std::set<std::string> possible;
    bool known = true;
    std::vector<std::string> pending{attempt};
    while (!pending.empty()) {
      const std::string at = pending.back();
      pending.pop_back();
      for (const auto& child : index.children(at)) {
        const model::Entity* c = model_->find(child);
        if (!c) continue;
        if (c->kind == model::Kind::Frame) {
          if (!(at == attempt && c->field("kind") == "handler")) pending.push_back(child);  // the handlers are not its steps
          continue;
        }
        if (c->kind != model::Kind::Message) continue;
        if (!c->field("fails").empty()) possible.insert(c->field("fails"));
        for (const auto& r : index.from(child)) {
          if (r.kind != "calls") continue;
          const model::Entity* op = model_->find(r.to);
          if (!op) continue;
          bool behaves = false;
          for (const auto& b : index.to(op->id)) behaves = behaves || b.kind == "behaviour-of";
          if (!behaves && op->fields.count("fails") == 0) known = false;  // says nothing of how it fails
          for (const auto& f : splitTrimmed(op->field("escapes"), ',')) possible.insert(f);
        }
      }
    }
    const std::string failure = handler.field("failure");
    if (!known || failure.empty() || possible.count(failure)) continue;
    found_.push_back(
        {"W-HANDLER-UNREACHED", "warning",
         "the handler of " + failure + " handles nothing: no step of its attempt fails with it" +
             (possible.empty() ? "" : " (they fail with " + join(std::vector<std::string>(possible.begin(), possible.end()), ", ") + ")"),
         handler.location});
  }
}

void ContractCheck::checkScenarios() {
  // a scenario has one name: a branch of the behaviour, or a behaviour of its own, never both nor twice
  std::map<std::string, int> named;
  for (const auto& pair : model_->entities()) {
    const model::Entity& f = pair.second;
    if (f.kind != model::Kind::Frame || f.field("kind") == "branch") continue;
    // a loop or an opt names its scenario itself; an alt's sides name theirs, where each is written
    std::vector<const model::Entity*> namers{&f};
    if (f.field("kind") == "alt") namers = {model_->find(f.id + "::b0"), model_->find(f.id + "::b1")};
    for (const model::Entity* side : namers) {
      if (!side) continue;
      const std::string name = side->field("scenario");
      if (name.empty()) continue;
      const std::string base =
          model::Identity(model::Identity(f.scope).subject().text()).sequence().text();  // the behaviour, whatever scenario holds the frame
      if (++named[model::Identity(base).withScenario(name).text()] == 2 || model_->find(model::Identity(base).withScenario(name).text())) {
        found_.push_back(
            {"W-SCENARIO-TWICE", "warning", base + " names the scenario '" + name + "' twice: a scenario is one path", side->location});
      }
    }
  }
}

void ContractCheck::checkDiagrams() {
  for (const auto& pair : model_->entities()) {
    if (pair.second.kind != model::Kind::Diagram) continue;
    // a sequence diagram that asks for a scenario: it is a behaviour of its own, or a named branch of one
    const auto include = pair.second.fields.find("include");
    if (include != pair.second.fields.end() && pair.second.fields.count("kind") &&
        (pair.second.fields.at("kind") == "sequence" || pair.second.fields.at("kind") == "tour")) {
      for (const auto& pattern : splitTrimmed(include->second, ',')) {
        const auto slash = pattern.find(" / ");
        if (slash == std::string::npos) continue;
        const std::string behaviour = model::Identity(pattern.substr(0, slash)).sequence().text();
        const std::string scenario = pattern.substr(slash + 3);
        if (!model_->find(behaviour) || model_->find(model::Identity(behaviour).withScenario(scenario).text()) ||
            model::hasScenario(*model_, behaviour, scenario))
          continue;
        found_.push_back({"W-SCENARIO-UNDECLARED", "warning", pair.second.name + " asks for " + pattern + ", a scenario nobody declares",
                          pair.second.location});
      }
    }
    // a pattern that names what nothing declares is a broken reference, as in the model (DS-212); a box that
    // exists and is empty is not: a level with no children is valid
    for (const char* key : {"include", "exclude", "open", "close", "unfold", "fold"}) {
      for (const auto& pattern : splitTrimmed(pair.second.field(key), ',')) {
        // a cut may name a kind of box instead of a box (DS-229)
        const bool kind =
            (key == std::string("open") || key == std::string("close")) &&
            (pattern == model_->metamodel().nameOf(model::Kind::Module) || pattern == model_->metamodel().nameOf(model::Kind::Package));
        if (kind || namesSomething(*model_, pattern)) continue;
        found_.push_back({"W-DIAGRAM-UNDECLARED", "warning",
                          pair.second.name + " " + key + "s " + pattern + ", which the workspace declares nowhere", pair.second.location});
      }
    }
    // the diagram a cut is taken from is a reference like any other
    const std::string lender = pair.second.field("cut");
    const model::Entity* lent = lender.empty() ? nullptr : model_->find("diagram:" + lender);
    if (!lender.empty() && (!lent || lent->kind != model::Kind::Diagram)) {
      found_.push_back({"W-DIAGRAM-UNDECLARED", "warning",
                        pair.second.name + " takes the cut of " + lender + ", a diagram the workspace declares nowhere",
                        pair.second.location});
    }
    const auto depth = pair.second.fields.find("depth");
    if (depth == pair.second.fields.end() || depth->second == "*") continue;
    if (depth->second.empty() || depth->second.find_first_not_of("0123456789") != std::string::npos) {
      found_.push_back({"W-DIAGRAM-DEPTH", "warning",
                        pair.second.name + " asks for depth '" + depth->second + "': a number, or * for every level",
                        pair.second.location});
    }
  }
}

}  // namespace check
}  // namespace ds
