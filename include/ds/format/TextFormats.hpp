#ifndef DRIFTSPEC_FORMAT_TEXTFORMATS_HPP
#define DRIFTSPEC_FORMAT_TEXTFORMATS_HPP

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/emit/Sink.hpp"
#include "ds/format/Format.hpp"
#include "ds/query/Query.hpp"

namespace ds {
namespace format {

/// The answers of a query as lines: kind  identity  detail (ds query).
class AnswersText : public IFormat<std::vector<query::Answer>> {
 public:
  std::string name() const override;
  std::string write(const std::vector<query::Answer>& document, const FormatOptions& options, emit::ISink& out_sink) const override;
};

/// The answers of a query as a JSON array of the same three fields (ds query --json).
class AnswersJson : public IFormat<std::vector<query::Answer>> {
 public:
  std::string name() const override;
  std::string write(const std::vector<query::Answer>& document, const FormatOptions& options, emit::ISink& out_sink) const override;
};

/// The one place that names the formats of a query's answers.
std::map<std::string, std::unique_ptr<IFormat<std::vector<query::Answer>>>> answerFormats();

/// A check as lines: each finding where it is, then the counts of the model (ds check).
class ReportText : public IFormat<check::Report> {
 public:
  std::string name() const override;
  std::string write(const check::Report& document, const FormatOptions& options, emit::ISink& out_sink) const override;
};

/// A check as JSON: the findings and the counts, the one shape every caller reads (ds check --json, the API).
class ReportJson : public IFormat<check::Report> {
 public:
  std::string name() const override;
  /// Written by the report's encoder, and taken whole from the sink.
  std::string write(const check::Report& document, const FormatOptions& options, emit::ISink& out_sink) const override;
};

/// The one place that names the formats of a check's report.
std::map<std::string, std::unique_ptr<IFormat<check::Report>>> reportFormats();

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_TEXTFORMATS_HPP
