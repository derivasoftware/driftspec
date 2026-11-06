#include "ds/query/Query.hpp"

#include <nlohmann/json.hpp>

#include "ds/render/Ir.hpp"
#include "ds/render/IrExport.hpp"

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
          {"shows", "<diagram>"},
          {"findings", "[file]"},
          {"coupling", ""}};
}

std::vector<Answer> Query::ask(const std::string& question, const std::string& subject) const {
  // a question about an identity needs one the model declares: an empty answer would read as "it has none"
  const bool aboutOne = question == "relations" || question == "artefacts" || question == "behaviours";
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
  if (question == "shows") return shows(subject);
  if (question == "findings") return findings(subject);
  if (question == "coupling") return coupling();
  return {{"error", question, "not a question ds answers"}};
}

bool Query::known(const std::string& id) const {
  if (model_.find(id)) return true;
  const std::string scope = id.back() == '@' ? id : id + "@";  // a package is a scope too: driftspec@ds
  for (const auto& pair : model_.entities())
    if (pair.second.scope == scope || pair.second.scope == id) return true;
  return false;
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
    const std::string name = p.second.id.rfind("diagram:", 0) == 0 ? p.second.id.substr(8) : p.second.id;  // named as the user names it
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
  for (const auto& r : model_.relations()) {
    if (r.from == id)
      out.push_back({r.kind, r.to, "->" + (r.multiplicity.empty() ? "" : " " + r.multiplicity)});
    else if (r.to == id)
      out.push_back({r.kind, r.from, "<-"});
  }
  return out;
}

std::vector<Answer> Query::artefacts(const std::string& id) const {
  // what the entity realises, requires or is bound by, then what verifies each of those
  std::vector<Answer> out;
  for (const auto& r : model_.relations()) {
    if (r.from != id || (r.kind != "realises" && r.kind != "requires" && r.kind != "constrained-by")) continue;
    out.push_back({r.kind, r.to, ""});
    for (const auto& v : model_.relations())
      if (v.kind == "verifies" && v.to == r.to) out.push_back({"verified-by", v.from, r.to});
  }
  return out;
}

std::vector<Answer> Query::behaviours(const std::string& id) const {
  std::vector<Answer> out;
  for (const auto& r : model_.relations()) {
    if (r.kind != "behaviour-of" || r.to != id) continue;
    const model::Entity* b = model_.find(r.from);
    out.push_back(b ? entityAnswer(*b, b->fields.count("scenario") ? b->fields.at("scenario") : "") : Answer{"behaviour", r.from, ""});
  }
  return out;
}

std::vector<Answer> Query::shows(const std::string& diagram) const {
  // what the diagram draws: ask the projection itself, so the answer is what a drawing would show
  std::vector<Answer> out;
  const std::string doc = render::IrExport(model_).render(diagram.rfind("diagram:", 0) == 0 ? diagram : "diagram:" + diagram, false);
  if (doc.empty()) return {{"error", diagram, "no such diagram"}};
  for (const auto& n : render::IrDocument::parse(doc).bundled().nodes) {
    if (n.kind == "note") continue;
    const model::Entity* e = model_.find(n.id);
    out.push_back(e ? entityAnswer(*e) : Answer{n.kind, n.id, ""});
  }
  return out;
}

// derived from behaviour: a local or an attribute typed in another module is coupling to it
std::vector<Answer> Query::coupling() const {
  std::vector<Answer> out;
  for (const auto& r : model_.relations()) {
    if (r.kind != "couples-structural" && r.kind != "couples-transient") continue;
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

std::string Query::text(const std::vector<Answer>& answers) {
  std::string out;
  for (const auto& a : answers) out += a.kind + "  " + a.id + (a.detail.empty() ? "" : "  " + a.detail) + "\n";
  return out;
}

std::string Query::json(const std::vector<Answer>& answers) {
  nlohmann::json out = nlohmann::json::array();
  for (const auto& a : answers) out.push_back({{"kind", a.kind}, {"id", a.id}, {"detail", a.detail}});
  return out.dump();
}

}  // namespace query
}  // namespace ds
