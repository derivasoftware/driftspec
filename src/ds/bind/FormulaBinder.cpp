#include "ds/bind/FormulaBinder.hpp"

#include <algorithm>
#include <cstring>

#include "ds/bind/TypeTokens.hpp"
#include "ds/model/Metamodel.hpp"
#include "ds/text/Text.hpp"

namespace ds {
namespace bind {

using parse::Field;
using parse::TreeReader;
using text::join;

model::Term FormulaBinder::termOf(const parse::TreeReader& reader, TSNode node) {
  const std::string text = reader.textOf(node);
  if (TreeReader::is(node, "field_term")) return {reader.fieldText(node, Field::of), true, false, reader.fieldText(node, Field::field)};
  // name(x), id(x), kind(x): what the entity bound to x is (DS-403)
  if (TreeReader::is(node, "property_term"))
    return {reader.fieldText(node, Field::of), true, false, "", reader.fieldText(node, Field::property)};
  if (!TreeReader::is(node, "qualified_name")) return {text, false, true};  // a number or a string: a value
  // a bare name is a variable; an entity is always written qualified (the form decides, nothing is guessed)
  return {text, !model::Identity(text).isQualified(), false};
}

std::string FormulaBinder::clauseText(const parse::TreeReader& reader, TSNode clause) {
  const std::string text = reader.textOf(clause);
  const auto space = text.find_first_of(" \t\r\n");
  return space == std::string::npos ? "" : std::string(text::trimWhitespace(text.substr(space)));
}

void FormulaBinder::bind(TSNode node, const BindContext& ctx, model::Entity& out_entity) const {
  model::Formula formula;
  const auto literals = [&](TSNode clause) {
    std::vector<model::Literal> out;
    const uint32_t n = ts_node_named_child_count(clause);
    for (uint32_t i = 0; i < n; ++i) {
      TSNode lit = ts_node_named_child(clause, i);
      if (!TreeReader::is(lit, "literal")) continue;
      model::Literal l;
      l.negated = !ts_node_is_null(ctx.reader.child(lit, Field::negated));
      l.location = ctx.locationOf(lit);
      TSNode atom = ts_node_named_child(lit, 0);
      if (TreeReader::is(atom, "relation_atom")) {
        l.form = model::LiteralForm::Relation;
        l.name = ctx.reader.fieldText(atom, Field::relation);
        l.terms = {termOf(ctx.reader, ctx.reader.child(atom, Field::left)), termOf(ctx.reader, ctx.reader.child(atom, Field::right))};
      } else if (TreeReader::is(atom, "kind_atom")) {
        l.name = ctx.reader.fieldText(atom, Field::kind);
        // `m is abstract`: a word no kind is named but a flag some kind declares reads the flag (DS-403)
        const bool flag = !ctx.metamodel.specNamed(l.name) && ctx.metamodel.flagNamed(l.name);
        l.form = flag ? model::LiteralForm::Flag : model::LiteralForm::Kind;
        l.terms = {termOf(ctx.reader, ctx.reader.child(atom, Field::left))};
      } else if (TreeReader::is(atom, "in_atom")) {
        l.form = model::LiteralForm::In;
        l.terms = {termOf(ctx.reader, ctx.reader.child(atom, Field::left)), termOf(ctx.reader, ctx.reader.child(atom, Field::right))};
      } else if (TreeReader::is(atom, "compare_atom")) {
        static const std::map<std::string, model::LiteralForm> kOperators = {
            {"=", model::LiteralForm::Equal},        {"!=", model::LiteralForm::NotEqual}, {"<", model::LiteralForm::Less},
            {"<=", model::LiteralForm::LessEqual},   {">", model::LiteralForm::Greater},   {">=", model::LiteralForm::GreaterEqual},
            {"matches", model::LiteralForm::Matches}};
        l.form = kOperators.at(ctx.reader.fieldText(atom, Field::operator_));
        l.terms = {termOf(ctx.reader, ctx.reader.child(atom, Field::left)), termOf(ctx.reader, ctx.reader.child(atom, Field::right))};
      } else if (TreeReader::is(atom, "arg_atom")) {
        l.form = model::LiteralForm::Arg;
        l.name = ctx.reader.fieldText(atom, Field::parameter);
        l.terms = {termOf(ctx.reader, ctx.reader.child(atom, Field::call)), termOf(ctx.reader, ctx.reader.child(atom, Field::value))};
      } else if (TreeReader::is(atom, "call_atom")) {
        l.form = model::LiteralForm::Call;
        l.name = ctx.reader.fieldText(atom, Field::definition);
        const uint32_t count = ts_node_child_count(atom);
        for (uint32_t j = 0; j < count; ++j) {
          TSNode arg = ts_node_child(atom, j);
          if (TreeReader::is(arg, "qualified_name") && !ts_node_field_name_for_child(atom, j)) l.terms.push_back(termOf(ctx.reader, arg));
        }
      } else {
        continue;
      }
      out.push_back(l);
    }
    return out;
  };
  const auto append = [&](const char* key, const std::string& text) {
    std::string& field = out_entity.fields[key];
    field += (field.empty() ? "" : "; ") + text;
  };
  const uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    TSNode c = ts_node_child(node, i);
    if (TreeReader::is(c, "rule_for")) {
      const uint32_t n = ts_node_named_child_count(c);
      for (uint32_t j = 0; j < n; ++j) {
        TSNode r = ts_node_named_child(c, j);
        if (TreeReader::is(r, "range"))
          formula.ranges.push_back({ctx.reader.fieldText(r, Field::kind), ctx.reader.fieldText(r, Field::variable),
                                    ctx.reader.fieldText(r, Field::scope), ctx.locationOf(r)});
      }
      append("for", clauseText(ctx.reader, c));
    } else if (TreeReader::is(c, "rule_where")) {
      for (auto& l : literals(c)) formula.where.push_back(l);
      append("where", clauseText(ctx.reader, c));
    } else if (TreeReader::is(c, "rule_holds") || TreeReader::is(c, "rule_never")) {
      formula.claims.push_back(literals(c));
      formula.never = TreeReader::is(c, "rule_never");
      append(formula.never ? "never" : "holds", clauseText(ctx.reader, c));
    } else if (TreeReader::is(c, "column")) {
      TSNode what = ts_node_named_child(c, ts_node_named_child_count(c) - 1);
      model::Column column;
      column.name = ctx.reader.fieldText(c, Field::name);
      column.location = ctx.locationOf(c);
      if (TreeReader::is(what, "column_field")) {
        column.form = "field";
        column.variable = ctx.reader.fieldText(what, Field::of);
        column.field = ctx.reader.fieldText(what, Field::field);
      } else if (TreeReader::is(what, "column_related")) {
        column.form = "related";
        column.variable = ctx.reader.fieldText(what, Field::show);
        column.body = literals(what);
      } else {
        column.form = "holds";
        column.body = literals(what);
      }
      formula.columns.push_back(column);
    } else if (TreeReader::is(c, "definition_case")) {
      formula.cases.push_back(literals(c));
      append("if", clauseText(ctx.reader, c));
    } else if (TreeReader::is(c, "name") && !ts_node_field_name_for_child(node, i)) {
      formula.parameters.push_back(ctx.reader.textOf(c));  // a definition's parameters
    }
  }
  if (out_entity.kind == model::Kind::Diagram && formula.ranges.empty() && formula.columns.empty()) return;  // no table written
  ctx.state(out_entity.id, formula);
}

}  // namespace bind
}  // namespace ds
