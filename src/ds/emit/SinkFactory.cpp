#include "ds/emit/SinkFactory.hpp"

#include "ds/emit/JsonSink.hpp"

namespace ds {
namespace emit {

std::unique_ptr<ISink> SinkFactory::json(bool pretty) { return std::make_unique<JsonSink>(pretty); }

}  // namespace emit
}  // namespace ds
