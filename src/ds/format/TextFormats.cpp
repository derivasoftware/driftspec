#include "ds/format/TextFormats.hpp"

namespace ds {
namespace format {

std::string AnswersText::name() const { return "text"; }

std::string AnswersText::write(const std::vector<query::Answer>& document, const FormatOptions&, emit::ISink&) const {
  std::string out;
  for (const auto& a : document) out += a.kind + "  " + a.id + (a.detail.empty() ? "" : "  " + a.detail) + "\n";
  return out;
}

std::string AnswersJson::name() const { return "json"; }

std::string AnswersJson::write(const std::vector<query::Answer>& document, const FormatOptions&, emit::ISink& out_sink) const {
  out_sink.beginList();
  for (const auto& a : document)
    out_sink.beginRecord().field("detail").value(a.detail).field("id").value(a.id).field("kind").value(a.kind).endRecord();
  out_sink.endList();
  return out_sink.result();
}

std::map<std::string, std::unique_ptr<IFormat<std::vector<query::Answer>>>> answerFormats() {
  std::map<std::string, std::unique_ptr<IFormat<std::vector<query::Answer>>>> out;
  out["text"] = std::make_unique<AnswersText>();
  out["json"] = std::make_unique<AnswersJson>();
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

std::string ReportJson::name() const { return "json"; }

void ReportJson::findings(const check::Report& document, const std::vector<model::Finding>& findings, emit::ISink& out_sink) {
  out_sink.beginList();
  for (const auto& f : findings) {
    out_sink.beginRecord().field("code").value(f.code).field("location").beginRecord();
    out_sink.field("column").integer(f.location.column).field("file").value(f.location.file).field("line").integer(f.location.line);
    out_sink.endRecord().field("message").value(f.message).field("severity").value(document.severityOf(f.severity).name).endRecord();
  }
  out_sink.endList();
}

void ReportJson::describe(const check::Report& document, emit::ISink& out_sink) const {
  out_sink.beginRecord().field("coverage");
  findings(document, document.coverage, out_sink);
  out_sink.field("entities").integer(document.entities).field("errors").integer(document.errors);
  out_sink.field("expected").integer(document.expected).field("findings");
  findings(document, document.findings, out_sink);
  out_sink.field("relations").integer(document.relations).endRecord();
}

std::string ReportJson::write(const check::Report& document, const FormatOptions&, emit::ISink& out_sink) const {
  describe(document, out_sink);
  return out_sink.result();
}

std::map<std::string, std::unique_ptr<IFormat<check::Report>>> reportFormats() {
  std::map<std::string, std::unique_ptr<IFormat<check::Report>>> out;
  out["text"] = std::make_unique<ReportText>();
  out["json"] = std::make_unique<ReportJson>();
  return out;
}

}  // namespace format
}  // namespace ds
