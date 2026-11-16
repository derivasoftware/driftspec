#include <algorithm>
#include <regex>
#include <sstream>

#include "ds/axis/Cut.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/Words.hpp"
#include "ds/query/Filter.hpp"
#include "ds/query/Walk.hpp"

namespace ds {
namespace query {

using model::join;
using model::split;

namespace {

/// What a class diagram draws as a box: a class, an enum, an interface or a function of a module.
bool isBox(const model::Entity& e) {
  return e.kind == model::Kind::Class || e.kind == model::Kind::Enum || e.kind == model::Kind::Function || e.kind == model::Kind::Interface;
}

/// A box that holds others: a module, or a package below the system (the outermost package is the drawing).
bool isContainer(const model::Entity& e) { return e.kind == model::Kind::Module || (e.kind == model::Kind::Package && !e.scope.empty()); }

bool drawable(const model::Entity& e) { return isBox(e) || isContainer(e); }

/// The relations a class diagram draws as edges, the ones its neighbours follow.
bool drawnAsEdge(const std::string& relation) {
  return relation == "inherits" || relation == "owns" || relation == "refs" || relation == "shares" || relation == "uses" ||
         relation == "bounded-by" || relation == "realises" || relation == "consumes" || relation == "sends";
}

int number(const std::string& written, int every) {
  if (written == "*") return every;
  return std::regex_match(written, std::regex("[0-9]+")) ? std::stoi(written) : 0;
}

bool isArtefact(const model::Entity& e) {
  return e.kind == model::Kind::Requirement || e.kind == model::Kind::Constraint || e.kind == model::Kind::Verification;
}

/// The scenario of a behaviour taken as written: every branch.
const char* const kWhole = kEveryBranch;

}  // namespace

// one reading of a pattern for every kind of diagram (DS-212): an identity; a prefix with * over the
// identity or the scope (`p@m::*` is the module p@m and everything in it); a module named as such; a
// template written without its arity
bool inPattern(const model::Model& model, const std::string& pattern, const model::Entity& entity) {
  if (pattern == entity.id) return true;
  if (!pattern.empty() && pattern.back() == '*') {
    const std::string prefix = pattern.substr(0, pattern.size() - 1);
    if (entity.id.rfind(prefix, 0) == 0 || entity.scope.rfind(prefix, 0) == 0) return true;
    const bool modulePrefix = prefix.size() > 2 && prefix.compare(prefix.size() - 2, 2, "::") == 0;
    return modulePrefix && entity.scope == prefix.substr(0, prefix.size() - 2);
  }
  if (const model::Entity* m = model.find(pattern)) {
    if (m->kind == model::Kind::Module) return entity.scope == pattern || entity.scope.rfind(pattern + "::", 0) == 0;
  }
  const auto lt = entity.id.find('<');
  return lt != std::string::npos && entity.id.substr(0, lt) == pattern;
}

// ---- class diagrams --------------------------------------------------------------------------

ClassFilter::ClassFilter(std::vector<std::string> include, std::vector<std::string> exclude, int neighbours, std::string cutOf)
    : include_(std::move(include)), exclude_(std::move(exclude)), neighbours_(neighbours), cutOf_(std::move(cutOf)) {}

std::string drawnFrom(const model::Model& model, const model::Entity& entity) {
  if (isBox(entity) || entity.kind == model::Kind::Module || entity.kind == model::Kind::Package) return entity.id;  // a box's own @uses
  if (entity.kind == model::Kind::Sequence || entity.kind == model::Kind::State) {
    const model::Entity* subject = model.find(model::Identity(entity.id).subject().text());
    if (!subject && entity.kind == model::Kind::State) subject = model.find(model::Identity(entity.id).machineSubject().text());
    if (subject) return isBox(*subject) ? subject->id : subject->scope;
  }
  return entity.scope;
}

bool ClassFilter::matches(const model::Model& model, const std::string& pattern, const model::Entity& entity) const {
  if (pattern == entity.id) return true;
  if (entity.has("extern")) return false;  // what lives outside the workspace is drawn only when named
  return inPattern(model, pattern, entity);
}

std::set<std::string> ClassFilter::boxes(const model::Model& model) const {
  std::set<std::string> selected;
  for (const auto& pattern : include_) {
    for (const auto& pair : model.entities()) {
      if (drawable(pair.second) && !pair.second.expected && matches(model, pattern, pair.second)) selected.insert(pair.first);
    }
  }
  for (const auto& pattern : exclude_) {
    for (auto it = selected.begin(); it != selected.end();) {
      it = matches(model, pattern, *model.find(*it)) ? selected.erase(it) : std::next(it);
    }
  }
  // neighbours: n widens the selection to what the selected entities relate to, n steps away
  for (int step = 0; step < neighbours_; ++step) {
    std::set<std::string> grown = selected;
    for (const auto& r : model.index().all()) {
      if (!drawnAsEdge(r.kind)) continue;
      const model::Entity* from = model.find(r.from);
      const model::Entity* to = model.find(r.to);
      if (!from || !to) continue;
      const std::string fromClass = drawnFrom(model, *from);
      const model::Entity* fromEntity = model.find(fromClass);
      if (!fromEntity || !isBox(*fromEntity) || !isBox(*to) || to->expected || fromEntity->expected) continue;
      if (selected.count(fromClass) && !to->has("extern")) grown.insert(r.to);
      if (selected.count(r.to) && !fromEntity->has("extern")) grown.insert(fromClass);
    }
    selected = grown;
  }
  return selected;
}

std::set<std::string> ClassFilter::containers(const model::Model& model, const std::set<std::string>& taken) {
  // modules of the selected classes become containers (R2), and so do nested packages (the HLD
  // boxes); the outermost package is the drawing itself
  std::set<std::string> out;
  auto up = [](const std::string& s) {
    return model::Scope(model::Scope(s).outer().text()).box().text();
  };  // ds@ds::b → ds@ds, app@core@model → app@core
  // every box is drawn, with or without children (DS-218): a container taken is drawn itself, and the boxes
  // around anything taken are drawn around it; a package scope (p@q@) names the package p@q
  for (const auto& id : taken) {
    const model::Entity* e = model.find(id);
    if (!e || (!isBox(*e) && !isContainer(*e))) continue;
    const std::string start = isContainer(*e) ? id : model::Scope(e->scope).box().text();
    for (std::string s = start; !s.empty(); s = up(s)) {
      const model::Entity* m = model.find(s);
      if (m && isContainer(*m)) out.insert(s);
    }
  }
  return out;
}

std::set<std::string> ClassFilter::select(const model::Model& model) const {
  const std::set<std::string> taken = boxes(model);
  const std::set<std::string> around = containers(model, taken);
  std::set<std::string> out = taken;
  for (const auto& c : around) out.insert(c);
  const model::Entity* diagram = cutOf_.empty() ? nullptr : model.find(cutOf_);
  if (!diagram) return out;
  const axis::Cut cut(model, *diagram);
  if (!cut.written()) return out;
  std::set<std::string> shown;
  for (const auto& id : out)
    if (cut.standsFor(id) == id) shown.insert(id);  // what a closed box holds, the box stands for
  return shown;
}

// ---- sequence diagrams -----------------------------------------------------------------------

SequenceFilter::SequenceFilter(std::vector<std::string> include, int depth, std::string cutOf)
    : include_(std::move(include)), depth_(depth), cutOf_(std::move(cutOf)) {}

std::vector<Behaviour> SequenceFilter::behaviours(const model::Model& model) const {
  std::vector<Behaviour> out;
  for (const auto& pattern : include_) {
    for (const auto& pair : model.entities()) {
      const model::Entity& e = pair.second;
      if (e.kind != model::Kind::Sequence || e.expected) continue;
      const std::string subjectId = model::Identity(e.id).subject().text();
      const bool prefix = !pattern.empty() && pattern.back() == '*' && e.id.rfind(pattern.substr(0, pattern.size() - 1), 0) == 0;
      // as it is written: `Class::op` the default, `Class::op / scenario` that one, `Class::op*` them all
      const bool isDefault = e.fields.find("scenario") == e.fields.end();
      const bool named = !isDefault && pattern == subjectId + " / " + e.fields.at("scenario");
      if (pattern == e.id || (pattern == subjectId && isDefault) || named || prefix) out.push_back({&e, kWhole});
      // `Class::op / scenario` with no behaviour of its own: a named branch of the default one
      const auto slash = pattern.find(" / ");
      if (isDefault && slash != std::string::npos && pattern.substr(0, slash) == subjectId &&
          !model.find(model::Identity(subjectId).sequence(pattern.substr(slash + 3)).text()) &&
          model::hasScenario(model, e.id, pattern.substr(slash + 3))) {
        out.push_back({&e, pattern.substr(slash + 3)});
      }
    }
  }
  return out;
}

std::set<std::string> SequenceFilter::select(const model::Model& model) const {
  std::set<std::string> out;
  // on a cut each participant is the box that stands for it, and the calls followed are the ones it says (DS-231)
  const model::Entity* diagram = cutOf_.empty() ? nullptr : model.find(cutOf_);
  const std::unique_ptr<axis::Cut> cut(diagram ? new axis::Cut(model, *diagram) : nullptr);
  auto on = [&cut](const std::string& id) { return cut ? cut->standsFor(id) : id; };
  for (const Behaviour& b : behaviours(model)) {
    const model::Entity* subject = model.find(model::Identity(b.sequence->id).subject().text());
    const bool function = subject && subject->kind == model::Kind::Function;
    const bool method = subject && (subject->kind == model::Kind::Method || subject->kind == model::Kind::Operation || function);
    const model::Entity* cls = method ? model.find(subject->scope) : subject;
    // a scenario of a use case starts from whoever asks, who is no part of the design (DS-232)
    if (cls && cls->kind != model::Kind::UseCase) out.insert(on(cls->id));
    for (const Walked& w : Walker(model).walk(b.sequence->id, b.path, depth_, cut.get())) {  // what it calls takes part (DS-130)
      const model::Entity* m = model.find(w.message);
      if (m->field("return") == "true") continue;
      for (const char* end : {"type", "from-type"}) {  // who receives, and who sends when it is written
        const std::string typeId = m->field(end);
        if (!typeId.empty() && model.find(typeId)) out.insert(on(typeId));
      }
    }
  }
  std::set<std::string> known;
  for (const auto& id : out)
    if (model.find(id)) known.insert(id);
  return known;
}

// ---- state diagrams --------------------------------------------------------------------------

StateFilter::StateFilter(std::vector<std::string> include) : include_(std::move(include)) {}

std::vector<std::string> StateFilter::machines(const model::Model& model) const {
  std::vector<std::string> out;
  for (const auto& pattern : include_) {
    const model::Entity* state = model.find(model::Identity(pattern).state().text());
    if (!state) state = model.find(pattern);
    if (state && state->kind == model::Kind::State) out.push_back(state->id);
  }
  return out;
}

std::set<std::string> StateFilter::select(const model::Model& model) const {
  std::set<std::string> out;
  for (const auto& machine : machines(model)) {
    out.insert(machine);
    for (const auto& pair : model.entities())  // the states the design describes
      if (pair.second.scope == machine) out.insert(pair.first);
  }
  return out;
}

// ---- requirements diagrams -------------------------------------------------------------------

RequirementsFilter::RequirementsFilter(std::vector<std::string> include, bool design, bool verifications, std::string cutOf)
    : include_(std::move(include)), design_(design), verifications_(verifications), cutOf_(std::move(cutOf)) {}

std::set<std::string> RequirementsFilter::select(const model::Model& model) const {
  // the roots: artefacts named, or every artefact of a package / module
  std::set<std::string> roots;
  for (const auto& pattern : include_) {
    for (const auto& pair : model.entities()) {
      const model::Entity& e = pair.second;
      if (!isArtefact(e) || e.expected) continue;
      if (inPattern(model, pattern, e)) roots.insert(e.id);  // the same reading as a class diagram's (DS-212)
    }
  }
  // on a cut, what answers to a requirement is the box that stands for it (DS-231)
  const model::Entity* diagram = cutOf_.empty() ? nullptr : model.find(cutOf_);
  const std::unique_ptr<axis::Cut> cut(diagram ? new axis::Cut(model, *diagram) : nullptr);
  // the umbrella: what refines a root (down, transitively), what verifies it, what realises it
  std::set<std::string> selected = roots;
  bool grew = true;
  while (grew) {
    grew = false;
    for (const auto& r : model.index().ofKinds({"refines", "verifies", "requires", "constrained-by"})) {
      if ((r.kind == "refines" && selected.count(r.to) && !selected.count(r.from)) ||
          (verifications_ && r.kind == "verifies" && selected.count(r.to) && !selected.count(r.from)) ||
          (design_ && (r.kind == "requires" || r.kind == "constrained-by") && selected.count(r.to) && !selected.count(r.from))) {
        const model::Entity* from = model.find(r.from);
        if (!from || from->expected) continue;
        // whatever answers to a requirement is drawn under it: a box, an interface, a function, a rule; a member or
        // an operation is lifted to the class or the interface that holds it (DS-220)
        if (from->kind == model::Kind::Method || from->kind == model::Kind::Attribute || from->kind == model::Kind::Operator ||
            from->kind == model::Kind::Operation)
          from = model.find(from->scope);
        if (from && cut && !isArtefact(*from)) from = model.find(cut->standsFor(from->id));
        if (!from || from->expected || selected.count(from->id)) continue;
        selected.insert(from->id);
        grew = true;
      }
    }
  }
  return selected;
}

// ---- the filter of each kind of diagram ------------------------------------------------------

std::unique_ptr<IFilter> filterFor(const model::Entity& diagram) {
  if (diagram.kind != model::Kind::Diagram) return nullptr;
  const std::string kind = diagram.field("kind");
  const std::vector<std::string> include = split(diagram.field("include"), ',');
  if (kind == "sequence" || kind == "tour")
    return std::unique_ptr<IFilter>(new SequenceFilter(include, number(diagram.field("depth"), 1000), diagram.id));
  if (kind == "state") return std::unique_ptr<IFilter>(new StateFilter(include));
  if (kind == "requirement") {
    const std::vector<std::string> shown = split(diagram.field("show"), ',');
    const std::set<std::string> show(shown.begin(), shown.end());
    return std::unique_ptr<IFilter>(
        new RequirementsFilter(include, show.empty() || show.count("design"), show.empty() || show.count("verifications"), diagram.id));
  }
  return std::unique_ptr<IFilter>(
      new ClassFilter(include, split(diagram.field("exclude"), ','), number(diagram.field("neighbours"), 0), diagram.id));
}

}  // namespace query
}  // namespace ds
