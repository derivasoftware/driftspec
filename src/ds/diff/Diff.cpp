#include "ds/diff/Diff.hpp"

#include <algorithm>
#include <set>
#include <sstream>

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace diff {

namespace {

/// Fields that describe a declaration: what the check derives is a consequence, reported through what
/// caused it. The metamodel says which fields are derived (DS-114); a field it does not declare was written.
bool declarative(model::Kind kind, const std::string& key) {
  const model::FieldSpec* spec = model::fieldOf(kind, key);
  return !spec || spec->source != model::FieldSource::Derived;
}

std::string joinFlags(const std::vector<std::string>& flags) {
  std::vector<std::string> sorted = flags;
  std::sort(sorted.begin(), sorted.end());
  std::string out;
  for (const auto& f : sorted) out += (out.empty() ? "" : ",") + f;
  return out;
}

std::string relationKey(const model::Relation& r) {
  return r.kind + " " + r.from + " -> " + r.to + (r.multiplicity.empty() ? "" : " [" + r.multiplicity + "]");
}

bool isArtefact(model::Kind k) { return k == model::Kind::Requirement || k == model::Kind::Verification || k == model::Kind::Constraint; }

/// What two declarations of one identity differ in, as "key: a -> b" items.
std::vector<std::string> differences(const model::Entity& a, const model::Entity& b) {
  std::vector<std::string> out;
  std::set<std::string> keys;
  for (const auto& f : a.fields) keys.insert(f.first);
  for (const auto& f : b.fields) keys.insert(f.first);
  for (const auto& key : keys) {
    if (!declarative(a.kind, key)) continue;
    const auto fa = a.fields.find(key), fb = b.fields.find(key);
    const std::string va = fa == a.fields.end() ? "" : fa->second;
    const std::string vb = fb == b.fields.end() ? "" : fb->second;
    if (va != vb) out.push_back(key + ": " + (va.empty() ? "(none)" : va) + " -> " + (vb.empty() ? "(none)" : vb));
  }
  const std::string flagsA = joinFlags(a.flags), flagsB = joinFlags(b.flags);
  if (flagsA != flagsB) out.push_back("flags: " + (flagsA.empty() ? "(none)" : flagsA) + " -> " + (flagsB.empty() ? "(none)" : flagsB));
  if (a.scope != b.scope) out.push_back("scope: " + a.scope + " -> " + b.scope);
  if (a.kind != b.kind) out.push_back(std::string("kind: ") + model::kindName(a.kind) + " -> " + model::kindName(b.kind));
  return out;
}

/// A class's shape: its members' names and signatures, to recognise a rename.
std::set<std::string> shapeOf(const model::Model& m, const std::string& classId) {
  std::set<std::string> out;
  for (const auto& pair : m.entities()) {
    const model::Entity& e = pair.second;
    if (e.scope != classId) continue;
    std::string sig = std::string(model::kindName(e.kind)) + " " + e.name;
    for (const char* key : {"params", "returns", "type"}) {
      const auto f = e.fields.find(key);
      if (f != e.fields.end()) sig += " " + f->second;
    }
    out.insert(sig);
  }
  return out;
}

std::string join(const std::vector<std::string>& items, const char* sep) {
  std::string out;
  for (const auto& i : items) out += (out.empty() ? "" : sep) + i;
  return out;
}

}  // namespace

int DiffResult::added() const {
  int n = 0;
  for (const auto& c : changes) n += c.kind == "added";
  return n;
}
int DiffResult::removed() const {
  int n = 0;
  for (const auto& c : changes) n += c.kind == "removed";
  return n;
}
int DiffResult::changed() const {
  int n = 0;
  for (const auto& c : changes) n += c.kind == "changed" || c.kind == "renamed";
  return n;
}

DiffResult Diff::compare(const model::Model& a, const model::Model& b) {
  DiffResult out;
  std::map<std::string, std::string> renamed;  // old id -> new id
  // entities: by identity; expected nodes are not declarations
  std::vector<const model::Entity*> removedClasses, addedClasses;
  for (const auto& pair : a.entities()) {
    const model::Entity& ea = pair.second;
    if (ea.expected) continue;
    const model::Entity* eb = b.find(ea.id);
    if (!eb || eb->expected) {
      if (ea.kind == model::Kind::Class) removedClasses.push_back(&ea);
      continue;
    }
    const auto diffs = differences(ea, *eb);
    if (diffs.empty()) continue;
    Change change{"changed", "entity", ea.id, model::kindName(ea.kind), ea.scope, join(diffs, "; "), "", ""};
    if (ea.kind == model::Kind::Message) change.label = ea.scope + " : " + ea.name;
    out.changes.push_back(change);
    if (isArtefact(ea.kind)) {
      // the same id with other text: the revision should have moved
      out.findings.push_back(
          {"W-REVISION-STALE", "warning",
           std::string(model::kindName(ea.kind)) + " " + ea.id + " changed (" + join(diffs, "; ") + ") but keeps its revision",
           eb->location});
    }
  }
  for (const auto& pair : b.entities()) {
    const model::Entity& eb = pair.second;
    if (eb.expected) continue;
    const model::Entity* ea = a.find(eb.id);
    if (!ea || ea->expected) {
      if (eb.kind == model::Kind::Class) addedClasses.push_back(&eb);
    }
  }
  // renames: a removed class and an added one in the same scope with most members alike
  std::set<std::string> matched;
  // who points at a class: the members (not containment) that reference it
  auto referrers = [](const model::Model& m, const std::string& id) {
    std::set<std::string> out;
    for (const auto& r : m.relations()) {
      if (r.to == id && r.kind != "contains") out.insert(r.from);
    }
    return out;
  };
  for (const model::Entity* r : removedClasses) {
    const std::set<std::string> shape = shapeOf(a, r->id);
    const std::set<std::string> pointedAt = referrers(a, r->id);
    const model::Entity* best = nullptr;
    size_t bestScore = 0;
    for (const model::Entity* ad : addedClasses) {
      if (ad->scope != r->scope || matched.count(ad->id)) continue;
      const std::set<std::string> other = shapeOf(b, ad->id);
      size_t common = 0;
      for (const auto& s : shape) common += other.count(s);
      const size_t total = std::max(shape.size(), other.size());
      if (total == 0 || common * 10 < total * 6) continue;  // at least 60 % of the members alike
      // the referrers that now point at the candidate break the tie between look-alikes
      size_t followed = 0;
      for (const auto& from : referrers(b, ad->id)) followed += pointedAt.count(from);
      const size_t score = common * 10 + followed * 100 + 1;
      if (score > bestScore) {
        best = ad;
        bestScore = score;
      }
    }
    if (best) {
      matched.insert(best->id);
      renamed[r->id] = best->id;
      out.changes.push_back(
          {"renamed", "entity", best->id, model::kindName(best->kind), best->scope, r->name + " -> " + best->name, r->id, ""});
    }
  }
  // the members of a renamed class are compared under their new name
  for (const auto& rn : renamed) {
    for (const auto& pair : a.entities()) {
      const model::Entity& ea = pair.second;
      if (ea.scope != rn.first) continue;
      const std::string newId = rn.second + ea.id.substr(rn.first.size());
      const model::Entity* eb = b.find(newId);
      if (!eb) {
        out.changes.push_back({"removed", "entity", ea.id, model::kindName(ea.kind), rn.second, "", "", ""});
        continue;
      }
      model::Entity moved = ea;  // under its new name the scope is not a change
      moved.scope = eb->scope;
      const auto diffs = differences(moved, *eb);
      if (!diffs.empty())
        out.changes.push_back({"changed", "entity", newId, model::kindName(ea.kind), rn.second, join(diffs, "; "), "", ""});
    }
    for (const auto& pair : b.entities()) {
      const model::Entity& eb = pair.second;
      if (eb.scope != rn.second) continue;
      const std::string oldId = rn.first + eb.id.substr(rn.second.size());
      if (!a.find(oldId)) out.changes.push_back({"added", "entity", eb.id, model::kindName(eb.kind), rn.second, "", "", ""});
    }
  }
  for (const auto& pair : a.entities()) {
    const model::Entity& ea = pair.second;
    if (ea.expected || renamed.count(ea.id)) continue;
    const model::Entity* eb = b.find(ea.id);
    if (eb && !eb->expected) continue;
    // members of a renamed class are not removals
    bool insideRenamed = false;
    for (const auto& r : renamed) insideRenamed |= ea.scope == r.first || ea.id.rfind(r.first + "::", 0) == 0;
    if (insideRenamed) continue;
    out.changes.push_back({"removed", "entity", ea.id, model::kindName(ea.kind), ea.scope, "", "", ""});
  }
  for (const auto& pair : b.entities()) {
    const model::Entity& eb = pair.second;
    if (eb.expected || matched.count(eb.id)) continue;
    const model::Entity* ea = a.find(eb.id);
    if (ea && !ea->expected) continue;
    bool insideRenamed = false;
    for (const auto& r : renamed) insideRenamed |= eb.scope == r.second || eb.id.rfind(r.second + "::", 0) == 0;
    if (insideRenamed) continue;
    out.changes.push_back({"added", "entity", eb.id, model::kindName(eb.kind), eb.scope, "", "", ""});
  }
  // relations: by their key, with renamed ids translated; containment is implied by the entities
  auto translate = [&](std::string id) {
    for (const auto& r : renamed) {
      if (id == r.first) return r.second;
      if (id.rfind(r.first + "::", 0) == 0) return r.second + id.substr(r.first.size());
    }
    return id;
  };
  std::set<std::string> relA, relB;
  std::map<std::string, const model::Relation*> byKey;
  for (const auto& r : a.relations()) {
    if (r.kind == "contains") continue;
    model::Relation t = r;
    t.from = translate(r.from);
    t.to = translate(r.to);
    relA.insert(relationKey(t));
  }
  for (const auto& r : b.relations()) {
    if (r.kind == "contains") continue;
    relB.insert(relationKey(r));
    byKey[relationKey(r)] = &r;
  }
  std::map<std::string, std::string> scopeA;
  for (const auto& r : a.relations()) {
    const model::Entity* from = a.find(r.from);
    model::Relation t = r;
    t.from = translate(r.from);
    t.to = translate(r.to);
    if (from) scopeA[relationKey(t)] = translate(from->scope);
  }
  for (const auto& k : relA) {
    if (!relB.count(k)) out.changes.push_back({"removed", "relation", k, "relation", scopeA[k], "", "", ""});
  }
  for (const auto& k : relB) {
    if (!relA.count(k)) {
      const model::Relation* r = byKey[k];
      const model::Entity* from = b.find(r->from);
      out.changes.push_back({"added", "relation", k, "relation", from ? from->scope : "", "", "", ""});
    }
  }
  return out;
}

model::Model Diff::marked(const model::Model& a, const model::Model& b, const DiffResult& diff) {
  model::Model out = b;
  std::map<std::string, const Change*> byId;
  for (const auto& c : diff.changes) {
    if (c.what == "entity") byId[c.id] = &c;
  }
  for (const auto& pair : b.entities()) {
    const auto c = byId.find(pair.first);
    if (c == byId.end()) continue;
    model::Entity* e = out.find(pair.first);
    e->fields["change"] = c->second->kind;
    if (!c->second->detail.empty()) e->fields["change-detail"] = c->second->detail;
    if (!c->second->from.empty()) e->fields["change-from"] = c->second->from;
  }
  // what was removed comes over, marked, so the drawing can show it going
  for (const auto& c : diff.changes) {
    if (c.what != "entity" || c.kind != "removed") continue;
    const model::Entity* ea = a.find(c.id);
    if (!ea) continue;
    model::Entity ghost = *ea;
    ghost.fields["change"] = "removed";
    out.declare(ghost);
  }
  std::set<std::string> removedRelations, addedRelations;
  for (const auto& c : diff.changes) {
    if (c.what != "relation") continue;
    (c.kind == "removed" ? removedRelations : addedRelations).insert(c.id);
  }
  std::vector<model::Relation> relations = out.relations();
  // relations are rebuilt with their change marked, for a drawing that shows it
  model::Model rebuilt;
  for (const auto& pair : out.entities()) rebuilt.declare(pair.second);
  for (const auto& r : out.relations()) {
    model::Relation m = r;
    if (addedRelations.count(relationKey(r))) m.change = "added";
    rebuilt.relate(m);
  }
  for (const auto& r : a.relations()) {
    if (removedRelations.count(relationKey(r))) {
      model::Relation m = r;
      m.change = "removed";
      rebuilt.relate(m);
    }
  }
  for (const auto& f : out.findings()) rebuilt.report(f);
  return rebuilt;
}

std::string Diff::report(const DiffResult& diff) {
  std::ostringstream out;
  if (diff.empty()) {
    out << "no design change\n";
    return out.str();
  }
  std::map<std::string, std::vector<const Change*>> byScope;
  for (const auto& c : diff.changes) byScope[c.scope.empty() ? "(workspace)" : c.scope].push_back(&c);
  for (const auto& group : byScope) {
    out << group.first << "\n";
    for (const Change* c : group.second) {
      const char* mark = c->kind == "added" ? "+" : c->kind == "removed" ? "-" : c->kind == "renamed" ? ">" : "~";
      out << "  " << mark << " " << c->entity << " " << (c->label.empty() ? c->id : c->label);
      if (!c->detail.empty()) out << "   (" << c->detail << ")";
      out << "\n";
    }
  }
  out << diff.added() << " added · " << diff.removed() << " removed · " << diff.changed() << " changed\n";
  for (const auto& f : diff.findings) out << f.location.file << ":" << f.location.line << "  " << f.code << "  " << f.message << "\n";
  return out.str();
}

}  // namespace diff
}  // namespace ds
