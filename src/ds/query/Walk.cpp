#include "ds/query/Walk.hpp"

#include <algorithm>
#include <functional>

#include "ds/axis/Cut.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/SequenceTree.hpp"

namespace ds {
namespace query {

const char* const kEveryBranch = "*";

std::vector<Taken> Walker::steps(const std::string& sequence, const std::string& path, std::set<std::string>* out_kept) const {
  const model::Model& model = model_;
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
    const std::string next = m->field("expands");
    out.push_back({m->id, model.find(next) ? next : "", model.find(next) ? m->field("expands-path") : ""});
  }
  return out;
}

std::vector<Walked> Walker::walk(const std::string& sequence, const std::string& path, int depth, const axis::Cut* cut) const {
  std::vector<Walked> out;
  std::set<std::string> stack;
  std::function<void(const std::string&, const std::string&, int)> walk = [&](const std::string& seq, const std::string& p, int level) {
    const std::string key = model::Identity(seq).withScenario(p).text();
    if (!stack.insert(key).second) return;  // a behaviour already being walked: the cycle ends here
    for (const Taken& t : steps(seq, p)) {
      out.push_back({t.message, level});
      if (t.next.empty()) continue;
      const int left = depth - level + 1;  // the levels of the depth asked for that are left
      if (cut ? cut->follows(model::Identity(t.next).subject().text(), left) : left > 0) walk(t.next, t.nextPath, level + 1);
    }
    stack.erase(key);
  };
  walk(sequence, path, 1);
  return out;
}

}  // namespace query
}  // namespace ds
