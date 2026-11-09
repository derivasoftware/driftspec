#include "ds/model/Model.hpp"

#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace model {

bool isArtefact(Kind kind) {
  return kind == Kind::Requirement || kind == Kind::Verification || kind == Kind::Constraint || kind == Kind::UseCase;
}

const char* kindName(Kind kind) {
  switch (kind) {
    case Kind::Package:
      return "package";
    case Kind::Module:
      return "module";
    case Kind::Requirement:
      return "requirement";
    case Kind::Verification:
      return "verification";
    case Kind::Interface:
      return "interface";
    case Kind::View:
      return "view";
    case Kind::Operation:
      return "operation";
    case Kind::Constraint:
      return "constraint";
    case Kind::Class:
      return "class";
    case Kind::Enum:
      return "enum";
    case Kind::Function:
      return "function";
    case Kind::Method:
      return "method";
    case Kind::Attribute:
      return "attribute";
    case Kind::Operator:
      return "operator";
    case Kind::Sequence:
      return "sequence";
    case Kind::Message:
      return "message";
    case Kind::Frame:
      return "frame";
    case Kind::State:
      return "state";
    case Kind::Diagram:
      return "diagram";
    case Kind::UseCase:
      return "usecase";
    case Kind::Outcome:
      return "outcome";
    case Kind::Rule:
      return "rule";
    case Kind::Definition:
      return "definition";
  }
  return "?";
}

bool Entity::has(const std::string& flag) const {
  for (const auto& f : flags) {
    if (f == flag) return true;
  }
  return false;
}

Entity& Model::declare(const Entity& entity) {
  auto it = entities_.find(entity.id);
  if (it == entities_.end()) {
    return entities_.emplace(entity.id, entity).first->second;
  }
  Entity& existing = it->second;
  if (existing.expected && !entity.expected) {
    // a declaration arrives for something referenced earlier (R6)
    existing = entity;
    return existing;
  }
  if (entity.expected) {
    return existing;  // a second reference to the same expected node
  }
  const bool reopenable = entity.kind == Kind::Package || entity.kind == Kind::Module;
  if (!reopenable) {
    report({"E-DUPLICATE-IDENTITY", "error",
            std::string(kindName(entity.kind)) + " '" + entity.id + "' is declared twice (first at " + existing.location.file + ":" +
                std::to_string(existing.location.line) + ")",
            entity.location});
  }
  return existing;
}

const Entity* Model::find(const std::string& id) const {
  auto it = entities_.find(id);
  return it == entities_.end() ? nullptr : &it->second;
}

Entity* Model::find(const std::string& id) {
  auto it = entities_.find(id);
  return it == entities_.end() ? nullptr : &it->second;
}

void Model::relate(const Relation& relation) {
  // a relation the metamodel does not declare is a defect of ds, not of the specification read
  if (!relationOf(relation.kind)) throw std::logic_error("the metamodel does not declare the relation '" + relation.kind + "'");
  relations_.push_back(relation);
}

bool Model::rename(const std::string& from, const std::string& to) {
  if (from == to) return true;
  auto it = entities_.find(from);
  if (it == entities_.end()) return false;
  if (const Entity* other = find(to); other && !other->expected) {
    report({"E-DUPLICATE-IDENTITY", "error",
            std::string(kindName(it->second.kind)) + " '" + to + "' is declared twice (first at " + other->location.file + ":" +
                std::to_string(other->location.line) + ")",
            it->second.location});
    return false;
  }
  entities_.erase(to);  // an expected node gives way to the declaration
  Entity moved = it->second;
  moved.id = to;
  entities_.erase(it);
  entities_.emplace(to, moved);
  // members: everything whose identity or scope begins with the old one
  std::vector<std::pair<std::string, Entity>> members;
  for (auto m = entities_.begin(); m != entities_.end();) {
    if (m->first.rfind(from + "::", 0) == 0 || m->first.rfind(from + "/", 0) == 0) {
      Entity e = m->second;
      e.id = to + m->first.substr(from.size());
      if (e.scope.rfind(from, 0) == 0) e.scope = to + e.scope.substr(from.size());
      members.push_back({e.id, e});
      m = entities_.erase(m);
    } else {
      if (m->second.scope == from) m->second.scope = to;
      ++m;
    }
  }
  for (auto& m : members) entities_.emplace(m.first, m.second);
  auto translate = [&](std::string& id) {
    if (id == from)
      id = to;
    else if (id.rfind(from + "::", 0) == 0 || id.rfind(from + "/", 0) == 0)
      id = to + id.substr(from.size());
  };
  // what a rule of a renamed class says moves with it
  std::map<std::string, Formula> formulas;
  for (auto& f : formulas_) {
    std::string id = f.first;
    translate(id);
    formulas.emplace(id, f.second);
  }
  formulas_.swap(formulas);
  for (Relation& r : relations_) {
    translate(r.from);
    translate(r.to);
  }
  for (auto& e : entities_) {
    for (auto& f : e.second.fields) {
      if (f.first == "expands" || f.first == "subject-of") translate(f.second);
    }
  }
  return true;
}

void Model::report(const Finding& finding) { findings_.push_back(finding); }

}  // namespace model
}  // namespace ds

namespace ds {
namespace model {

bool isSystemScope(const std::string& scope) { return !scope.empty() && scope.back() == '@' && scope.find('@') == scope.size() - 1; }

std::string lastName(const std::string& id) {
  const auto cut = id.find_last_of(":@");
  return cut == std::string::npos ? id : id.substr(cut + 1);
}

namespace {

int orderOf(const Model& model, const std::string& messageId) {
  const Entity* m = messageId.empty() ? nullptr : model.find(messageId);
  const auto order = m ? m->fields.find("order") : std::map<std::string, std::string>::const_iterator();
  return m && order != m->fields.end() ? std::stoi(order->second) : -1;
}

std::string fieldIn(const Entity& e, const std::string& key) {
  const auto f = e.fields.find(key);
  return f == e.fields.end() ? "" : f->second;
}

/// The frames of one behaviour, and for each what its two sides name.
struct Branches {
  const Model& model;
  std::map<std::string, const Entity*> frames;

  int elseAt(const Entity& f) const {
    const std::string otherwise = fieldIn(f, "else");
    return otherwise.empty() ? std::numeric_limits<int>::max() : orderOf(model, otherwise);
  }

  bool onElse(const Entity& f, int order) const { return order >= elseAt(f); }

  std::set<std::string> names(const Entity& f, bool elseSide) const {
    std::set<std::string> out;
    const std::string own = fieldIn(f, elseSide ? "else-scenario" : "scenario");
    if (!own.empty()) out.insert(own);
    for (const auto& pair : frames) {
      const Entity& child = *pair.second;
      if (fieldIn(child, "parent") != f.id) continue;
      if (onElse(f, orderOf(model, fieldIn(child, "first"))) != elseSide) continue;
      for (const bool side : {false, true}) {
        for (const auto& n : names(child, side)) out.insert(n);
      }
    }
    return out;
  }

  /// Then, Else: the path takes that side, drawn inline. Skip: it does not. Keep: it cannot tell, the frame is
  /// drawn. Enter: a loop the path goes into, drawn as the loop it is.
  enum class Way { Then, Else, Skip, Keep, Enter };

  Way decide(const Entity& f, const std::string& scenario) const {
    const std::set<std::string> thenNames = names(f, false);
    const std::set<std::string> elseNames = names(f, true);
    // a loop is not a choice: every path goes through it, and one that names a branch inside goes into it
    if (fieldIn(f, "kind") == "loop") return !scenario.empty() && thenNames.count(scenario) ? Way::Enter : Way::Keep;
    if (!scenario.empty() && thenNames.count(scenario)) return Way::Then;
    if (!scenario.empty() && elseNames.count(scenario)) return Way::Else;
    const bool twoSides = !fieldIn(f, "else").empty() || !fieldIn(f, "else-scenario").empty();
    if (twoSides) {
      if (!thenNames.empty() && elseNames.empty()) return Way::Else;
      if (thenNames.empty() && !elseNames.empty()) return Way::Then;
      return Way::Keep;
    }
    return thenNames.empty() ? Way::Keep : Way::Skip;  // a branch nobody asked for is not taken
  }
};

}  // namespace

bool hasScenario(const Model& model, const std::string& sequence, const std::string& scenario) {
  for (const auto& pair : model.entities()) {
    const Entity& f = pair.second;
    if (f.kind != Kind::Frame || f.scope != sequence) continue;
    if (fieldIn(f, "scenario") == scenario || fieldIn(f, "else-scenario") == scenario) return true;
  }
  return false;
}

std::vector<const Entity*> pathOf(const Model& model, const std::string& sequence, const std::string& scenario,
                                  std::set<std::string>* kept) {
  Branches branches{model, {}};
  std::vector<const Entity*> messages;
  for (const auto& pair : model.entities()) {
    if (pair.second.scope != sequence) continue;
    if (pair.second.kind == Kind::Frame) branches.frames[pair.first] = &pair.second;
    if (pair.second.kind == Kind::Message) messages.push_back(&pair.second);
  }
  std::sort(messages.begin(), messages.end(),
            [](const Entity* a, const Entity* b) { return std::stoi(fieldIn(*a, "order")) < std::stoi(fieldIn(*b, "order")); });
  std::vector<const Entity*> out;
  for (const Entity* m : messages) {
    const int order = std::stoi(fieldIn(*m, "order"));
    bool taken = true;
    bool certain = true;  // every frame around it is a side the path takes, none it cannot tell
    std::vector<std::string> around;
    for (std::string id = fieldIn(*m, "frame"); !id.empty() && taken;) {
      const auto it = branches.frames.find(id);
      if (it == branches.frames.end()) break;
      const Entity& f = *it->second;
      const Branches::Way way = branches.decide(f, scenario);
      const bool elseSide = branches.onElse(f, order);
      if (way == Branches::Way::Skip || (way == Branches::Way::Then && elseSide) || (way == Branches::Way::Else && !elseSide)) {
        taken = false;
      } else if (way == Branches::Way::Keep) {
        certain = false;
        around.push_back(f.id);
      } else if (way == Branches::Way::Enter) {
        around.push_back(f.id);
      }
      id = fieldIn(f, "parent");
    }
    if (!taken) continue;
    if (kept) kept->insert(around.begin(), around.end());
    out.push_back(m);
    if (certain && fieldIn(*m, "return") == "true") break;  // the path ends here
  }
  return out;
}

}  // namespace model
}  // namespace ds
