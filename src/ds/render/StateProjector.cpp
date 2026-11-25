#include "ds/render/StateProjector.hpp"

#include <algorithm>
#include <nlohmann/json.hpp>
#include <sstream>

#include "ds/machine/Machine.hpp"
#include "ds/query/Filter.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace render {

using text::splitTrimmed;

nlohmann::json StateProjector::project(const model::Entity& diagram) const {
  nlohmann::json nodes = nlohmann::json::array();
  nlohmann::json edges = nlohmann::json::array();
  std::set<std::string> drawn;
  for (const auto& machine : query::StateFilter(splitTrimmed(diagram.field("include"), ',')).machines(model_)) {  // DS-127
    const model::Entity* state = model_.find(machine);
    const machine::Machine written(model_, *state);  // a state a rule computes shows its rule (DS-249)
    const std::vector<machine::Call> calls = written.calls();
    const auto box = [&](const std::string& name) {
      const std::string id = state->id + "::" + name;
      if (!drawn.insert(id).second) return id;
      nlohmann::json node = {{"id", id}, {"kind", "state"}, {"label", name}, {"classifier", "state"}};  // a capsule (DS-442)
      const std::vector<std::string> rules = written.rules(name);
      if (!rules.empty()) {
        nlohmann::json lines = nlohmann::json::array();
        for (const std::string& rule : rules)
          for (const std::string& line : text::wrap("if " + rule, 40)) lines.push_back(line);
        node["sections"] = nlohmann::json::array({lines});
        node["sectionKinds"] = nlohmann::json::array({"attributes"});
      }
      if (const model::Entity* view = model_.find(id)) {
        node["refs"] = {{"location", view->location.file + ":" + std::to_string(view->location.line)}};
        nlohmann::json sections = nlohmann::json::array();
        nlohmann::json kinds = nlohmann::json::array();
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
          node["sections"] = sections;
          node["sectionKinds"] = kinds;
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
      nodes.push_back(
          {{"id", start}, {"kind", "initial"}, {"label", ""}, {"classifier", "initial"}, {"title", "where " + state->name + " starts"}});
      edges.push_back({{"from", start}, {"to", box(initial)}, {"kind", "association"}});
    }
    std::stringstream first(state->field("transitions"));
    for (std::string move; std::getline(first, move);) {  // in the order written: the first jumps go down
      const auto cut = move.find('>');
      if (cut == std::string::npos) continue;
      box(move.substr(0, cut));
      const auto end = move.find('>', cut + 1);
      if (end != std::string::npos) box(move.substr(cut + 1, end - cut - 1));
    }
    for (const std::string& name : written.states()) box(name);  // a state a rule computes is drawn though no jump names it
    for (const std::string& name : written.finals()) {           // each final state ends in a bullseye (DS-442)
      const std::string end = state->id + "::final::" + name;
      nodes.push_back(
          {{"id", end}, {"kind", "final"}, {"label", ""}, {"classifier", "final"}, {"title", state->name + " ends in " + name}});
      edges.push_back({{"from", box(name)}, {"to", end}, {"kind", "association"}});
    }
    for (const auto& member : model_.index().members(state->id)) {  // a view no transition names is still drawn: it is the finding
      const model::Entity& view = *model_.find(member);
      if (view.kind == model::Kind::View) box(view.name);
    }
    std::stringstream moves(state->field("transitions"));
    for (std::string move; std::getline(moves, move);) {
      const auto arrow = move.find('>');
      const auto second = move.find('>', arrow + 1);
      if (arrow == std::string::npos || second == std::string::npos) continue;
      // a jump says the behaviours that call its event (DS-250): the label stays as it is written
      const std::string label = move.substr(second + 1);
      const std::string event = label.substr(0, label.find_first_of(" ["));
      std::vector<std::string> callers;
      for (const machine::Call& call : calls)
        if (call.event == event && std::find(callers.begin(), callers.end(), call.behaviour) == callers.end())
          callers.push_back(call.behaviour);
      std::string in;
      for (const std::string& caller : callers) in += (in.empty() ? "" : ", ") + caller;
      nlohmann::json edge = {{"from", box(move.substr(0, arrow))},
                             {"to", box(move.substr(arrow + 1, second - arrow - 1))},
                             {"kind", "association"},
                             {"label", label}};
      if (!in.empty()) edge["title"] = event + " is called in " + in;  // one nobody calls says nothing
      edges.push_back(edge);
    }
  }
  nlohmann::json out;
  if (nodes.empty()) out["notice"] = "No state machine selected: include names its class or interface (shop@web)";
  out["nodes"] = nodes;
  out["edges"] = edges;
  return out;
}

}  // namespace render
}  // namespace ds
