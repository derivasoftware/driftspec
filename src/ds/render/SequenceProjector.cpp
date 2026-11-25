#include "ds/render/SequenceProjector.hpp"

#include "ds/axis/Cut.hpp"
#include "ds/query/Filter.hpp"
#include "ds/query/Walk.hpp"
#include "ds/render/SequenceBuilder.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace render {

using text::splitTrimmed;

IrDocument SequenceProjector::project(const model::Entity& diagram) const {
  const int depth = query::SequenceFilter::levelsOf(diagram);
  // the behaviours selected, by subject id, by sequence id, or a namespace: the query's filter (DS-127)
  std::vector<const model::Entity*> selected;
  std::map<const model::Entity*, std::string> pathOf;  // a named branch asked for: the path of that behaviour
  for (const query::Behaviour& taken :
       query::SequenceFilter(metamodel_, splitTrimmed(diagram.field("include"), ','), depth, diagram.id).behaviours(model_)) {
    selected.push_back(taken.sequence);
    if (taken.path != query::kEveryBranch) pathOf[taken.sequence] = taken.path;
  }
  // the lifelines stand on the cut the diagram writes, and the calls followed are the ones it says (DS-231)
  const axis::Cut cut(model_, metamodel_, diagram);
  SequenceBuilder b(model_, cut);
  IrDocument out;
  std::set<std::string> subjects;  // the lifelines of the boxes and use cases whose scenarios are drawn
  for (const model::Entity* seq : selected) {
    const std::string subjectId = model::Identity(seq->id).subject().text();
    const model::Entity* subject = model_.find(subjectId);
    const bool written =
        subject && (subject->kind == model::Kind::Package || subject->kind == model::Kind::Module || subject->kind == model::Kind::UseCase);
    // a method runs on its class, a function on its module
    const bool function = subject && subject->kind == model::Kind::Function;
    const bool method = subject && (subject->kind == model::Kind::Method || subject->kind == model::Kind::Operation || function);
    const model::Entity* cls = method ? model_.find(subject->scope) : subject;
    // the caller, leftmost, invokes the entry operation on the subject: what is being looked at is the first thing drawn
    const std::string caller = method ? b.lifeline("caller", "caller", "actor") : "";
    const std::string selfLabel = cls ? (function ? cls->name : "self: " + cls->name) : "self: " + subjectId;
    const std::string around = cls ? b.closedAround(cls->id) : "";
    // a scenario of a use case starts from whoever asks; one of a box runs on the box (DS-232)
    const std::string self = written && subject->kind == model::Kind::UseCase ? b.lifeline("caller", "caller", "actor")
                             : written                                        ? b.lineOf(subjectId, subject->name, seq->id)
                             : !around.empty()                                ? b.boxLine(around)
                                               : b.lifeline(cls ? cls->id : subjectId, selfLabel, "participant", cls ? cls->id : "");
    if (written) subjects.insert(self);
    if (method) {
      IrEdge call = IrEdge::between(caller, self, "message");
      call.order = b.order++;
      call.label = (around.empty() || around == cls->id ? "" : cls->name + "::") + subject->name + "(" + subject->field("params") + ")";
      call.title = subject->id;
      call.refs["location"] = subject->location.file + ":" + std::to_string(subject->location.line);
      b.edges.push_back(call);
    }
    std::set<std::string> stack;
    // a behaviour asked for by its subject is drawn whole; a scenario of it, as its path
    const std::string path = pathOf.count(seq) ? pathOf[seq] : query::kEveryBranch;
    b.emit(*seq, self, caller, path, depth, stack);
    const std::string returns = method ? subject->field("returns") : "";
    if (method && !returns.empty()) {  // a return written is drawn as written: ds knows no language's nothing (DS-18)
      std::string given = returns;     // what its `return` says, or the declared type
      for (const query::Taken& taken : query::Walker(model_).steps(seq->id, path)) {
        const model::Entity* m = model_.find(taken.message);
        if (m->field("return") == "true" && !m->field("result").empty()) given = m->field("result");
      }
      IrEdge back = IrEdge::between(self, caller, "message");
      back.order = b.order++;
      back.label = given;
      back.dashed = true;
      b.edges.push_back(back);
    }
  }
  // a scenario written between boxes may never send from its own subject: that lifeline is drawn when a message
  // touches it (DS-232)
  std::set<std::string> touched;
  for (const IrEdge& e : b.edges) {
    touched.insert(e.from);
    touched.insert(e.to);
  }
  for (const IrNode& n : b.nodes)
    if (!subjects.count(n.id) || touched.count(n.id)) out.nodes.push_back(n);
  if (b.edges.empty()) out.notice = "No behaviour selected: include names a subject (ds::Checker::run), a behaviour or a namespace.";
  out.edges = b.edges;
  return out;
}

}  // namespace render
}  // namespace ds
