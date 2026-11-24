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

std::string StructureCheck::name() const { return "structure"; }

std::vector<std::string> StructureCheck::needs() const {
  // the finished model: every field and relation the enriching steps write
  return {"linker", "behaviour", "derivation"};
}

StructureCheck::StructureCheck(const Context& context)
    : context_(&context), model_(&context.model()), metamodel_(&context.metamodel()), resolver_(&context.resolver()) {}

std::vector<model::Finding> StructureCheck::check() {
  found_.clear();
  checkOwnershipCycles();
  checkCycles("refines", model::Code::E_REFINES_CYCLE, "refines");
  checkCycles("inherits", model::Code::E_INHERITANCE_CYCLE, "derives from");
  checkRefinesUpward();
  checkSatisfiesHome();
  checkArtefacts();
  checkExtensions();
  checkFields();
  std::vector<model::Finding> out;
  out.swap(found_);
  return out;
}

// A owns B and B owns A: nobody can create either (R16). Self-composition (a tree) is fine.
void StructureCheck::checkOwnershipCycles() {
  struct Edge {
    std::string to;
    model::Location location;
  };
  std::map<std::string, std::vector<Edge>> owns;
  for (const auto& r : model_->index().ofKind("owns")) {
    const model::Entity* attribute = model_->find(r.from);
    const model::Entity* part = model_->find(r.to);
    if (!attribute || !part || part->expected) continue;
    const std::string whole = attribute->scope;
    if (whole.empty() || whole == r.to) continue;
    owns[whole].push_back({r.to, r.location});
  }
  std::set<std::string> reportedCycles;
  std::vector<std::string> path;
  std::set<std::string> onPath;
  std::set<std::string> done;
  std::function<void(const std::string&)> walk = [&](const std::string& node) {
    path.push_back(node);
    onPath.insert(node);
    for (const auto& edge : owns[node]) {
      if (onPath.count(edge.to)) {
        std::vector<std::string> cycle(std::find(path.begin(), path.end(), edge.to), path.end());
        std::vector<std::string> key = cycle;
        std::sort(key.begin(), key.end());
        if (reportedCycles.insert(join(key, "|")).second) {
          found_.push_back(context_->metamodel().finding(
              model::Code::E_OWNERSHIP_CYCLE, "composition cycle: " + join(cycle, " owns ") + " owns " + edge.to, edge.location));
        }
      } else if (!done.count(edge.to)) {
        walk(edge.to);
      }
    }
    onPath.erase(node);
    path.pop_back();
    done.insert(node);
  };
  for (const auto& pair : owns) {
    if (!done.count(pair.first)) walk(pair.first);
  }
}

// A requirement that refines itself through others, a class that derives from itself: a chain of the relation
// that comes back to where it starts is a contradiction between written things (DS-214). Reported once per cycle.
void StructureCheck::checkCycles(const std::string& relation, model::Code code, const std::string& verb) {
  std::map<std::string, std::vector<std::pair<std::string, model::Location>>> next;
  for (const auto& r : model_->index().ofKind(relation)) {
    const model::Entity* to = model_->find(r.to);
    if (!to || to->expected || r.from == r.to) continue;
    next[r.from].push_back({r.to, r.location});
  }
  std::set<std::string> reported, done, onPath;
  std::vector<std::string> path;
  std::function<void(const std::string&)> walk = [&](const std::string& node) {
    path.push_back(node);
    onPath.insert(node);
    for (const auto& edge : next[node]) {
      if (onPath.count(edge.first)) {
        std::vector<std::string> cycle(std::find(path.begin(), path.end(), edge.first), path.end());
        std::vector<std::string> key = cycle;
        std::sort(key.begin(), key.end());
        if (reported.insert(join(key, "|")).second)
          found_.push_back(context_->metamodel().finding(code, "a cycle: " + join(cycle, " " + verb + " ") + " " + verb + " " + edge.first,
                                                         edge.second));
      } else if (!done.count(edge.first)) {
        walk(edge.first);
      }
    }
    onPath.erase(node);
    path.pop_back();
    done.insert(node);
  };
  for (const auto& pair : next)
    if (!done.count(pair.first)) walk(pair.first);
}

// A requirement refines one written further down the containment axis: the box of the refined requirement lies
// inside the box of the one that refines it, the detail above what it details (DS-214).
void StructureCheck::checkRefinesUpward() {
  const auto inside = [](const std::string& inner, const std::string& outer) {
    if (inner.size() <= outer.size() || inner.compare(0, outer.size(), outer) != 0) return false;
    const char after = inner[outer.size()];
    return outer.back() == '@' || after == '@' || after == ':';
  };
  for (const auto& r : model_->index().ofKind("refines")) {
    const model::Entity* child = model_->find(r.from);
    const model::Entity* parent = model_->find(r.to);
    if (!child || !parent || parent->expected || child->scope.empty() || parent->scope.empty()) continue;
    if (!inside(parent->scope, child->scope)) continue;
    found_.push_back(context_->metamodel().finding(
        model::Code::W_REFINES_UPWARD,
        child->id + " (in " + model::Scope(child->scope).box().text() + ") refines " + parent->id + ", which is written inside it (in " +
            model::Scope(parent->scope).box().text() + "): the detail stands above what it details",
        r.location));
  }
}

// A piece answers to a requirement whose home does not hold it: the requirement is written at one place and
// realised outside it (DDEV-1470). The home is the box the requirement is written in (about); holding is read
// from where each piece is written, outward through its scopes, never from the prefix of an id. A requirement
// written at the root holds everything, what nothing realises is no finding, and a system that adopts the
// requirement of another (no box holds both) answers to it as to any artefact. The finding names the box that
// holds the home and every piece that realises it: where the requirement would stand with all of them inside.
void StructureCheck::checkSatisfiesHome() {
  const auto isPiece = [](model::Kind k) {
    switch (k) {
      case model::Kind::Package:
      case model::Kind::Module:
      case model::Kind::Interface:
      case model::Kind::Operation:
      case model::Kind::Class:
      case model::Kind::Enum:
      case model::Kind::Function:
      case model::Kind::Method:
      case model::Kind::Attribute:
      case model::Kind::Operator:
        return true;
      default:
        return false;
    }
  };
  // the boxes a piece is written in, innermost first, the piece itself first when it is a box
  const auto boxesOf = [this](const model::Entity& e) {
    std::vector<std::string> out{e.id};
    for (model::Scope s(e.scope); !s.empty(); s = s.outer()) {
      const std::string box = s.box().text();
      if (out.back() != box) out.push_back(box);
    }
    return out;
  };
  std::map<std::string, std::string> homes;
  for (const auto& r : model_->index().ofKind("about")) {
    const model::Entity* artefact = model_->find(r.from);
    if (artefact && artefact->kind == model::Kind::Requirement) homes.emplace(r.from, r.to);
  }
  struct Outside {
    const model::Entity* piece;
    model::Location location;
  };
  std::map<std::string, std::vector<Outside>> outside;
  std::map<std::string, std::vector<std::vector<std::string>>> realisers;
  for (const auto& r : model_->index().ofKind("satisfies")) {
    const auto home = homes.find(r.to);
    const model::Entity* piece = model_->find(r.from);
    if (home == homes.end() || !piece || piece->expected || !isPiece(piece->kind)) continue;
    const auto boxes = boxesOf(*piece);
    realisers[r.to].push_back(boxes);
    if (std::find(boxes.begin(), boxes.end(), home->second) == boxes.end()) outside[r.to].push_back({piece, r.location});
  }
  for (const auto& pair : outside) {
    const std::string& home = homes.at(pair.first);
    // the innermost box around the home that holds every piece that realises the requirement
    std::string common;
    const model::Entity* homeEntity = model_->find(home);
    const std::vector<std::string> around = homeEntity ? boxesOf(*homeEntity) : std::vector<std::string>{home};
    for (const auto& box : around) {
      const auto& all = realisers.at(pair.first);
      if (std::all_of(all.begin(), all.end(),
                      [&](const std::vector<std::string>& b) { return std::find(b.begin(), b.end(), box) != b.end(); })) {
        common = box;
        break;
      }
    }
    // no box holds them all: the pieces lie in another system, which adopts the requirement as any artefact
    if (common.empty()) continue;
    const std::string where = common + " holds it and every piece that realises it";
    for (const auto& o : pair.second)
      found_.push_back(
          context_->metamodel().finding(model::Code::W_SATISFIES_OUTSIDE_HOME,
                                        o.piece->id + " satisfies " + pair.first + ", whose home " + home +
                                            " does not hold it: the requirement is written at one place and realised outside it; " + where,
                                        o.location));
  }
}

void StructureCheck::checkArtefacts() {
  std::set<std::string> verified;
  for (const auto& r : model_->index().ofKind("verifies")) verified.insert(r.to);
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.expected) continue;
    const auto status = e.fields.find("status");
    if (e.kind == model::Kind::Requirement && status != e.fields.end() && status->second == "approved" && !verified.count(e.id)) {
      found_.push_back(
          context_->metamodel().finding(model::Code::W_REQ_NO_VERIFICATION, e.id + " is approved and nothing verifies it", e.location));
    }
    const auto method = e.fields.find("method");
    if (e.kind == model::Kind::Verification && method != e.fields.end() && method->second == "test") {
      const auto tests = e.fields.find("tests");
      if (tests == e.fields.end() || tests->second.empty())
        found_.push_back(
            context_->metamodel().finding(model::Code::W_VER_NO_TEST, e.id + " verifies by test but names no tests", e.location));
    }
  }
}

// approved requirements are verified; a test verification names its tests
void StructureCheck::checkExtensions() {
  std::set<std::string> said;  // the linker may have said it already; a check only reads what it said
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    if (e.expected && e.has("extension") && !context_->reported().count(e.id) && said.insert(e.id).second) {
      found_.push_back(context_->metamodel().finding(
          model::Code::W_REF_UNDECLARED,
          metamodel_->nameOf(e.kind) + " '" + e.id + "' is extended with annotations but defined nowhere in the workspace", e.location));
    }
  }
}

// a field its kind does not declare in the metamodel is a typo or a field the language does not know
void StructureCheck::checkFields() {
  for (const auto& pair : model_->entities()) {
    const model::Entity& e = pair.second;
    for (const auto& f : e.fields) {
      if (const model::FieldSpec* spec = metamodel_->fieldOf(e.kind, f.first)) {
        // a closed field takes one of its values; an empty one is left to the field's own check (DS-211)
        if (!spec->values.empty() && !f.second.empty() &&
            std::find(spec->values.begin(), spec->values.end(), f.second) == spec->values.end())
          found_.push_back(context_->metamodel().finding(model::Code::W_FIELD_VALUE,
                                                         metamodel_->nameOf(e.kind) + " '" + e.id + "' has " + f.first + ": " + f.second +
                                                             ", which is none of " + join(spec->values, ", "),
                                                         e.location));
        continue;
      }
      found_.push_back(context_->metamodel().finding(model::Code::W_FIELD_UNKNOWN,
                                                     metamodel_->nameOf(e.kind) + " '" + e.id + "' has a field '" + f.first + "' that a " +
                                                         metamodel_->nameOf(e.kind) + " does not declare",
                                                     e.location));
    }
  }
}

}  // namespace check
}  // namespace ds
