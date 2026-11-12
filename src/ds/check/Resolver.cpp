#include "ds/check/Resolver.hpp"

#include <set>
#include <vector>

#include "ds/model/Metamodel.hpp"

namespace ds {
namespace check {

using model::outer;

std::string Resolver::resolve(const std::string& name, const std::string& scope) const {
  if (model_.find(name)) return name;  // a full id (pkg@mod::Class), a global artefact, a package
  const std::string bare = model::withoutRevision(name);
  if (bare != name && model_.find(bare)) return bare;
  if (model::isAbsolute(name)) return "";  // absolute, and nobody declares it
  for (std::string s = scope; !s.empty(); s = outer(s)) {
    const std::string candidate = model::qualify(s, name);
    if (model_.find(candidate)) return candidate;
  }
  // the extern catalogue (R11): a bare name nobody declares in scope is the one extern
  // entity of that simple name, declared in any extern module of the workspace
  const std::vector<std::string> externs = externsNamed(name);
  return externs.size() == 1 ? externs.front() : "";
}

std::vector<std::string> Resolver::externsNamed(const std::string& name) const {
  std::vector<std::string> out;
  if (model::isQualified(name)) return out;
  // an extern is known by its simple name: a use's template arguments (tuple<2>) are not the
  // design's to check, so any arity of the use, and of the declaration, finds it (R11)
  const std::string simple = name.substr(0, name.find('<'));
  for (const auto& pair : model_.entities()) {
    const model::Entity& e = pair.second;
    if (e.has("extern") && (e.kind == model::Kind::Class || e.kind == model::Kind::Enum) && e.name == simple) out.push_back(e.id);
  }
  // several declarations of the name (tuple<1>, tuple<2>): the one of the use's arity, when there is one
  std::vector<std::string> exact;
  for (const auto& id : out) {
    if (id.size() >= name.size() + 2 && id.compare(id.size() - name.size() - 2, std::string::npos, "::" + name) == 0) exact.push_back(id);
  }
  return exact.size() == 1 ? exact : out;
}

bool Resolver::derives(const std::string& derived, const std::string& base) const {
  std::vector<std::string> pending{derived};
  std::set<std::string> seen;
  while (!pending.empty()) {
    const std::string current = pending.back();
    pending.pop_back();
    if (current == base) return true;
    if (!seen.insert(current).second) continue;
    for (const auto& r : model_.index().from(current)) {
      if (r.kind == "inherits") pending.push_back(r.to);
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
      for (const auto& r : model_.index().from(t))
        if (r.kind == "inherits") next.push_back(r.to);
    }
    level.swap(next);
  }
  return nullptr;
}

std::string Resolver::privateOnTheWay(const std::string& from, const std::string& target) const {
  std::set<std::string> chain;
  for (std::string s = from; !s.empty(); s = outer(s)) chain.insert(s);
  // down from the first container they share: a child is seen from inside its parent, so siblings see each other
  for (std::string t = target; !t.empty() && !chain.count(t); t = outer(t)) {
    const model::Entity* e = model_.find(t);
    if (!e || e->has("extern")) return "";
    const auto visibility = e->fields.find("visibility");
    if (visibility != e->fields.end() && visibility->second == "-" && !chain.count(outer(t))) return t;
  }
  return "";
}

std::string Resolver::resolveTypeText(const std::string& text, const std::string& scope) const {
  const auto trimmed = [](const std::string& s) {
    const auto b = s.find_first_not_of(' ');
    return b == std::string::npos ? std::string() : s.substr(b, s.find_last_not_of(' ') - b + 1);
  };
  // the arguments between the outer < >, split at their own level: map<string, pair<a, b>> has two
  const auto arguments = [&trimmed](const std::string& inside) {
    std::vector<std::string> out;
    int depth = 0;
    std::string arg;
    for (char c : inside) {
      if (c == '<') ++depth;
      if (c == '>') --depth;
      if (c == ',' && depth == 0) {
        out.push_back(trimmed(arg));
        arg.clear();
      } else {
        arg += c;
      }
    }
    if (!trimmed(arg).empty()) out.push_back(trimmed(arg));
    return out;
  };
  std::string t = trimmed(text);
  // containers hand over to their element (the value of a map), read from the table the binder reads (R16)
  for (;;) {
    const auto lt = t.find('<');
    const auto gt = t.rfind('>');
    if (lt == std::string::npos || gt == std::string::npos || gt < lt) break;
    const std::string head = t.substr(0, lt);
    const std::vector<std::string> args = arguments(t.substr(lt + 1, gt - lt - 1));
    if (const model::ContainerSpec* container = model::containerOf(head)) {
      t = container->element < static_cast<int>(args.size()) ? args[container->element] : "";
      continue;
    }
    t = model::arityId(head, args.size());  // a template use: identity is name + arity (R15)
    break;
  }
  if (t.empty()) return "";
  return resolve(t, scope);
}

}  // namespace check
}  // namespace ds
