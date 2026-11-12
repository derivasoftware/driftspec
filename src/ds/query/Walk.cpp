#include "ds/query/Walk.hpp"

#include <algorithm>
#include <functional>

#include "ds/model/Identity.hpp"
#include "ds/model/SequenceTree.hpp"

namespace ds {
namespace query {

const char* const kEveryBranch = "*";

namespace {

std::string fieldOf(const model::Entity& e, const std::string& key) {
  const auto it = e.fields.find(key);
  return it == e.fields.end() ? "" : it->second;
}

}  // namespace

std::vector<Taken> stepsOf(const model::Model& model, const std::string& sequence, const std::string& path,
                           std::set<std::string>* out_kept) {
  std::vector<const model::Entity*> messages;
  if (path == kEveryBranch) {
    const model::SequenceTree tree(model, sequence);  // every branch, as written; kept while its answer is read
    for (const auto& id : tree.messages()) messages.push_back(model.find(id));
  } else {
    messages = model::pathOf(model, sequence, path, out_kept);
  }
  std::vector<Taken> out;
  for (const model::Entity* m : messages) {
    // where a call goes on is derived by the checker: the behaviour it enters, and the named branch of it
    const std::string next = fieldOf(*m, "expands");
    out.push_back({m->id, model.find(next) ? next : "", model.find(next) ? fieldOf(*m, "expands-path") : ""});
  }
  return out;
}

std::vector<Walked> walkOf(const model::Model& model, const std::string& sequence, const std::string& path, int depth) {
  std::vector<Walked> out;
  std::set<std::string> stack;
  std::function<void(const std::string&, const std::string&, int)> walk = [&](const std::string& seq, const std::string& p, int level) {
    const std::string key = model::withScenario(seq, p);
    if (!stack.insert(key).second) return;  // a behaviour already being walked: the cycle ends here
    for (const Taken& t : stepsOf(model, seq, p)) {
      out.push_back({t.message, level});
      if (!t.next.empty() && level <= depth) walk(t.next, t.nextPath, level + 1);
    }
    stack.erase(key);
  };
  walk(sequence, path, 1);
  return out;
}

}  // namespace query
}  // namespace ds
