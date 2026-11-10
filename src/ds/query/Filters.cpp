#include <algorithm>
#include <regex>
#include <sstream>

#include "ds/model/Identity.hpp"
#include "ds/query/Filter.hpp"

namespace ds {
namespace query {

namespace {

std::vector<std::string> split(const std::string& text, char sep) {
  std::vector<std::string> out;
  std::stringstream in(text);
  std::string item;
  while (std::getline(in, item, sep)) {
    const auto b = item.find_first_not_of(' ');
    const auto e = item.find_last_not_of(' ');
    if (b != std::string::npos) out.push_back(item.substr(b, e - b + 1));
  }
  return out;
}

std::string fieldOf(const model::Entity& e, const std::string& key) {
  const auto it = e.fields.find(key);
  return it == e.fields.end() ? "" : it->second;
}

/// What a class diagram draws as a box: a class, an enum, an interface or a function of a module.
bool isBox(const model::Entity& e) {
  return e.kind == model::Kind::Class || e.kind == model::Kind::Enum || e.kind == model::Kind::Function || e.kind == model::Kind::Interface;
}

bool drawable(const model::Entity& e) { return isBox(e) || e.kind == model::Kind::Module; }

/// The relations a class diagram draws as edges, the ones its neighbours follow.
bool drawnAsEdge(const std::string& relation) {
  return relation == "inherits" || relation == "owns" || relation == "refs" || relation == "shares" || relation == "uses" ||
         relation == "bounded-by" || relation == "realises";
}

int number(const std::string& written, int every) {
  if (written == "*") return every;
  return std::regex_match(written, std::regex("[0-9]+")) ? std::stoi(written) : 0;
}

bool isArtefact(const model::Entity& e) {
  return e.kind == model::Kind::Requirement || e.kind == model::Kind::Constraint || e.kind == model::Kind::Verification;
}

/// The messages of a behaviour, in the order they are written.
std::vector<const model::Entity*> messagesOf(const model::Model& model, const std::string& sequence) {
  std::vector<const model::Entity*> out;
  for (const auto& pair : model.entities()) {
    if (pair.second.kind == model::Kind::Message && pair.second.scope == sequence) out.push_back(&pair.second);
  }
  std::sort(out.begin(), out.end(), [](const model::Entity* a, const model::Entity* b) {
    return std::stoi(fieldOf(*a, "order")) < std::stoi(fieldOf(*b, "order"));
  });
  return out;
}

/// The scenario of a behaviour taken as written: every branch.
const char* const kWhole = "*";

}  // namespace

// ---- class diagrams --------------------------------------------------------------------------

ClassFilter::ClassFilter(std::vector<std::string> include, std::vector<std::string> exclude, int neighbours, bool closes,
                         std::vector<std::string> open)
    : include_(std::move(include)), exclude_(std::move(exclude)), neighbours_(neighbours), closes_(closes), open_(std::move(open)) {}

namespace {
std::string outerOf(const std::string& id) { return model::boxOf(model::outer(id)); }  // the box outside
std::vector<std::string> pathTo(const std::string& id) {                               // from the outermost box down to the id
  std::vector<std::string> path;
  for (std::string s = id; !s.empty(); s = outerOf(s)) path.insert(path.begin(), s);
  return path;
}
}  // namespace

std::set<std::string> ClassFilter::opened(const std::set<std::string>& containers, const std::vector<std::string>& open) {
  // a box is open when it is named, or when something inside it is: opening a box shows the way to it
  std::set<std::string> out;
  for (const auto& id : containers) {
    for (const auto& pattern : open) {
      const bool star = !pattern.empty() && pattern.back() == '*';
      const std::string prefix = star ? pattern.substr(0, pattern.size() - 1) : pattern;
      if (id != pattern && !(star && id.rfind(prefix, 0) == 0)) continue;
      for (const auto& step : pathTo(id)) out.insert(step);
    }
  }
  return out;
}

std::string ClassFilter::standsFor(const std::string& id, const std::set<std::string>& containers, const std::set<std::string>& opened) {
  for (const auto& step : pathTo(id)) {
    if (containers.count(step) && !opened.count(step)) return step;
  }
  return id;
}

bool ClassFilter::matches(const model::Model& model, const std::string& pattern, const model::Entity& entity) const {
  if (pattern == entity.id) return true;
  if (entity.has("extern")) return false;  // what lives outside the workspace is drawn only when named
  // a prefix with *: ds@* is the whole package, ds@ds::* a module and everything inside (full ids, R4)
  if (!pattern.empty() && pattern.back() == '*') {
    const std::string prefix = pattern.substr(0, pattern.size() - 1);
    return entity.id.rfind(prefix, 0) == 0 || entity.scope.rfind(prefix, 0) == 0;
  }
  // a module named as such: the module and everything inside
  if (const model::Entity* m = model.find(pattern)) {
    if (m->kind == model::Kind::Module) return entity.scope == pattern || entity.scope.rfind(pattern + "::", 0) == 0;
  }
  // a template written without arity: Repository matches Repository<1>
  const auto lt = entity.id.find('<');
  return lt != std::string::npos && entity.id.substr(0, lt) == pattern;
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
    for (const auto& r : model.relations()) {
      if (!drawnAsEdge(r.kind)) continue;
      const model::Entity* from = model.find(r.from);
      const model::Entity* to = model.find(r.to);
      if (!from || !to) continue;
      const std::string fromClass = isBox(*from) ? from->id : from->scope;
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
  auto up = [](const std::string& s) { return model::boxOf(model::outer(s)); };  // ds@ds::b → ds@ds, app@core@model → app@core
  for (const auto& id : taken) {
    const model::Entity* e = model.find(id);
    if (!isBox(*e)) continue;
    for (std::string s = e->scope; !s.empty(); s = up(s)) {
      const model::Entity* m = model.find(s);
      if (m && (m->kind == model::Kind::Module || (m->kind == model::Kind::Package && !m->scope.empty()))) out.insert(s);
    }
  }
  return out;
}

std::set<std::string> ClassFilter::select(const model::Model& model) const {
  const std::set<std::string> taken = boxes(model);
  const std::set<std::string> around = containers(model, taken);
  std::set<std::string> out = taken;
  for (const auto& c : around) out.insert(c);
  if (!closes_) return out;
  const std::set<std::string> open = opened(around, open_);
  std::set<std::string> shown;
  for (const auto& id : out)
    if (standsFor(id, around, open) == id) shown.insert(id);  // what a closed box holds, the box stands for
  return shown;
}

// ---- sequence diagrams -----------------------------------------------------------------------

SequenceFilter::SequenceFilter(std::vector<std::string> include, int depth, bool modules)
    : include_(std::move(include)), depth_(depth), modules_(modules) {}

std::vector<Behaviour> SequenceFilter::behaviours(const model::Model& model) const {
  std::vector<Behaviour> out;
  for (const auto& pattern : include_) {
    for (const auto& pair : model.entities()) {
      const model::Entity& e = pair.second;
      if (e.kind != model::Kind::Sequence || e.expected) continue;
      const std::string subjectId = model::subjectOf(e.id);
      const bool prefix = !pattern.empty() && pattern.back() == '*' && e.id.rfind(pattern.substr(0, pattern.size() - 1), 0) == 0;
      // as it is written: `Class::op` the default, `Class::op / scenario` that one, `Class::op*` them all
      const bool isDefault = e.fields.find("scenario") == e.fields.end();
      const bool named = !isDefault && pattern == subjectId + " / " + e.fields.at("scenario");
      if (pattern == e.id || (pattern == subjectId && isDefault) || named || prefix) out.push_back({&e, kWhole});
      // `Class::op / scenario` with no behaviour of its own: a named branch of the default one
      const auto slash = pattern.find(" / ");
      if (isDefault && slash != std::string::npos && pattern.substr(0, slash) == subjectId &&
          !model.find(model::behaviourId(subjectId, pattern.substr(slash + 3))) &&
          model::hasScenario(model, e.id, pattern.substr(slash + 3))) {
        out.push_back({&e, pattern.substr(slash + 3)});
      }
    }
  }
  return out;
}

void SequenceFilter::walk(const model::Model& model, const model::Entity& sequence, const std::string& path, int depth,
                          std::set<std::string>& out_seen, std::set<std::string>& out_ids) const {
  if (!out_seen.insert(model::withScenario(sequence.id, path)).second) return;  // recursion: taken once
  const std::vector<const model::Entity*> messages =
      path == kWhole ? messagesOf(model, sequence.id) : model::pathOf(model, sequence.id, path, nullptr);
  for (const model::Entity* m : messages) {
    if (fieldOf(*m, "return") == "true") continue;
    const std::string typeId = fieldOf(*m, "type");
    if (!typeId.empty() && model.find(typeId)) out_ids.insert(typeId);  // what it calls takes part
    const std::string expands = fieldOf(*m, "expands");
    if (!expands.empty() && depth > 0) {
      if (const model::Entity* child = model.find(expands)) walk(model, *child, fieldOf(*m, "expands-path"), depth - 1, out_seen, out_ids);
    }
  }
}

std::set<std::string> SequenceFilter::select(const model::Model& model) const {
  std::set<std::string> out;
  for (const Behaviour& b : behaviours(model)) {
    const std::string subjectId = model::subjectOf(b.sequence->id);
    const model::Entity* subject = model.find(subjectId);
    const bool function = subject && subject->kind == model::Kind::Function;
    const bool method = subject && (subject->kind == model::Kind::Method || subject->kind == model::Kind::Operation || function);
    const model::Entity* cls = method ? model.find(subject->scope) : subject;
    if (modules_) {
      // lifted to modules: the module each class lives in, and the one a function runs in
      if (!cls) continue;
      out.insert(function ? cls->id : cls->scope);
      for (const model::Entity* m : messagesOf(model, b.sequence->id)) {
        const model::Entity* type = model.find(fieldOf(*m, "type"));
        if (type) out.insert(type->kind == model::Kind::Module ? type->id : type->scope);
      }
      continue;
    }
    if (cls) out.insert(cls->id);
    std::set<std::string> seen;
    walk(model, *b.sequence, b.path, depth_, seen, out);
  }
  std::set<std::string> known;
  for (const auto& id : out)
    if (model.find(id)) known.insert(id);
  return known;
}

// ---- state diagrams --------------------------------------------------------------------------

StateFilter::StateFilter(std::vector<std::string> include) : include_(std::move(include)) {}

std::vector<const model::Entity*> StateFilter::machines(const model::Model& model) const {
  std::vector<const model::Entity*> out;
  for (const auto& pattern : include_) {
    const model::Entity* state = model.find(model::stateId(pattern));
    if (!state) state = model.find(pattern);
    if (state && state->kind == model::Kind::State) out.push_back(state);
  }
  return out;
}

std::set<std::string> StateFilter::select(const model::Model& model) const {
  std::set<std::string> out;
  for (const model::Entity* machine : machines(model)) {
    out.insert(machine->id);
    for (const auto& pair : model.entities())  // the states the design describes
      if (pair.second.scope == machine->id) out.insert(pair.first);
  }
  return out;
}

// ---- requirements diagrams -------------------------------------------------------------------

RequirementsFilter::RequirementsFilter(std::vector<std::string> include, bool design, bool verifications)
    : include_(std::move(include)), design_(design), verifications_(verifications) {}

std::set<std::string> RequirementsFilter::select(const model::Model& model) const {
  // the roots: artefacts named, or every artefact of a package / module
  std::set<std::string> roots;
  for (const auto& pattern : include_) {
    for (const auto& pair : model.entities()) {
      const model::Entity& e = pair.second;
      if (!isArtefact(e) || e.expected) continue;
      bool hit = pattern == e.id;
      if (!pattern.empty() && pattern.back() == '*') {
        // iris@* every artefact of the package (their scope starts with iris@), iris@projection::* those of a module
        hit |= e.scope.rfind(pattern.substr(0, pattern.size() - 1), 0) == 0;
      } else if (const model::Entity* m = model.find(pattern)) {
        hit |= m->kind == model::Kind::Module && (e.scope == pattern || e.scope.rfind(pattern + "::", 0) == 0);
      }
      if (hit) roots.insert(e.id);
    }
  }
  // the umbrella: what refines a root (down, transitively), what verifies it, what realises it
  std::set<std::string> selected = roots;
  bool grew = true;
  while (grew) {
    grew = false;
    for (const auto& r : model.relations()) {
      if ((r.kind == "refines" && selected.count(r.to) && !selected.count(r.from)) ||
          (verifications_ && r.kind == "verifies" && selected.count(r.to) && !selected.count(r.from)) ||
          (design_ && (r.kind == "requires" || r.kind == "constrained-by") && selected.count(r.to) && !selected.count(r.from))) {
        const model::Entity* from = model.find(r.from);
        if (!from || from->expected) continue;
        if (from->kind != model::Kind::Class && !isArtefact(*from)) continue;
        selected.insert(r.from);
        grew = true;
      }
    }
  }
  return selected;
}

// ---- the filter of each kind of diagram ------------------------------------------------------

std::unique_ptr<IFilter> filterFor(const model::Entity& diagram) {
  if (diagram.kind != model::Kind::Diagram) return nullptr;
  const std::string kind = fieldOf(diagram, "kind");
  const std::vector<std::string> include = split(fieldOf(diagram, "include"), ',');
  if (kind == "sequence")
    return std::unique_ptr<IFilter>(
        new SequenceFilter(include, number(fieldOf(diagram, "depth"), 1000), fieldOf(diagram, "level") == "module"));
  if (kind == "state") return std::unique_ptr<IFilter>(new StateFilter(include));
  if (kind == "requirement") {
    const std::vector<std::string> shown = split(fieldOf(diagram, "show"), ',');
    const std::set<std::string> show(shown.begin(), shown.end());
    return std::unique_ptr<IFilter>(
        new RequirementsFilter(include, show.empty() || show.count("design"), show.empty() || show.count("verifications")));
  }
  return std::unique_ptr<IFilter>(new ClassFilter(include, split(fieldOf(diagram, "exclude"), ','),
                                                  number(fieldOf(diagram, "neighbours"), 0), diagram.fields.count("open") > 0,
                                                  split(fieldOf(diagram, "open"), ',')));
}

}  // namespace query
}  // namespace ds
