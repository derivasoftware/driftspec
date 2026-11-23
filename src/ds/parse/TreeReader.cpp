#include "ds/parse/TreeReader.hpp"

#include <cstring>

namespace ds {
namespace parse {

std::vector<TSNode> TreeReader::childrenNamed(TSNode node, const char* fieldName) {
  std::vector<TSNode> out;
  const uint32_t count = ts_node_child_count(node);
  for (uint32_t i = 0; i < count; ++i) {
    const char* f = ts_node_field_name_for_child(node, i);
    if (f && std::strcmp(f, fieldName) == 0) out.push_back(ts_node_child(node, i));
  }
  return out;
}

TSNode TreeReader::root() const { return ts_tree_root_node(file_.tree.get()); }

}  // namespace parse
}  // namespace ds
