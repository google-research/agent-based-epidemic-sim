#include "agent_based_epidemic_sim/core/small_world_graph.h"

#include <type_traits>
#include <utility>
#include <vector>

#include "absl/random/distributions.h"
#include "agent_based_epidemic_sim/core/random.h"
#include "agent_based_epidemic_sim/port/logging.h"

namespace abesim {

bool SmallWorldGraph::HasEdge(int i, int j) const {
  if (i > j) std::swap(i, j);
  if (graph_[i].empty()) return false;
  return std::find(graph_[i].begin(), graph_[i].end(), j) != graph_[i].end();
}

void SmallWorldGraph::AddEdge(int i, int j) {
  if (i > j) std::swap(i, j);
  if (HasEdge(i, j)) return;
  graph_[i].push_back(j);
  degrees_[i]++;
  degrees_[j]++;
}

void SmallWorldGraph::AddNode(int i) {
  if (i > graph_.size()) graph_.resize(i + 1);
  if (i > degrees_.size()) degrees_.resize(i + 1);
}

void SmallWorldGraph::RemoveEdge(int i, int j) {
  if (i > j) std::swap(i, j);
  if (!HasEdge(i, j)) return;
  graph_[i].erase(std::remove(graph_[i].begin(), graph_[i].end(), j),
                  graph_[i].end());
  degrees_[i]--;
  degrees_[j]--;
}

int SmallWorldGraph::Degree(int i) const { return degrees_[i]; }

std::vector<std::pair<int, int>> SmallWorldGraph::GetEdges() const {
  std::vector<std::pair<int, int>> edges;
  CHECK(graph_.size() == n_);
  for (int i = 0; i < n_; ++i) {
    for (int j = i + 1; j < n_; ++j) {
      if (HasEdge(i, j)) {
        edges.push_back(std::make_pair(i, j));
      }
    }
  }
  CHECK(edges.size() == n_ * (k_ / 2));
  return edges;
}

// First create a ring over 'n' nodes.  Then each node in the ring is joined to
// its 'k' nearest neighbors (or 'k - 1' neighbors if 'k' is odd). Then
// shortcuts are created by replacing some edges as follows: for each
// edge (u, v) in the underlying n-ring, with probability 'p' replace it with a
// new edge (u, w) with uniformly random choice of existing node 'w'.
/* static */ std::unique_ptr<SmallWorldGraph>
SmallWorldGraph::GenerateWattsStrogatzGraph(int n, int k, float p) {
  // Validate inputs
  // 0 <= p <= 1
  CHECK(p >= 0) << "'p' must be >= 0";
  CHECK(p <= 1) << "'p' must be <= 1";
  // n >> k >> ln(n) >> 1
  CHECK(k >= 2) << "'k' must be >= 2";
  CHECK(n > k) << "'n' must be >= 'k'";
  auto ws = absl::WrapUnique(new SmallWorldGraph());
  ws->n_ = n;
  ws->k_ = k;
  ws->p_ = p;

  // 1. Create nodes labeled 0,...n-1
  ws->AddNode(n - 1);

  // 2. Create ring lattice. There is an edge (u,v) iff
  //    0 < |u-v| mod (n - 1 - k/2) <= k/2
  for (int u = 0; u < n; ++u) {
    // Add k/2 edges to the right for node 'u'.
    for (int v = u + 1; v <= u + k / 2; ++v) {
      ws->AddEdge(u, v % n);
    }
    // Add k/2 edges to the left for node 'u'.
    for (int v = u - 1; v < u - 1 - k / 2; --v) {
      ws->AddEdge(u, v % n);
    }
  }

  // 3. Rewire an edge with probability 'p'.
  // For every node u = 0,..., n-1, take every edge connecting 'u' to its k/2
  // rightmost neighbors, that is, every edge (u, v mod n) with
  // u < v <= u + k/2, and rewire it with probability 'p'. Rewiring is done by
  // replacing (u, v mod N) with (u, w) where w is chosen uniformly at random
  // from all possible nodes while avoiding self-loops (w != u) and link
  // duplication (there is no edge (u, w') with w'=w at this point in the
  // algorithm).
  absl::BitGenRef gen = GetBitGen();
  for (int u = 0; u < n; ++u) {
    if (ws->Degree(u) >= n - 1) {
      // u is already fully connected. Skip this rewiring
      continue;
    }
    for (int v = u + 1; v <= u + k / 2; ++v) {
      // Check if there is an edge (u, v) since could have been already rewired.
      if (!ws->HasEdge(u, v % n)) continue;
      // Rewire with probability 'p'
      if (absl::Bernoulli(gen, p)) {
        int w = u;
        // Enforce no self-loops or duplicate edges. There is at least one
        // available node.
        while (w == u || ws->HasEdge(u, w)) {
          // Generate a uniform value between [0, n-1].
          w = absl::Uniform<int>(absl::IntervalClosedClosed, gen, 0, n - 1);
        }
        // Rewire the edges.
        ws->RemoveEdge(u, v % n);
        ws->AddEdge(u, w);
      }
    }
  }
  CHECK(ws->graph_.size() == ws->n_);

  return ws;
}

}  // namespace abesim
