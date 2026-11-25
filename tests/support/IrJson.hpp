#ifndef DRIFTSPEC_TESTS_SUPPORT_IRJSON_HPP
#define DRIFTSPEC_TESTS_SUPPORT_IRJSON_HPP

#include <optional>
#include <string>

#include "ds/emit/JsonSink.hpp"
#include "ds/format/DescriptionFactory.hpp"
#include "ds/render/IrExport.hpp"

namespace ds {
namespace test {

/// The render-IR of `diagramId`, as a door writes it (DS-418): the document IrExport makes, described into a compact
/// JSON sink whose key-order check is on; empty when the model declares no such diagram.
inline std::string irJson(const ds::render::IrExport& exporter, const std::string& diagramId) {
  const std::optional<ds::render::IrDocument> document = exporter.document(diagramId);
  if (!document) return "";
  ds::emit::JsonSink sink(false, true);
  ds::format::DescriptionFactory::ir(*document)->describe(sink);
  return sink.result();
}

}  // namespace test
}  // namespace ds

#endif  // DRIFTSPEC_TESTS_SUPPORT_IRJSON_HPP
