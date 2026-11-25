#include "ds/bind/SequenceBinder.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <map>

#include "ds/bind/TypeTokens.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace bind {

using parse::Field;
using parse::TreeReader;
using text::join;
using text::splitTrimmed;

// ---- process artefacts: requirement, verification, constraint (R5, R18) ------

// A use case is an artefact identified by its id (R5) and a contract: its signature, the base it
// specialises, its outcomes as entities of their own (DS-77).
// ---- types (R15, R16) ----------------------------------------------------------

// A type is the code's text (DS-18). Its names are read out of it; each that the design declares is related
// with the holder's relation (owns by default, refs or shares as written, uses for a signature or a local).
// ---- classes (R14, R15, R17, R19) ----------------------------------------------

// ---- behaviour (block 3): a sequence is the behaviour of its subject ------------

void SequenceBinder::bindSteps(const BindContext& ctx, TSNode node, const model::Entity& sequence, const std::string& holder,
                               int& out_order, int& out_frames) const {
  // The steps are a tree (DS-172): each message and frame is contained by what encloses it, the sequence
  // or a frame, in the order written; an alt holds its sides as branches. `holder` is that encloser.
  const std::string sequenceId = sequence.id;
  const uint32_t n = ts_node_child_count(node);
  std::string current = holder;  // an alt's steps go to its first branch, then to its else branch
  for (uint32_t i = 0; i < n; ++i) {
    TSNode c = ts_node_child(node, i);
    if (!ts_node_is_named(c)) {
      if (std::strcmp(ts_node_type(c), "else") == 0 && TreeReader::is(node, "alt")) {
        // the else opens the alt's second side; `holder` is its first, alt::b0
        const std::string alt = holder.substr(0, holder.size() - model::Identity("").branch(false).text().size());
        current = branch(ctx, c, sequenceId, alt, 1, ctx.reader.fieldText(node, Field::else_scenario));
      }
      continue;
    }
    const model::Location loc = ctx.locationOf(c);
    if (TreeReader::is(c, "message") || TreeReader::is(c, "return") || TreeReader::is(c, "fail")) {
      model::Entity m;
      m.kind = model::Kind::Message;
      m.id = sequenceId + "::m" + std::to_string(out_order);
      m.scope = sequenceId;
      m.location = loc;
      const std::string from = ctx.reader.fieldText(c, Field::from);
      const std::string to = ctx.reader.fieldText(c, Field::to);
      m.fields["from"] = from.empty() ? "self" : from;
      m.fields["to"] = to.empty() ? "self" : to;
      m.fields["op"] = ctx.reader.fieldText(c, Field::op);
      if (TreeReader::is(c, "return")) {
        m.fields["result"] = ctx.reader.fieldText(c, Field::value);
        m.fields["return"] = "true";
      } else if (TreeReader::is(c, "fail")) {  // the path ends, failing: an ending like a return, with the failure's name
        m.fields["fails"] = ctx.reader.fieldText(c, Field::failure);
        m.fields["return"] = "true";
      } else {
        const std::string result = ctx.reader.fieldText(c, Field::result);
        if (!result.empty()) m.fields["result"] = result;
        const std::string scenario = ctx.reader.fieldText(c, Field::scenario);
        if (!scenario.empty()) m.fields["scenario"] = scenario;
      }
      int argc = 0;
      std::vector<std::string> args;
      const uint32_t k = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < k; ++j) {
        TSNode a = ts_node_named_child(c, j);
        if (!TreeReader::is(a, "arguments")) continue;
        const uint32_t na = ts_node_named_child_count(a);
        for (uint32_t x = 0; x < na; ++x) {
          args.push_back(std::string(text::trimWhitespace(ctx.reader.textOf(ts_node_named_child(a, x)))));
          ++argc;
        }
      }
      m.fields["args"] = join(args, ", ");
      m.args = args;
      m.fields["argc"] = std::to_string(argc);
      m.name = TreeReader::is(c, "return") ? "return" : TreeReader::is(c, "fail") ? "fail" : m.fields["to"] + "." + m.fields["op"];
      ctx.declare(m);
      ctx.relate({"contains", current.empty() ? sequenceId : current, m.id, loc, ""});
      ctx.noteMessage(
          {sequenceId, m.id, m.fields["to"], m.fields["op"], argc, loc, TreeReader::is(c, "return") || TreeReader::is(c, "fail")});
      ++out_order;
    } else if (TreeReader::is(c, "loop") || TreeReader::is(c, "alt") || TreeReader::is(c, "opt") || TreeReader::is(c, "attempt") ||
               TreeReader::is(c, "handler")) {
      model::Entity f;
      f.kind = model::Kind::Frame;
      f.id = sequenceId + "::f" + std::to_string(out_frames++);
      f.scope = sequenceId;
      f.location = loc;
      f.name = ts_node_type(c);
      f.fields["kind"] = ts_node_type(c);
      f.fields["condition"] = std::string(text::trimWhitespace(ctx.reader.fieldText(c, Field::condition)));
      if (TreeReader::is(c, "handler")) f.fields["failure"] = ctx.reader.fieldText(c, Field::failure);  // the failure it handles (DS-187)
      const std::string scenario = ctx.reader.fieldText(c, Field::scenario);
      if (!scenario.empty() && !TreeReader::is(c, "alt")) f.fields["scenario"] = scenario;  // an alt's sides name their own
      ctx.declare(f);
      ctx.relate({"contains", current.empty() ? sequenceId : current, f.id, loc, ""});
      // an alt holds its first side as a branch; the else, when written, opens the second
      const std::string inside = TreeReader::is(c, "alt") ? branch(ctx, c, sequenceId, f.id, 0, scenario) : f.id;
      bindSteps(ctx, c, *ctx.find(sequenceId), inside, out_order, out_frames);
    }
  }
}

std::string SequenceBinder::branch(const BindContext& ctx, TSNode at, const std::string& sequence, const std::string& alt, int side,
                                   const std::string& scenario) const {
  model::Entity b;
  b.kind = model::Kind::Frame;
  b.id = model::Identity(alt).branch(side == 1).text();
  b.scope = sequence;
  b.location = ctx.locationOf(at);  // the first side where the alt is written, the else where else is
  b.name = side == 0 ? "branch" : "else";
  b.fields["kind"] = "branch";
  if (!scenario.empty()) b.fields["scenario"] = scenario;
  ctx.declare(b);
  ctx.relate({"contains", alt, b.id, b.location, ""});
  return b.id;
}

void SequenceBinder::bind(TSNode c, const BindContext& ctx) const {
  const std::string& scope = ctx.scope;
  const std::string& owner = ctx.owner;
  const model::Location loc = ctx.locationOf(c);
  std::string subject = ctx.reader.fieldText(c, Field::subject);
  const TSNode overload = ctx.reader.child(c, Field::overload);
  if (!ts_node_is_null(overload)) {  // the overload it is the behaviour of, by its parameter types (DS-455)
    std::vector<std::string> types;
    for (uint32_t i = 0; i < ts_node_named_child_count(overload); ++i)
      types.push_back(SignatureBinder::normalised(ctx.reader.textOf(ts_node_named_child(overload, i))));
    subject = model::Identity(subject).withParameters(text::join(types, ", ")).text();
  }
  const std::string scenario = ctx.reader.fieldText(c, Field::scenario);
  model::Entity e;
  e.kind = model::Kind::Sequence;
  e.name = subject + (scenario.empty() ? "" : "/" + scenario);
  e.id = model::Identity(model::Scope(scope).qualify(subject).text()).sequence(scenario).text();  // identity = the subject (B1)
  e.scope = scope;
  e.location = loc;
  e.fields["subject"] = subject;
  if (!scenario.empty()) e.fields["scenario"] = scenario;
  fields_.bind(c, ctx, e);
  const model::Entity& declared = ctx.declare(e);
  if (!owner.empty()) ctx.relate({"contains", owner, declared.id, loc, ""});
  ctx.refer({declared.id, subject, scope, "behaviour-of", model::Kind::Class, loc, "", {}});
  annotations_.bind(c, ctx, declared.id, scope);
  // locals (and the older `participant`) are the only participants written (B3), at any depth of frames
  std::vector<TSNode> pending{c};
  while (!pending.empty()) {
    TSNode node = pending.back();
    pending.pop_back();
    const uint32_t n = ts_node_named_child_count(node);
    for (uint32_t j = 0; j < n; ++j) {
      TSNode s = ts_node_named_child(node, j);
      if (TreeReader::is(s, "loop") || TreeReader::is(s, "alt") || TreeReader::is(s, "opt") || TreeReader::is(s, "attempt") ||
          TreeReader::is(s, "handler"))
        pending.push_back(s);
      if (!TreeReader::is(s, "local") && !TreeReader::is(s, "participant")) continue;
      const std::string name = ctx.reader.fieldText(s, Field::name);
      ctx.annotate(declared.id, "local." + name,
                   types_.bind(ctx.reader.child(s, Field::type), ctx, scope, declared.id, "uses", "", {}, ctx.locationOf(s)));
    }
  }
  int order = 0, frames = 0;
  bindSteps(ctx, c, *ctx.find(declared.id), "", order, frames);
}

// ---- the walk --------------------------------------------------------------------

std::vector<std::string> SequenceBinder::kinds() const { return {"sequence"}; }

}  // namespace bind
}  // namespace ds
