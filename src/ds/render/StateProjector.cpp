#include "ds/render/StateProjector.hpp"

#include <algorithm>
#include <sstream>

#include "ds/machine/Machine.hpp"
#include "ds/query/Filter.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace render {

using text::splitTrimmed;

IrDocument StateProjector::project(const model::Entity& diagram) const {
  IrDocument out;
  std::vector<IrNode>& nodes = out.nodes;
  std::vector<IrEdge>& edges = out.edges;
  std::set<std::string> drawn;
  for (const auto& machine : query::StateFilter(splitTrimmed(diagram.field("include"), ',')).machines(model_)) {  // DS-127
    const model::Entity* state = model_.find(machine);
    const machine::Machine written(model_, *state);  // a state a rule computes shows its rule (DS-249)
    const std::vector<machine::Call> calls = written.calls();
    const auto box = [&](const std::string& name) {
      const std::string id = state->id + "::" + name;
      if (!drawn.insert(id).second) return id;
      IrNode node;  // a capsule (DS-442)
      node.id = id;
      node.kind = "state";
      node.label = name;
      node.classifier = "state";
      const std::vector<std::string> rules = written.rules(name);
      if (!rules.empty()) {
        std::vector<std::string> lines;
        for (const std::string& rule : rules)
          for (const std::string& line : text::wrap("if " + rule, 40)) lines.push_back(line);
        node.sections = {lines};
        node.sectionKinds = {"attributes"};
      }
      if (const model::Entity* view = model_.find(id)) {
        node.refs = {{"location", view->location.file + ":" + std::to_string(view->location.line)}};
        std::vector<std::vector<std::string>> sections;
        std::vector<std::string> kinds;
        const std::string text = view->field("text");
        if (!text.empty()) {
          sections.push_back(text::wrap(text, 36));
          kinds.push_back("attributes");
        }
        std::vector<std::string> binds;
        for (const auto& r : model_.index().from(id)) {
          if (r.kind == "satisfies" || r.kind == "constrained-by") binds.push_back(r.to);
        }
        if (!binds.empty()) {
          sections.push_back(binds);
          kinds.push_back("methods");
        }
        if (!sections.empty()) {
          node.sections = sections;
          node.sectionKinds = kinds;
        }
      }
      nodes.push_back(node);
      return id;
    };
    // the machine reads from where it starts: first in the document, so the layout flows down from it. Where it starts
    // is UML's filled dot, with an arrow to the initial state (DS-442)
    const std::string initial = written.initial();  // the one written, else where the defaults are (DS-249)
    if (!initial.empty()) {
      const std::string start = state->id + "::initial";
      IrNode dot;
      dot.id = start;
      dot.kind = "initial";
      dot.classifier = "initial";
      dot.title = "where " + state->name + " starts";
      nodes.push_back(dot);
      edges.push_back(IrEdge::between(start, box(initial), "association"));
    }
    for (const model::WrittenTransition& move : state->transitions) {  // in the order written: the first jumps go down
      box(move.from);
      box(move.to);
    }
    for (const std::string& name : written.states()) box(name);  // a state a rule computes is drawn though no jump names it
    for (const std::string& name : written.finals()) {           // each final state ends in a bullseye (DS-442)
      const std::string end = state->id + "::final::" + name;
      IrNode bullseye;
      bullseye.id = end;
      bullseye.kind = "final";
      bullseye.classifier = "final";
      bullseye.title = state->name + " ends in " + name;
      nodes.push_back(bullseye);
      edges.push_back(IrEdge::between(box(name), end, "association"));
    }
    for (const auto& member : model_.index().members(state->id)) {  // a view no transition names is still drawn: it is the finding
      const model::Entity& view = *model_.find(member);
      if (view.kind == model::Kind::View) box(view.name);
    }
    for (const model::WrittenTransition& move : state->transitions) {
      // a jump says the behaviours that call its event (DS-250): the label is the transition as it is written
      const std::string label =
          move.event + (move.guard.empty() ? "" : " [" + move.guard + "]") + (move.action.empty() ? "" : " / " + move.action);
      const std::string& event = move.event;
      std::vector<std::string> callers;
      for (const machine::Call& call : calls)
        if (call.event == event && std::find(callers.begin(), callers.end(), call.behaviour) == callers.end())
          callers.push_back(call.behaviour);
      std::string in;
      for (const std::string& caller : callers) in += (in.empty() ? "" : ", ") + caller;
      const std::string from = box(move.from);
      IrEdge edge = IrEdge::between(from, box(move.to), "association");
      edge.label = label;
      if (!in.empty()) edge.title = event + " is called in " + in;  // one nobody calls says nothing
      edges.push_back(edge);
    }
  }
  if (nodes.empty()) out.notice = "No state machine selected: include names its class or interface (shop@web)";
  return out;
}

}  // namespace render
}  // namespace ds
