#include "ds/format/TextFormats.hpp"

namespace ds {
namespace format {

std::string AnswersText::name() const { return "text"; }

std::string AnswersText::write(const std::vector<query::Answer>& document, const FormatOptions&, emit::ISink&) const {
  std::string out;
  for (const auto& a : document) out += a.kind + "  " + a.id + (a.detail.empty() ? "" : "  " + a.detail) + "\n";
  return out;
}

std::string ReportText::name() const { return "text"; }

std::string ReportText::write(const check::Report& document, const FormatOptions& options, emit::ISink&) const {
  std::string out;
  for (const auto& f : document.findings)
    out += f.location.file + ":" + std::to_string(f.location.line) + ":" + std::to_string(f.location.column) + "  " + f.code + "  " +
           f.message + "\n";
  // what is not yet written is no finding, but a check that says nothing of it reads as done (DS-431): its count ends the
  // line, and the next says where it is listed; with none open, the report is as it was
  const std::size_t open = document.coverage.size();
  out += std::to_string(document.entities) + " entities · " + std::to_string(document.expected) + " expected · " +
         std::to_string(document.relations) + " relations · " + std::to_string(document.findings.size()) + " findings" +
         (open ? " · " + std::to_string(open) + " open" : "") + "\n";
  if (open)
    out += std::to_string(open) + (open == 1 ? " open item" : " open items") + " of coverage: ds query " +
           (options.title.empty() ? "<workspace>" : options.title) + " coverage lists them\n";
  return out;
}

}  // namespace format
}  // namespace ds
