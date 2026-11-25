#include "ds/emit/SinkFactory.hpp"

#include "ds/emit/JsonSink.hpp"
#include "ds/emit/NotImplemented.hpp"
#include "ds/emit/TextSink.hpp"

namespace ds {
namespace emit {

std::unique_ptr<ISink> SinkFactory::of(const std::string& technology, bool pretty) {
  if (technology == "json") return json(pretty);
  if (technology == "text") return text();
  throw NotImplemented("", technology, names());
}

std::unique_ptr<ISink> SinkFactory::json(bool pretty) { return std::make_unique<JsonSink>(pretty); }

std::unique_ptr<ISink> SinkFactory::text() { return std::make_unique<TextSink>(); }

std::vector<std::string> SinkFactory::names() { return {"json", "text"}; }

}  // namespace emit
}  // namespace ds
