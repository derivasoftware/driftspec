#include "ds/query/Query.hpp"

#include <nlohmann/json.hpp>

#include "ds/text/Text.hpp"

namespace ds {
namespace query {

namespace {

Answer entityAnswer(const model::Entity& e, const std::string& detail = "") { return {model::kindName(e.kind), e.id, detail}; }

bool endsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

}  // namespace

std::vector<std::pair<std::string, std::string>> Query::questions() {
  return {{"diagrams", ""},
          {"entities", "<scope>"},
          {"relations", "<identity>"},
          {"artefacts", "<identity>"},
          {"behaviours", "<identity>"},
          {"failures", "<identity>"},
          {"shows", "<diagram>"},
          {"findings", "[file]"},
          {"coverage", "[identity]"},
          {"coupling", ""}};
}

std::vector<Answer> Query::ask(const std::string& question, const std::string& subject) const {
  // a question about an identity needs one the model declares: an empty answer would read as "it has none"
  const bool aboutOne = question == "relations" || question == "artefacts" || question == "behaviours" || question == "failures";
  if (aboutOne && subject.empty()) return {{"error", question, "needs an identity: ds query <workspace> " + question + " <identity>"}};
  if ((aboutOne || (question == "entities" && !subject.empty())) && !known(subject)) {
    const std::vector<std::string> near = nearTo(subject);
    std::string detail = "nothing in the model is named so";
    for (size_t i = 0; i < near.size(); ++i) detail += (i ? ", " : "; did you mean ") + near[i];
    return {{"error", subject, detail}};
  }
  if (question == "diagrams") return diagrams();
  if (question == "entities") return entities(subject);
  if (question == "relations") return relations(subject);
  if (question == "artefacts") return artefacts(subject);
  if (question == "behaviours") return behaviours(subject);
  if (question == "failures") return failures(subject);
  if (question == "shows") return shows(subject);
  if (question == "findings") return findings(subject);
  if (question == "coverage") return coverage(subject);
  if (question == "coupling") return coupling();
  return {{"error", question, "not a question ds answers"}};
}

bool Query::known(const std::string& id) const {
  if (model_.find(id)) return true;
  const std::string scope = model::Scope::inside(model::Identity(id)).text();  // a package is a scope too: driftspec@ds
  const model::Index index = model_.index();
  return !index.members(scope).empty() || !index.members(id).empty();
}

std::vector<std::string> Query::nearTo(const std::string& name) const {
  // identities that end in the name given: Checker is driftspec@ds@check::Checker
  std::vector<std::string> out;
  for (const auto& pair : model_.entities()) {
    const std::string& id = pair.first;
    if (pair.second.expected || id.size() <= name.size()) continue;
    const std::string tail = id.substr(id.size() - name.size());
    const char before = id[id.size() - name.size() - 1];
    if (tail == name && (before == ':' || before == '@')) out.push_back(id);
    if (out.size() == 5) break;
  }
  return out;
}

std::vector<Answer> Query::diagrams() const {
  std::vector<Answer> out;
  for (const auto& p : model_.entities()) {
    if (p.second.kind != model::Kind::Diagram) continue;
    const auto kind = p.second.fields.find("kind");
    const std::string name = model::Identity(p.second.id).diagramName();  // named as the user names it
    out.push_back({model::kindName(p.second.kind), name, kind == p.second.fields.end() ? "class" : kind->second});
  }
  return out;
}

std::vector<Answer> Query::entities(const std::string& asked) const {
  // a package's members carry the scope `pkg@`; the question may name the package plainly
  const model::Entity* pkg = model_.find(asked);
  const std::string scope = pkg && pkg->kind == model::Kind::Package ? asked + "@" : asked;
  std::vector<Answer> out;
  for (const auto& p : model_.entities()) {
    if (p.second.scope != scope || p.second.expected) continue;
    out.push_back(entityAnswer(p.second, p.second.fields.count("title") ? p.second.fields.at("title") : ""));
  }
  return out;
}

std::vector<Answer> Query::relations(const std::string& id) const {
  std::vector<Answer> out;
  for (const auto& r : model_.index().touching(id)) {
    if (r.from == id)
      out.push_back({r.kind, r.to, "->" + (r.multiplicity.empty() ? "" : " " + r.multiplicity)});
    else
      out.push_back({r.kind, r.from, "<-"});
  }
  return out;
}

std::vector<Answer> Query::artefacts(const std::string& id) const {
  // what the entity realises, satisfies or is bound by, then what verifies each of those
  std::vector<Answer> out;
  const model::Index index = model_.index();
  for (const auto& r : index.from(id)) {
    if (r.kind != "realises" && r.kind != "satisfies" && r.kind != "constrained-by") continue;
    out.push_back({r.kind, r.to, ""});
    for (const auto& v : index.to(r.to))
      if (v.kind == "verifies") out.push_back({"verified-by", v.from, r.to});
  }
  return out;
}

std::vector<Answer> Query::behaviours(const std::string& id) const {
  std::vector<Answer> out;
  for (const auto& r : model_.index().to(id)) {
    if (r.kind != "behaviour-of") continue;
    const model::Entity* b = model_.find(r.from);
    out.push_back(b ? entityAnswer(*b, b->fields.count("scenario") ? b->fields.at("scenario") : "") : Answer{"behaviour", r.from, ""});
  }
  const model::Entity* asked = model_.find(id);
  if (!asked || asked->kind != model::Kind::Class) return out;
  for (const auto& r : model_.index().from(id)) {  // a class's behaviours are its machine's and its operations' (DS-53)
    const model::Entity* member = r.kind == "contains" ? model_.find(r.to) : nullptr;
    if (!member || member->kind != model::Kind::Method) continue;
    for (auto& answer : behaviours(member->id)) out.push_back(answer);
  }
  return out;
}

std::vector<Answer> Query::failures(const std::string& id) const {
  // what the check computed (DS-188): each failure escaping `id`, named by the operation it starts in,
  // with the path it comes up by
  std::vector<Answer> out;
  const model::Entity* e = model_.find(id);
  if (!e) return out;
  for (const auto& name : text::splitTrimmed(e->field("escapes"), ',')) {
    const std::string path = e->field("escape." + name);
    const std::string through = path.substr(0, path.find(" from "));
    const auto last = through.rfind(" -> ");
    out.push_back({"failure", last == std::string::npos ? through : through.substr(last + 4), name + ": " + path});
  }
  return out;
}

std::set<std::string> Query::select(const IFilter& filter) const { return filter.select(model_); }

std::vector<Answer> Query::shows(const std::string& diagram) const {
  // what the diagram takes is the filter of its kind, applied to the model: nothing is drawn (DS-127)
  const model::Entity* d = model_.find(model::Identity::ofDiagram(diagram).text());
  const auto filter = d ? filterFor(*d) : nullptr;
  if (!filter) return {{"error", diagram, "no such diagram"}};
  std::vector<Answer> out;
  for (const auto& id : select(*filter)) out.push_back(entityAnswer(*model_.find(id)));
  return out;
}

// derived from behaviour: a local or an attribute typed in another module is coupling to it
std::vector<Answer> Query::coupling() const {
  std::vector<Answer> out;
  for (const auto& r : model_.index().ofKinds({"couples-structural", "couples-transient"})) {
    const model::Entity* seq = model_.find(r.from);
    out.push_back({r.kind, r.to, (seq ? seq->name + " in " + seq->scope : r.from)});
  }
  return out;
}

std::vector<Answer> Query::findings(const std::string& file) const {
  std::vector<Answer> out;
  for (const auto& f : model_.findings()) {
    if (!file.empty() && !endsWith(f.location.file, file)) continue;
    out.push_back({f.code, f.location.file + ":" + std::to_string(f.location.line) + ":" + std::to_string(f.location.column), f.message});
  }
  return out;
}

// What is not yet written (DS-216): every absence the check keeps apart, or those that name an identity.
std::vector<Answer> Query::coverage(const std::string& identity) const {
  std::vector<Answer> out;
  for (const auto& f : model_.coverage()) {
    if (!identity.empty() && f.message.find(identity) == std::string::npos) continue;
    out.push_back({f.code, f.location.file + ":" + std::to_string(f.location.line) + ":" + std::to_string(f.location.column), f.message});
  }
  return out;
}

}  // namespace query
}  // namespace ds
