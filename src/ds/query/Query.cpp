#include "ds/query/Query.hpp"

#include <algorithm>

#include "ds/model/Identity.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/model/Walkthroughs.hpp"
#include "ds/query/Pattern.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace query {

std::vector<model::Question> Query::questions() { return model::Manual::questions(); }

std::string Query::usage() { return model::Manual::usage(); }

std::vector<Answer> Query::ask(const std::string& question, const std::string& subject) const {
  const std::vector<model::Question> all = questions();
  const auto q = std::find_if(all.begin(), all.end(), [&question](const model::Question& each) { return each.name == question; });
  if (q == all.end()) return {{"error", question, "not a question ds answers"}};
  if (q->subject.empty() && !subject.empty())  // a subject it would ignore: the answer would not be about it
    return {{"error", subject, question + " takes no subject: ds query <workspace> " + question}};
  // a question about an identity needs one the model declares: an empty answer would read as "it has none"
  const bool aboutOne = question == "relations" || question == "artefacts" || question == "behaviours" || question == "failures";
  if (aboutOne && subject.empty()) return {{"error", question, "needs an identity: ds query <workspace> " + question + " <identity>"}};
  const bool ofFile = (question == "findings" || question == "coverage") && isFile(subject);  // they may take a file
  const bool deep = question == "entities" && endsWith(subject, "*");                         // the include syntax: all it holds (DS-444)
  if (deep) return entities(subject);
  const bool aboutWhat = (question == "entities" || question == "findings" || question == "coverage") && !subject.empty() && !ofFile;
  if ((aboutOne || aboutWhat) && !known(subject)) {
    const std::vector<std::string> near = nearTo(subject);
    std::string detail = "nothing in the model is named so";
    for (size_t i = 0; i < near.size(); ++i) detail += (i ? ", " : "; did you mean ") + near[i];
    return {{"error", subject, detail}};
  }
  const std::string why = refused(*q, subject);
  if (!why.empty()) return {{"error", subject, why}};
  if (question == "diagrams") return diagrams();
  if (question == "entities") return entities(subject);
  if (question == "relations") return relations(subject);
  if (question == "artefacts") return artefacts(subject);
  if (question == "behaviours") return behaviours(subject);
  if (question == "failures") return failures(subject);
  if (question == "shows") return shows(subject);
  if (question == "findings") return findings(subject);
  if (question == "coverage") return coverage(subject);
  if (question == "walkthroughs") return walkthroughs(subject);
  return coupling();
}

// What each question takes, read from the metamodel so a kind the language gains is taken where it fits (DS-436):
// a scope is a kind others are declared in, a design element what may realise, satisfy or obey, what has a
// behaviour what a behaviour may be of (and a class, whose methods' are its own, DS-53), and what a failure may
// escape what has the escapes the check writes.
std::string Query::refused(const model::Question& question, const std::string& subject) const {
  const model::Entity* e = subject.empty() ? nullptr : model_.find(subject);
  if (!e) return "";  // a scope with no entity of its own, or none asked
  const model::Kind kind = e->kind;
  const auto relates = [this, kind](const std::string& relation, bool from) {
    const model::RelationSpec* r = metamodel_.relationOf(relation);
    const std::vector<model::Kind>& kinds = r ? (from ? r->from : r->to) : std::vector<model::Kind>{};
    return r && (kinds.empty() || std::find(kinds.begin(), kinds.end(), kind) != kinds.end());
  };
  bool takes = true;
  if (question.name == "entities") {
    takes = false;
    for (const auto& spec : metamodel_.kinds())
      takes = takes || std::find(spec.parents.begin(), spec.parents.end(), kind) != spec.parents.end();
  } else if (question.name == "artefacts") {
    takes = relates("realises", true) || relates("satisfies", true) || relates("constrained-by", true);
  } else if (question.name == "behaviours") {
    takes = kind == model::Kind::Class || relates("behaviour-of", false);
  } else if (question.name == "failures") {
    takes = metamodel_.fieldOf(kind, "escapes") != nullptr;
  }
  if (takes) return "";
  const std::string a = metamodel_.nameOf(kind);
  std::string why =
      subject + " is " + (a.find_first_of("aeio") == 0 ? "an " : "a ") + a + ": " + question.name + " takes " + question.takes;
  if (question.name == "artefacts" && metamodel_.isArtefact(kind))
    why += "; ds query <workspace> relations " + subject + " says what it links";
  return why;
}

bool Query::isFile(const std::string& subject) { return endsWith(subject, ".ds"); }

bool Query::within(const model::Location& at, const std::string& id) const {
  // the entity's span or a span of what it holds, down every level: a package's modules may sit in many files
  const model::Index index = model_.index();
  std::vector<std::string> pending{id};
  std::set<std::string> seen;
  while (!pending.empty()) {
    const std::string next = pending.back();
    pending.pop_back();
    if (!seen.insert(next).second) continue;
    if (const model::Entity* e = model_.find(next)) {
      const model::Location& span = e->location;
      if (span.file == at.file && at.line >= span.line && at.line <= std::max(span.line, span.endLine)) return true;
    }
    for (const auto& r : index.fromOfKind(next, "contains")) pending.push_back(r.to);
  }
  return false;
}

// an entity as an answer: its kind named by the metamodel the model is written in
Answer Query::entityAnswer(const model::Entity& entity, const std::string& detail) const {
  return {metamodel_.nameOf(entity.kind), entity.id, detail};
}

bool Query::endsWith(const std::string& text, const std::string& suffix) {
  return text.size() >= suffix.size() && text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
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
    out.push_back({metamodel_.nameOf(p.second.kind), name, kind == p.second.fields.end() ? "class" : kind->second});
  }
  return out;
}

std::vector<Answer> Query::entities(const std::string& asked) const {
  if (endsWith(asked, "*")) {  // written as a diagram's include writes it: what it takes at any depth (DS-444)
    const Pattern pattern(model_, asked);
    std::vector<Answer> out;
    for (const auto& p : model_.entities())
      if (!p.second.expected && pattern.takes(p.second))
        out.push_back(entityAnswer(p.second, p.second.fields.count("title") ? p.second.fields.at("title") : ""));
    if (out.empty()) return {{"error", asked, "nothing in the model falls under the pattern"}};
    return out;
  }
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
    // the other end and the direction, as before, and both ends as the model holds them (DS-445)
    if (r.from == id)
      out.push_back({r.kind, r.to, "->" + (r.multiplicity.empty() ? "" : " " + r.multiplicity), r.from, r.to});
    else
      out.push_back({r.kind, r.from, "<-", r.from, r.to});
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
  for (const auto& r : model_.index().toOfKind(id, "behaviour-of")) {
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
  const auto filter = d ? FilterFactory::of(metamodel_, *d) : nullptr;
  if (!filter) return {{"error", diagram, "no such diagram"}};
  std::vector<Answer> out;
  for (const auto& id : select(*filter)) out.push_back(entityAnswer(*model_.find(id), ""));
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

std::vector<Answer> Query::findings(const std::string& subject) const {
  // a file as today (DS-436 keeps it), or an identity: what lies on the entity or on anything it holds
  std::vector<Answer> out;
  for (const auto& f : model_.findings()) {
    if (!subject.empty() && (isFile(subject) ? !endsWith(f.location.file, subject) : !within(f.location, subject))) continue;
    out.push_back({f.code, f.location.file + ":" + std::to_string(f.location.line) + ":" + std::to_string(f.location.column), f.message});
  }
  return out;
}

// What is not yet written (DS-216): every absence the check keeps apart; of a file, or of an identity: those that
// name it, and those that lie on it or on anything it holds (DS-436)
std::vector<Answer> Query::coverage(const std::string& subject) const {
  std::vector<Answer> out;
  for (const auto& f : model_.coverage()) {
    if (!subject.empty() && (isFile(subject) ? !endsWith(f.location.file, subject)
                                             : f.message.find(subject) == std::string::npos && !within(f.location, subject)))
      continue;
    out.push_back({f.code, f.location.file + ":" + std::to_string(f.location.line) + ":" + std::to_string(f.location.column), f.message});
  }
  return out;
}

// The walkthroughs, by identity, with their titles; of one, its steps in order, each with its title and what it shows
// (DS-471). A name that is no walkthrough is told, not answered empty.
std::vector<Answer> Query::walkthroughs(const std::string& subject) const {
  const std::vector<model::Walkthrough> walks = model::Walkthroughs::of(model_);
  std::vector<Answer> out;
  if (subject.empty()) {
    for (const auto& w : walks) out.push_back({"walkthrough", w.id, w.title});
    return out;
  }
  const std::string id = model::Identity::ofDiagram(subject).text();
  for (const auto& w : walks) {
    if (w.id != id && w.id != subject) continue;
    for (const auto& s : w.steps) out.push_back({"step", s.id, s.title + " → " + s.show + (s.scenario.empty() ? "" : " / " + s.scenario)});
    return out;
  }
  return {{"error", subject, "no walkthrough is named so: ds query <workspace> walkthroughs lists them"}};
}

}  // namespace query
}  // namespace ds
