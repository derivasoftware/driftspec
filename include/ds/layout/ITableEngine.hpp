#ifndef DRIFTSPEC_LAYOUT_ITABLEENGINE_HPP
#define DRIFTSPEC_LAYOUT_ITABLEENGINE_HPP

#include "ds/layout/TableGraph.hpp"
#include "ds/layout/TablePlacement.hpp"

namespace ds {
namespace layout {

/// A table engine (DS-464): a table graph in, where its columns, rows and lines land out.
class ITableEngine {
 public:
  virtual ~ITableEngine() = default;
  virtual TablePlacement place(const TableGraph& graph) const = 0;
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ITABLEENGINE_HPP
