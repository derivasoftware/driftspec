#include "ds/layout/Orderer.hpp"

#include <algorithm>
#include <utility>
#include <vector>

namespace ds {
namespace layout {

void Orderer::order(Layering& L) {
  const int depth = L.depth;
  const auto& succA = L.succA;
  const auto& predA = L.predA;
  const int total = L.total;
  auto& rows = L.rows;
  // crossing reduction: barycenter sweeps, stable so the model order breaks ties
  std::vector<double> pos(total);
  auto positions = [&]() {
    for (const auto& row : rows) {
      for (size_t i = 0; i < row.size(); ++i) pos[row[i]] = static_cast<double>(i);
    }
  };
  auto sweep = [&](const std::vector<std::vector<int>>& neighbours, bool down) {
    positions();
    for (int l = down ? 1 : depth - 1; down ? l <= depth : l >= 0; l += down ? 1 : -1) {
      std::vector<int>& row = rows[l];
      std::vector<std::pair<double, int>> keyed;
      for (int v : row) {
        double bary = pos[v];
        if (!neighbours[v].empty()) {
          bary = 0;
          for (int u : neighbours[v]) bary += pos[u];
          bary /= static_cast<double>(neighbours[v].size());
        }
        keyed.push_back({bary, v});
      }
      std::stable_sort(keyed.begin(), keyed.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
      for (size_t i = 0; i < row.size(); ++i) row[i] = keyed[i].second;
      positions();
    }
  };
  for (int it = 0; it < 4; ++it) {
    sweep(predA, true);
    sweep(succA, false);
  }
}

}  // namespace layout
}  // namespace ds
