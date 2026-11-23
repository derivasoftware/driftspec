#include "ds/check/Resolver.hpp"

#include <set>
#include <vector>

#include "ds/bind/TypeTokens.hpp"
#include "ds/model/Metamodel.hpp"

namespace ds {
namespace check {

std::string Resolver::resolve(const std::string& name, const std::string& scope) const {
  // a bare name is looked for from the innermost scope outward first: a class Cart beside its user wins over
  // a root package named Cart (DS-224); a qualified name, an artefact or a root package is found as written
  if (!model::Identity(name).isQualified() && !model::ArtefactName::matches(name)) {
    for (std::string s = scope; !s.empty(); s = model::Scope(s).outer().text()) {
      const std::string candidate = model::Scope(s).qualify(name).text();
      if (candidate != name && model_.find(candidate)) return candidate;
    }
  }
  if (model_.find(name)) return name;  // a full id (pkg@mod::Class), a global artefact, a package
  const std::string bare = model::ArtefactName(name).withoutRevision().text();
  if (bare != name && model_.find(bare)) return bare;
  if (model::Identity(name).isAbsolute()) return "";  // absolute, and nobody declares it
  for (std::string s = scope; !s.empty(); s = model::Scope(s).outer().text()) {
    const std::string candidate = model::Scope(s).qualify(name).text();
    if (model_.find(candidate)) return candidate;
  }
  // a bare name that no scope on the way declares: the class of that name in the same package, as the code
  // imports it from a sibling module (`Plan` written in cli for plan::Plan). Two classes of the name: none,
  // and the linker says so (DS-325). The same rule argos applies to the code (DS-18)
  if (!model::Identity(name).isQualified() && !model::ArtefactName::matches(name)) return inPackage(name, scope);
  return "";
}

std::string Resolver::inPackage(const std::string& name, const std::string& scope) const {
  const std::vector<std::string> found = namesakes(name, scope);
  return found.size() == 1 ? found.front() : "";  // two of the name in the package: it relates nothing
}

// a bare name that resolves nowhere because two classes of its package share it: an incoherence, said (DS-325)
std::vector<std::string> Resolver::ambiguous(const std::string& name, const std::string& scope) const {
  if (model::Identity(name).isQualified() || model::ArtefactName::matches(name) || !resolve(name, scope).empty()) return {};
  std::vector<std::string> found = namesakes(name, scope);
  if (found.size() < 2) found.clear();
  return found;
}

std::vector<std::string> Resolver::namesakes(const std::string& name, const std::string& scope) const {
  const std::string package = model::Scope(scope).package().text();
  if (package.empty()) return {};  // written from the top package: no package to ask
  const auto open = name.find('<');
  const std::string simple = name.substr(0, open);
  const std::string arity = open == std::string::npos ? "" : name.substr(open);
  // the model keeps the declared classes and enums by package and name: the arity is told on that short list
  std::vector<std::string> found;
  for (const std::string& id : model_.index().named(package, simple)) {
    const bool templated = !id.empty() && id.back() == '>';
    if (arity.empty() ? templated : id.size() < arity.size() || id.compare(id.size() - arity.size(), arity.size(), arity) != 0) continue;
    found.push_back(id);
  }
  return found;
}

bool Resolver::derives(const std::string& derived, const std::string& base) const {
  std::vector<std::string> pending{derived};
  std::set<std::string> seen;
  while (!pending.empty()) {
    const std::string current = pending.back();
    pending.pop_back();
    if (current == base) return true;
    if (!seen.insert(current).second) continue;
    for (const auto& r : model_.index().fromOfKind(current, "inherits")) {
      pending.push_back(r.to);
    }
  }
  return false;
}

// a member is found where it is declared, the type first and then its bases, nearest first (DS-105)
const model::Entity* Resolver::member(const std::string& type, const std::string& name) const {
  std::vector<std::string> level{type};
  std::set<std::string> seen;
  while (!level.empty()) {
    std::vector<std::string> next;
    for (const auto& t : level) {
      if (!seen.insert(t).second) continue;
      if (const model::Entity* m = model_.find(t + "::" + name)) return m;
      for (const auto& r : model_.index().fromOfKind(t, "inherits")) next.push_back(r.to);
    }
    level.swap(next);
  }
  return nullptr;
}

std::string Resolver::privateOnTheWay(const std::string& from, const std::string& target) const {
  std::set<std::string> chain;
  for (std::string s = from; !s.empty(); s = model::Scope(s).outer().text()) chain.insert(s);
  // down from the first container they share: a child is seen from inside its parent, so siblings see each other
  for (std::string t = target; !t.empty() && !chain.count(t); t = model::Scope(t).outer().text()) {
    const model::Entity* e = model_.find(t);
    if (!e) return "";
    const auto visibility = e->fields.find("visibility");
    const std::string parent = model::Scope(t).outer().text();
    if (visibility != e->fields.end() && visibility->second == "-" && !chain.count(parent)) return t;
    // protected: seen from inside its class and from a class that derives from it (DS-224)
    if (visibility != e->fields.end() && visibility->second == "#" && !chain.count(parent)) {
      bool derived = false;
      for (const auto& s : chain) derived = derived || (model_.find(s) && model_.find(s)->kind == model::Kind::Class && derives(s, parent));
      if (!derived) return t;
    }
  }
  return "";
}

// the element of a type is the one entity of the design its text names; none, or two or more, and the type
// has no element: calls through it are not typed, and nothing is said (DS-18)
std::string Resolver::resolveTypeText(const std::string& text, const std::string& scope) const {
  std::set<std::string> designed;
  for (const bind::TypeToken& token : bind::scanTypeTokens(text)) {
    const std::string name = token.args.empty() ? token.name : model::Identity(token.name).withArity(token.args.size()).text();
    const std::string id = resolve(name, scope);
    const model::Entity* e = id.empty() ? nullptr : model_.find(id);
    if (e && !e->expected) designed.insert(id);
  }
  return designed.size() == 1 ? *designed.begin() : "";
}

}  // namespace check
}  // namespace ds
