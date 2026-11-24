#include "ds/api/Api.hpp"

#include <nlohmann/json.hpp>
#include <set>
#include <vector>

#include "ds/bind/Binder.hpp"
#include "ds/check/Checker.hpp"
#include "ds/check/Report.hpp"
#include "ds/check/Steps.hpp"
#include "ds/format/TextFormats.hpp"
#include "ds/model/Identity.hpp"
#include "ds/model/Model.hpp"
#include "ds/parse/Parser.hpp"

namespace ds {
namespace api {

namespace {

/// The block a file leaves open, read from its indentation (CST-00007-1: braces enclose one thing per line, a
/// closing brace at the indentation of the line that opened it): the first line that opens a block and is
/// followed, before its brace, by a line written at its own indentation or outside it. Empty when every block
/// closes where it should, or when the file is not written that way (DS-304).
struct Unclosed {
  int line = 0;   ///< the line that opens the block, 1-based
  int until = 0;  ///< the first line written outside it
  std::string opener;
};

Unclosed unclosedBlock(const std::string& text) {
  struct Open {
    std::size_t indent;
    int line;
    std::string opener;
  };
  std::vector<Open> open;
  std::size_t from = 0;
  for (int n = 1; from <= text.size(); ++n) {
    std::size_t to = text.find('\n', from);
    if (to == std::string::npos) to = text.size();
    std::string line = text.substr(from, to - from);
    from = to + 1;
    const auto comment = line.find("//");
    if (comment != std::string::npos) line.erase(comment);
    const std::size_t indent = line.find_first_not_of(' ');
    if (indent == std::string::npos) continue;
    const std::size_t end = line.find_last_not_of(" \t\r");
    const std::string body = line.substr(indent, end - indent + 1);
    if (body[0] == '}') {
      if (open.empty() || open.back().indent != indent) return {};  // not written the way the heuristic reads
      open.pop_back();
    } else if (!open.empty() && indent <= open.back().indent) {
      return {open.back().line, n, open.back().opener};
    }
    if (body.back() == '{' && body.size() > 1) open.push_back({indent, n, body});
  }
  return {};
}

}  // namespace

check::Selection Api::stepsFor(Output output, const model::Model& bound, const std::string& diagram) {
  if (output != Output::Drawing) return check::Selection::every();  // findings, the model, answers and the viewer read all
  // a drawing shows relations, calls and couplings, and stereotypes; it reads no finding, coverage, escape or breach
  const check::Selection drawing = check::Selection::of({check::Step::Linker, check::Step::Behaviour, check::Step::Derivation});
  // a table written as rows and columns is the answers of its rules (DS-268): they are computed for it
  return bound.formulas().count(model::Identity::ofDiagram(diagram).text()) ? drawing.with(check::Step::Rules) : drawing;
}

model::Model Api::load(const std::vector<parse::ParsedFile>& files, Output output, const std::string& diagram) const {
  model::Model model(metamodel_);
  bind::Binder binder(model, metamodel_);
  std::set<std::string> broken;
  for (const auto& parsed : files) {
    if (parsed.hasSyntaxErrors) {
      broken.insert(parsed.path);
      const Unclosed u = unclosedBlock(parsed.text);
      if (u.line > 0 && u.line <= parsed.errorLine) {  // a lost brace: say which block, where it opens
        model.report(metamodel_.finding(
            model::Code::E_SYNTAX,
            "the file does not parse: `" + u.opener + "` is not closed; line " + std::to_string(u.until) + " is written outside it",
            {parsed.path, u.line, 1}));
      } else {
        const std::string detail = parsed.errorDetail.empty() ? "" : ": " + parsed.errorDetail;
        model.report(metamodel_.finding(model::Code::E_SYNTAX, "the file does not parse" + detail,
                                        {parsed.path, parsed.errorLine, parsed.errorColumn}));
      }
    }
    binder.bind(parsed);
  }
  const auto steps = check::Steps::of(stepsFor(output, model, diagram));  // only the steps the output reads (DS-376)
  check::Checker(model, metamodel_, steps).run(binder.notes());
  // what a file that does not parse leaves out of the model comes back as findings that only echo the cut:
  // the rest of that file, and every name now declared nowhere. They go until the file parses (DS-304)
  model.withdrawEchoes(broken);
  model.sortFindings();
  return model;
}

std::string Api::report(const model::Model& model, bool pretty) const {
  format::FormatOptions options;  // the one report of a check (DS-120), as JSON (DS-126)
  options.pretty = pretty;
  return format::ReportJson().write(check::Report::of(model, metamodel_), options);
}

// the text functions of the command line's contract: a workspace loaded for one question (DS-173)
std::string Api::check(const Files& files) const { return Workspace(metamodel_, files, Output::Findings).report(); }

std::string Api::exportModel(const Files& files) const { return Workspace(metamodel_, files, Output::Model).exportModel(); }

std::string Api::render(const Files& files, const std::string& diagram, const std::string& format, int width) const {
  // svg, the IR itself, or text for anything else; loaded with the steps the drawing reads (DS-376)
  return Workspace(metamodel_, files, Output::Drawing, diagram).render(diagram, format == "svg" || format == "ir" ? format : "text", width);
}

std::string Api::page(const Files& files, const std::string& title, const std::vector<std::string>& focus) const {
  return Workspace(metamodel_, files, Output::Viewer).page(title, focus);
}

std::string Api::expectedAt(const std::string& text, int line, int column) {
  parse::Parser parser;
  return nlohmann::json(parser.expectedAt(text, line, column)).dump();
}

}  // namespace api
}  // namespace ds
