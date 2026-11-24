#ifndef DRIFTSPEC_LAYOUT_ORDERER_HPP
#define DRIFTSPEC_LAYOUT_ORDERER_HPP

#include "ds/layout/Layering.hpp"

namespace ds {
namespace layout {

/// Orders each layer to cross less (DS-370, DS-392): barycentre sweeps down and up, four times, stable so the written
/// order breaks ties.
class Orderer {
 public:
  /// Reorders the rows of `layering`.
  static void order(Layering& layering);
};

}  // namespace layout
}  // namespace ds

#endif  // DRIFTSPEC_LAYOUT_ORDERER_HPP
