#ifndef RESEARCH_SIMULATION_PANDEMIC_UTIL_SMALL_WORLD_GRAPH_H_
#define RESEARCH_SIMULATION_PANDEMIC_UTIL_SMALL_WORLD_GRAPH_H_

#include <memory>
#include <vector>

#include "absl/container/inlined_vector.h"
#include "absl/random/random.h"

namespace abesim {

class SmallWorldGraph {
 public:
  ~SmallWorldGraph() {}

  // Generates and returns a Watts–Strogatz small-world graph.
  // n : The number of nodes.
  // k : Each node is joined with its `k` nearest neighbors in a ring topology
  //     if 'k' is even, and with 'k-1' neighbors if 'k' is odd.
  // p : The probability of rewiring each edge.
  // References:
  // [1] Duncan J. Watts and Steven H. Strogatz, Collective dynamics of
  //     small-world networks, Nature, 393, pp.440 --442, 1998.
  // [2] https://en.wikipedia.org/wiki/Watts%E2%80%93Strogatz_model
  static std::unique_ptr<SmallWorldGraph> GenerateWattsStrogatzGraph(int n,
                                                                     int k,
                                                                     float p);

  int NumNodes() const { return n_; }
  int Degree() const { return k_; }
  float RewireProbability() const { return p_; }

  // Returns all the edges as int pairs where each int is a Node id.
  // Nodes are numbered from 0,...,n and edges are in topologically sorted
  // order. Should have n*k/2 edges, if k is even, and n*(k-1)/2 edges if k is
  // odd.
  std::vector<std::pair<int, int>> GetEdges() const;

 private:
  int n_;
  int k_;
  float p_;

  // undirected graph implemented as an adjacency list with edges going between
  // nodes i, j assigned to node i where i < j.
  std::vector<absl::InlinedVector<int, 4>> graph_;
  // Store degrees for performance.
  std::vector<int> degrees_;

  SmallWorldGraph() = default;
  // Disallow copy and assign.
  SmallWorldGraph(const SmallWorldGraph&) = delete;
  SmallWorldGraph& operator=(const SmallWorldGraph&) = delete;

  absl::BitGen bitgen_;

  void AddEdge(int i, int j);
  void AddNode(int i);
  int Degree(int i) const;
  bool HasEdge(int i, int j) const;
  void RemoveEdge(int i, int j);
};

}  // namespace abesim

#endif  // RESEARCH_SIMULATION_PANDEMIC_UTIL_SMALL_WORLD_GRAPH_H_
