#include "ds/model/Model.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <utility>

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace model {

bool Entity::has(const std::string& flag) const {
  for (const auto& f : flags) {
    if (f == flag) return true;
  }
  return false;
}

const std::string& Entity::field(const std::string& key) const {
  static const std::string kNone;  // no copy for a field read, written or not (DS-345)
  const auto found = fields.find(key);
  return found == fields.end() ? kNone : found->second;
}

bool Entity::number(const std::string& key, int& out_number) const {
  const std::string& written = field(key);
  if (written.empty() || written.find_first_not_of("0123456789") != std::string::npos) return false;
  int read = 0;  // from_chars says when the digits overflow an int, where std::stoi threw (DS-337)
  if (std::from_chars(written.data(), written.data() + written.size(), read).ec != std::errc()) return false;
  out_number = read;
  return true;
}

// A member written with a behaviour's name takes the identity of the behaviour when the behaviour exists (DS-222).
bool Model::behaviour(Kind kind) { return kind == Kind::Sequence || kind == Kind::State; }

const Entity& Model::declare(const Entity& entity) {
  auto it = entities_.find(entity.id);
  if (it == entities_.end()) {
    indexEntity(entity, true);
    return entities_.emplace(entity.id, entity).first->second;
  }
  Entity& existing = it->second;
  if (existing.expected && !entity.expected) {
    // a declaration arrives for something referenced earlier (R6)
    indexEntity(existing, false);
    existing = entity;
    indexEntity(existing, true);
    return existing;
  }
  if (entity.expected) {
    return existing;  // a second reference to the same expected node
  }
  // a behaviour and a member named after it: said once, on the member, and the behaviour keeps the identity, so
  // what is written about it (its jumps, its diagram) is still read as it was meant (DS-222)
  if (behaviour(entity.kind) != behaviour(existing.kind)) {
    const Entity& member = behaviour(entity.kind) ? existing : entity;
    report(metamodel_->finding(Code::E_NAME_RESERVED,
                               metamodel_->nameOf(member.kind) + " '" + member.id + "' takes the identity of the " +
                                   ((behaviour(entity.kind) ? entity.kind : existing.kind) == Kind::State ? "state machine" : "sequence") +
                                   " written for what holds it: rename the " + metamodel_->nameOf(member.kind),
                               member.location));
    if (behaviour(entity.kind)) {
      indexEntity(existing, false);
      existing = entity;
      indexEntity(existing, true);
    }
    return existing;
  }
  const bool reopenable = entity.kind == Kind::Package || entity.kind == Kind::Module;
  if (!reopenable) reportDuplicate(entity, entity.id, existing);
  return existing;
}

void Model::reportDuplicate(const Entity& entity, const std::string& id, const Entity& first) {
  report(metamodel_->finding(Code::E_DUPLICATE_IDENTITY,
                             metamodel_->nameOf(entity.kind) + " '" + id + "' is declared twice (first at " + first.location.file + ":" +
                                 std::to_string(first.location.line) + ")",
                             entity.location));
}

const Entity* Model::find(const std::string& id) const {
  auto it = entities_.find(id);
  return it == entities_.end() ? nullptr : &it->second;
}

Entity& Model::held(const std::string& id) {
  auto it = entities_.find(id);
  // a writer names what it declared or found: an identity the model does not keep is a defect of ds
  if (it == entities_.end()) throw std::logic_error("the model keeps no entity '" + id + "' to change");
  return it->second;
}

void Model::annotate(const std::string& id, const std::string& key, const std::string& value) { held(id).fields[key] = value; }

void Model::refine(const Entity& later) {
  Entity& kept = held(later.id);
  for (const auto& f : later.fields) kept.fields[f.first] = f.second;
  kept.flags.insert(kept.flags.end(), later.flags.begin(), later.flags.end());
  if (!later.params.empty()) kept.params = later.params;
  kept.location = later.location;
}

void Model::relate(const Relation& relation) {
  // a relation the metamodel does not declare is a defect of ds, not of the specification read
  if (!metamodel_->relationOf(relation.kind)) throw std::logic_error("the metamodel does not declare the relation '" + relation.kind + "'");
  relations_.push_back(relation);
  const std::size_t position = relations_.size() - 1;
  indexed_.from[relation.from].push_back(position);
  indexed_.to[relation.to].push_back(position);
  indexed_.kind[relation.kind].push_back(position);
  if (relation.kind == "contains") indexed_.treeStale = true;
}

void Model::indexEntity(const Entity& entity, bool enters) {
  indexed_.treeStale = true;
  auto& members = indexed_.members[entity.scope];
  auto& revisions = indexed_.revisions[model::ArtefactName(entity.id).withoutRevision().text()];
  if (enters) {
    members.insert(entity.id);
    if (!entity.expected) revisions.insert(entity.id);
  } else {
    members.erase(entity.id);
    revisions.erase(entity.id);
  }
  // a bare name the resolver finds by package needs no pass over the model (DS-336)
  if (entity.kind == Kind::Class || entity.kind == Kind::Enum) {
    auto& named = indexed_.named[{Scope(entity.scope).package().text(), entity.name}];
    if (enters && !entity.expected) named.insert(entity.id);
    if (!enters) named.erase(entity.id);
  }
}

void Model::indexAgain() {
  indexed_ = Indexed();
  for (const auto& pair : entities_) indexEntity(pair.second, true);
  for (std::size_t position = 0; position < relations_.size(); ++position) {
    const Relation& r = relations_[position];
    indexed_.from[r.from].push_back(position);
    indexed_.to[r.to].push_back(position);
    indexed_.kind[r.kind].push_back(position);
  }
}

bool Model::rename(const std::string& from, const std::string& to) {
  if (from == to) return true;
  auto it = entities_.find(from);
  if (it == entities_.end()) return false;
  if (const Entity* other = find(to); other && !other->expected) {
    reportDuplicate(it->second, to, *other);
    return false;
  }
  entities_.erase(to);  // an expected node gives way to the declaration
  // each entity moves as it is, its node taken out and put back under its new identity: nothing is copied
  auto moved = entities_.extract(it);
  moved.key() = to;
  moved.mapped().id = to;
  entities_.insert(std::move(moved));
  const Identity outer(from);
  // members: everything whose identity or scope begins with the old one
  std::vector<decltype(entities_)::node_type> members;
  for (auto m = entities_.begin(); m != entities_.end();) {
    if (m->first != from && Identity(m->first).within(outer)) {  // a member or a scenario of what is renamed
      auto member = entities_.extract(m++);
      Entity& e = member.mapped();
      e.id = to + member.key().substr(from.size());
      if (e.scope.rfind(from, 0) == 0) e.scope = to + e.scope.substr(from.size());
      member.key() = e.id;
      members.push_back(std::move(member));
    } else {
      if (m->second.scope == from) m->second.scope = to;
      ++m;
    }
  }
  for (auto& member : members) entities_.insert(std::move(member));  // an identity already held keeps its entity
  auto translate = [&](std::string& id) {
    if (id == from)
      id = to;
    else if (Identity(id).within(outer))
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
  // a field that holds an identity moves with it: the metamodel says which (DS-346)
  const std::vector<std::string>& identities = metamodel_->identityFields();
  for (auto& e : entities_) {
    for (const std::string& key : identities) {
      const auto field = e.second.fields.find(key);
      if (field != e.second.fields.end()) translate(field->second);
    }
  }
  indexAgain();  // identities moved: what was indexed by the old ones is built again
  return true;
}

void Model::report(const Finding& finding) { findings_.push_back(finding); }

void Model::cover() {
  // what is not yet written is no finding (Owner, 2026-11-15; DS-216): it is kept apart, as coverage
  std::vector<Finding> findings;
  for (Finding& f : findings_) {
    if (metamodel_->severityOf(f.severity).keptApart) {  // born with the severity the catalogue gives its code (DS-379)
      coverage_.push_back(std::move(f));
    } else {
      findings.push_back(std::move(f));
    }
  }
  findings_.swap(findings);
}

void Model::withdrawEchoes(const std::set<std::string>& broken) {
  if (broken.empty()) return;
  findings_.erase(std::remove_if(findings_.begin(), findings_.end(),
                                 [&](const Finding& f) {
                                   // what says a file is broken stays: it does not parse, or it is not UTF-8 (DS-411)
                                   return f.code != "E-SYNTAX" && f.code != "E-ENCODING" &&
                                          (broken.count(f.location.file) || f.code == "W-REF-UNDECLARED");
                                 }),
                  findings_.end());
}

void Model::sortFindings() {
  // where it is written, then what it says: two findings on one line keep no order of the steps that found them (DS-382)
  const auto before = [](const Finding& a, const Finding& b) {
    if (a.location.file != b.location.file) return a.location.file < b.location.file;
    if (a.location.line != b.location.line) return a.location.line < b.location.line;
    if (a.code != b.code) return a.code < b.code;
    return a.message < b.message;
  };
  std::stable_sort(findings_.begin(), findings_.end(), before);
  std::stable_sort(coverage_.begin(), coverage_.end(), before);
}

}  // namespace model
}  // namespace ds
