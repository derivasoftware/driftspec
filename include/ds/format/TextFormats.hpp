#ifndef DRIFTSPEC_FORMAT_TEXTFORMATS_HPP
#define DRIFTSPEC_FORMAT_TEXTFORMATS_HPP

#include <string>
#include <vector>

#include "ds/check/Report.hpp"
#include "ds/emit/Sink.hpp"
#include "ds/format/Format.hpp"
#include "ds/query/Query.hpp"

namespace ds {
namespace format {

/// The writers of the answers and the report as text, until text is a sink (DS-399): FormatFactory names them.

/// The answers of a query as lines: kind  identity  detail (ds query).
class AnswersText : public IFormat<std::vector<query::Answer>> {
 public:
  std::string name() const override;
  std::string write(const std::vector<query::Answer>& document, const FormatOptions& options, emit::ISink& out_sink) const override;
};

/// A check as lines: each finding where it is, then the counts of the model (ds check).
class ReportText : public IFormat<check::Report> {
 public:
  std::string name() const override;
  std::string write(const check::Report& document, const FormatOptions& options, emit::ISink& out_sink) const override;
};

}  // namespace format
}  // namespace ds

#endif  // DRIFTSPEC_FORMAT_TEXTFORMATS_HPP
