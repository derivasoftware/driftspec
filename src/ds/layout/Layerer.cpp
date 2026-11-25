#include "ds/layout/Layerer.hpp"

#include <functional>
#include <set>
#include <utility>

namespace ds {
namespace layout {

Layerer::Layerer(const Spacing& spacing) : spacing_(spacing) {}

Layering Layerer::of(const Component& c, const std::vector<Rect>& rel) const {
  const Spacing& S = spacing_;

  const int real = static_cast<int>(c.nodes.size());
  std::map<int, int> local;
  for (int i = 0; i < real; ++i) local[c.nodes[i]] = i;
  // sizes: pseudo nodes take no room
  std::vector<int> widths, heights;
  for (int k : c.nodes) {
    widths.push_back(k < 0 ? 0 : rel[k].w);
    heights.push_back(k < 0 ? 0 : rel[k].h);
  }
  // break cycles: an edge that closes a cycle in a DFS over model order is reversed
  std::vector<std::set<int>> adj(real);
  for (const auto& e : c.edges) adj[local[e.upper]].insert(local[e.lower]);
  std::vector<int> state(real, 0);
  std::set<std::pair<int, int>> dag;
  std::function<void(int)> dfs = [&](int v) {
    state[v] = 1;
    for (int w : adj[v]) {
      if (w == v) continue;
      if (state[w] == 1) {
        dag.insert({w, v});
      } else {
        dag.insert({v, w});
        if (state[w] == 0) dfs(w);
      }
    }
    state[v] = 2;
  };
  for (int v = 0; v < real; ++v) {
    if (state[v] == 0) dfs(v);
  }
  // layers by longest path from the sources
  std::vector<std::vector<int>> succ(real), pred(real);
  for (const auto& e : dag) {
    succ[e.first].push_back(e.second);
    pred[e.second].push_back(e.first);
  }
  std::vector<int> layer(real, 0), indeg(real, 0);
  for (int v = 0; v < real; ++v) indeg[v] = static_cast<int>(pred[v].size());
  std::vector<int> queue;
  for (int v = 0; v < real; ++v) {
    if (indeg[v] == 0) queue.push_back(v);
  }
  for (size_t head = 0; head < queue.size(); ++head) {
    const int v = queue[head];
    for (int w : succ[v]) {
      layer[w] = std::max(layer[w], layer[v] + 1);
      if (--indeg[w] == 0) queue.push_back(w);
    }
  }
  int depth = 0;
  for (int v = 0; v < real; ++v) depth = std::max(depth, layer[v]);
  // long edges get a dummy node in every layer they cross, so they are ordered and placed like nodes
  std::vector<std::vector<int>> succA(real), predA(real);
  std::map<std::pair<int, int>, std::vector<int>> chain;  // (upper, lower) → dummies, top to bottom
  int total = real;
  for (const auto& e : dag) {
    int prev = e.first;
    std::vector<int> dummies;
    for (int l = layer[e.first] + 1; l < layer[e.second]; ++l) {
      const int d = total++;
      succA.push_back({});
      predA.push_back({});
      layer.push_back(l);
      widths.push_back(S.channel);  // a channel one character wide
      heights.push_back(0);
      succA[prev].push_back(d);
      predA[d].push_back(prev);
      dummies.push_back(d);
      prev = d;
    }
    succA[prev].push_back(e.second);
    predA[e.second].push_back(prev);
    chain[e] = dummies;
  }
  std::vector<std::vector<int>> rows(depth + 1);
  for (int v = 0; v < total; ++v) rows[layer[v]].push_back(v);  // model order first, dummies after
  Layering L;
  L.real = real;
  L.local = std::move(local);
  L.widths = std::move(widths);
  L.heights = std::move(heights);
  L.layer = std::move(layer);
  L.depth = depth;
  L.succA = std::move(succA);
  L.predA = std::move(predA);
  L.chain = std::move(chain);
  L.total = total;
  L.rows = std::move(rows);
  return L;
}

}  // namespace layout
}  // namespace ds
