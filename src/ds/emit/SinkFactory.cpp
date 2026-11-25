#include "ds/emit/SinkFactory.hpp"

#include "ds/emit/JsonSink.hpp"
#include "ds/emit/NotImplemented.hpp"

namespace ds {
namespace emit {

std::unique_ptr<ISink> SinkFactory::of(const std::string& technology, bool pretty) {
  if (technology == "json") return json(pretty);
  throw NotImplemented("", technology, names());
}

std::unique_ptr<ISink> SinkFactory::json(bool pretty) { return std::make_unique<JsonSink>(pretty); }

std::vector<std::string> SinkFactory::names() { return {"json"}; }

}  // namespace emit
}  // namespace ds
