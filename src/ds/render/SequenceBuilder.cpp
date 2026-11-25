#include "ds/render/SequenceBuilder.hpp"

#include <algorithm>
#include <limits>

#include "ds/model/SequenceTree.hpp"
#include "ds/query/Walk.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace render {

std::string SequenceBuilder::closedAround(const std::string& typeId) const {
  if (typeId.empty()) return "";
  const std::string box = cut_.standsFor(typeId);
  return cut_.closed(box) ? box : "";  // a module that runs functions is a closed box itself
}

std::string SequenceBuilder::boxLine(const std::string& box) {
  boxLines_.insert(box);
  const model::Entity* e = model_.find(box);
  return lifeline(box, box.substr(box.find_last_of('@') == std::string::npos ? 0 : box.find_last_of('@') + 1),
                  e && e->kind == model::Kind::Package ? "package" : "namespace", box);
}

std::string SequenceBuilder::lineOf(const std::string& typeId, const std::string& name, const std::string& behaviour) {
  const std::string box = closedAround(typeId);
  if (!box.empty()) return boxLine(box);
  const model::Entity* type = typeId.empty() ? nullptr : model_.find(typeId);
  if (type && (type->kind == model::Kind::Package || type->kind == model::Kind::Module)) {
    return lifeline(typeId, type->name, type->kind == model::Kind::Package ? "package" : "namespace", typeId);
  }
  const std::string typeName = typeId.empty() ? "?" : model::Identity(typeId).lastName();  // an interface has no ::
  // two objects of one type are two lifelines: keyed by the name in the context, not by the type alone; a
  // participant named as what it is (an interface, a class) says it once
  return lifeline(typeId.empty() ? behaviour + "::" + name : typeId + "|" + name, name == typeName ? name : name + ": " + typeName,
                  "participant", typeId);
}

bool SequenceBuilder::followed(const std::string& behaviour, int depth) const {
  return cut_.follows(model::Identity(behaviour).subject().text(), depth);
}

std::string SequenceBuilder::lifeline(const std::string& key, const std::string& label, const std::string& classifier,
                                      const std::string& typeId) {
  auto it = lifelineOf_.find(key);
  if (it != lifelineOf_.end()) return it->second;
  nlohmann::json node = {{"id", key}, {"kind", "lifeline"}, {"label", label}, {"classifier", classifier}};
  if (!typeId.empty()) {
    node["href"] = "#" + typeId;  // a page that also shows the class diagram lands on the class
    node["title"] = typeId;
    const model::Entity* type = model_.find(typeId);
    if (type && !type->location.file.empty())
      node["refs"] = {{"location", type->location.file + ":" + std::to_string(type->location.line)}};
  }
  nodes.push_back(node);
  lifelineOf_[key] = key;
  return key;
}

std::string SequenceBuilder::shortName(const std::string& id) {
  const auto at = id.rfind("::");
  return at == std::string::npos ? id : id.substr(at + 2);
}

void SequenceBuilder::emit(const model::Entity& seq, const std::string& selfLine, const std::string& backLine, const std::string& path,
                           int depth, std::set<std::string>& stack) {
  const std::string key = model::Identity(seq.id).withScenario(path).text();
  if (stack.count(key)) return;  // recursion: drawn once
  stack.insert(key);
  std::map<std::string, int> firstOrder, lastOrder, elseOrder;  // by frame id
  // a path draws only the messages it takes, and only the frames it cannot tell; every branch otherwise
  std::set<std::string> kept;
  const bool whole = path == query::kEveryBranch;
  // what it takes and where each call goes on: the one walk of a behaviour (DS-130)
  const std::vector<query::Taken> steps = query::Walker(model_).steps(seq.id, path, whole ? nullptr : &kept);
  const model::SequenceTree tree(model_, seq.id);  // the frame around each step, where an alt's else begins (DS-172)
  auto drawn = [&](const std::string& frame) { return whole || kept.count(frame); };
  // a frame that holds no message (only prose, or a branch that ends with nothing) still has a row of its own
  std::vector<const model::Entity*> empty;
  if (whole) {
    for (const auto& f : tree.frames())
      if (tree.first(f).empty()) empty.push_back(model_.find(f));
    std::stable_sort(empty.begin(), empty.end(),
                     [&tree](const model::Entity* a, const model::Entity* b) { return tree.at(a->id) < tree.at(b->id); });
  }
  size_t nextEmpty = 0;
  auto flushEmpty = [&](int before) {
    for (; nextEmpty < empty.size() && tree.at(empty[nextEmpty]->id) <= before; ++nextEmpty) {
      const model::Entity& f = *empty[nextEmpty];
      edges.push_back({{"from", selfLine}, {"to", selfLine}, {"kind", "message"}, {"order", order}, {"label", "..."}, {"dashed", true}});
      for (std::string id = f.id; !id.empty(); id = tree.frameOf(id)) {
        if (!firstOrder.count(id)) firstOrder[id] = order;
        lastOrder[id] = order;
      }
      ++order;
    }
  };
  for (const query::Taken& taken : steps) {
    const model::Entity* m = model_.find(taken.message);
    flushEmpty(tree.orderOf(m->id));
    if (m->field("return") == "true") {
      // a return inside a frame ends that branch early: it is drawn, so the frame is never empty
      std::string frame = tree.frameOf(m->id);
      while (!frame.empty() && !drawn(frame)) frame = tree.frameOf(frame);
      const std::string fails = m->field("fails");                         // a path that ends failing is drawn wherever it ends (DS-187)
      if (backLine.empty() || (frame.empty() && fails.empty())) continue;  // the last return: the arrow back tells it
      if (selfLine == backLine && boxLines_.count(selfLine)) continue;     // inside a closed box: nothing crosses
      for (std::string f = frame; !f.empty(); f = tree.frameOf(f)) {
        if (!drawn(f)) continue;
        if (!firstOrder.count(f)) firstOrder[f] = order;
        if (tree.elseOf(f) == m->id) elseOrder[f] = order;
      }
      const std::string value = m->field("result");
      edges.push_back({{"from", selfLine},
                       {"to", backLine},
                       {"kind", "message"},
                       {"order", order++},
                       {"label", !fails.empty()  ? "fail " + fails
                                 : value.empty() ? "return"
                                                 : "return " + value},
                       {"dashed", true}});
      for (std::string f = frame; !f.empty(); f = tree.frameOf(f)) {
        if (drawn(f)) lastOrder[f] = order - 1;
      }
      continue;
    }
    const std::string to = m->field("to");
    const std::string typeId = m->field("type");
    // who sends is the sender written, a participant like any other; the subject when none is (DS-232)
    const std::string sender = m->field("from");
    const std::string from =
        sender.empty() || sender == "self" || m->field("from-type").empty() ? selfLine : lineOf(m->field("from-type"), sender, seq.id);
    std::string target = selfLine;
    const std::string box = closedAround(typeId);
    if (to == "self" && box.empty() && !typeId.empty() && typeId != selfLine) {
      // a function of another module (or of the class's module, called from a method) runs on that module's lifeline
      target = lifeline(typeId, shortName(typeId.substr(typeId.find_last_of('@') == std::string::npos ? 0 : typeId.find_last_of('@') + 1)),
                        "participant", typeId);
    } else if (to != "self" || !box.empty()) {
      target = lineOf(typeId, to, seq.id);  // what a closed box holds, the box stands for
    }
    const std::string frame = tree.frameOf(m->id);
    const std::string& entered = taken.next;
    const bool goesOn = !entered.empty() && followed(entered, depth);
    if (target == from && boxLines_.count(from)) {
      // both ends have the same representative: the message is internal and is not drawn. What it calls may
      // still cross, so it is followed like any other, and a frame is drawn around what crosses inside it
      const int before = order;
      if (goesOn) emit(*model_.find(entered), target, from, taken.nextPath, depth - 1, stack);
      if (order > before) {
        for (std::string f = frame; !f.empty(); f = tree.frameOf(f)) {
          if (!drawn(f)) continue;
          if (!firstOrder.count(f)) firstOrder[f] = before;
          if (tree.elseOf(f) == m->id) elseOrder[f] = before;
          lastOrder[f] = order - 1;
        }
      }
      continue;
    }
    for (std::string f = frame; !f.empty(); f = tree.frameOf(f)) {
      if (!drawn(f)) continue;  // a branch the path takes is drawn inline, without its frame
      if (!firstOrder.count(f)) firstOrder[f] = order;
      if (tree.elseOf(f) == m->id) elseOrder[f] = order;
    }
    const int row = order++;
    nlohmann::json edge = {
        {"from", from},
        {"to", target},
        {"kind", "message"},
        {"order", row},
        // a message to a closed box says which of its children takes it
        {"label", (box.empty() || box == typeId ? "" : model::Identity(typeId).lastName() + "::") + m->field("op") + "(" +
                      m->field("args") + ")" + (m->field("scenario").empty() ? "" : " / " + m->field("scenario"))}};
    nlohmann::json refs = {{"location", m->location.file + ":" + std::to_string(m->location.line)}};
    const std::string& expands = taken.next;
    std::string title;
    if (!typeId.empty()) title = typeId + "::" + m->field("op");
    const std::string change = m->field("change");  // a diff marks messages too
    if (!change.empty()) {
      refs["change"] = change;
      const std::string detail = m->field("change-detail");
      title = change + (detail.empty() ? "" : ": " + detail) + (title.empty() ? "" : "\n" + title);
    }
    if (!expands.empty()) {
      refs["expands"] = expands;
      title += (title.empty() ? "" : "\n") + std::string(goesOn ? "expanded below: " : "expands to: ") + model_.find(expands)->name;
      if (!goesOn) edge["href"] = "#" + expands;  // a page that also shows the child lands on it
    }
    if (!title.empty()) edge["title"] = title;
    const size_t at = edges.size();
    edges.push_back(edge);
    if (goesOn) {
      const int first = order;
      emit(*model_.find(expands), target, from, taken.nextPath, depth - 1, stack);
      if (order > first) refs["span"] = std::to_string(first) + "-" + std::to_string(order - 1);  // the rows the expansion occupies
    }
    edges[at]["refs"] = refs;
    const std::string result = m->field("result");
    if (!result.empty())
      edges.push_back({{"from", target}, {"to", from}, {"kind", "message"}, {"order", order++}, {"label", result}, {"dashed", true}});
    for (std::string f = frame; !f.empty(); f = tree.frameOf(f))
      if (drawn(f)) lastOrder[f] = order - 1;  // after any expansion
  }
  flushEmpty(std::numeric_limits<int>::max());
  for (const auto& f : firstOrder) {
    const model::Entity* fe = model_.find(f.first);
    nlohmann::json frame = {
        {"id", f.first},
        {"kind", "frame"},
        {"label", fe->field("kind") == "handler" ? "on " + fe->field("failure") : fe->field("kind") + " " + fe->field("condition")},
        {"classifier", fe->field("kind")},
        {"span", nlohmann::json::array({f.second, lastOrder[f.first]})}};
    if (elseOrder.count(f.first)) frame["dividers"] = nlohmann::json::array({{{"at", elseOrder[f.first]}, {"label", "else"}}});
    nodes.push_back(frame);
  }
  stack.erase(seq.id);
}

}  // namespace render
}  // namespace ds
