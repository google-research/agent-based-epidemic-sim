#include "agent_based_epidemic_sim/core/small_world_graph.h"

#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace abesim {
namespace {

using ::testing::Pair;
using ::testing::UnorderedElementsAre;

TEST(SmallWorldGraph, GenerateWattsStrogatzGraphInvalidInputsEvenK) {
  EXPECT_DEATH(SmallWorldGraph::GenerateWattsStrogatzGraph(4, 2, -0.1),
               "'p' must be >= 0");
  EXPECT_DEATH(SmallWorldGraph::GenerateWattsStrogatzGraph(4, 2, 1.1),
               "'p' must be <= 1");
  EXPECT_DEATH(SmallWorldGraph::GenerateWattsStrogatzGraph(4, 1, 0.5),
               "'k' must be >= 2");
  EXPECT_DEATH(SmallWorldGraph::GenerateWattsStrogatzGraph(2, 2, 0.5),
               "'n' must be >= 'k'");
}

TEST(SmallWorldGraph, GenerateWattsStrogatzGraphInvalidInputsOddK) {
  EXPECT_DEATH(SmallWorldGraph::GenerateWattsStrogatzGraph(5, 3, -0.1),
               "'p' must be >= 0");
  EXPECT_DEATH(SmallWorldGraph::GenerateWattsStrogatzGraph(5, 3, 1.1),
               "'p' must be <= 1");
  EXPECT_DEATH(SmallWorldGraph::GenerateWattsStrogatzGraph(5, 1, 0.5),
               "'k' must be >= 2");
  EXPECT_DEATH(SmallWorldGraph::GenerateWattsStrogatzGraph(3, 3, 0.5),
               "'n' must be >= 'k'");
}

TEST(SmallWorldGraph, GenerateWattsStrogatzGraphP0EvenK) {
  std::unique_ptr<SmallWorldGraph> ws =
      SmallWorldGraph::GenerateWattsStrogatzGraph(/*n=*/4, /*k=*/2, /*p=*/0.0);
  std::vector<std::pair<int, int>> edges = ws->GetEdges();
  EXPECT_EQ(ws->NumNodes(), 4);
  EXPECT_EQ(ws->Degree(), 2);
  EXPECT_FLOAT_EQ(ws->RewireProbability(), 0.0);
  EXPECT_EQ(edges.size(), 4);
  EXPECT_THAT(edges, UnorderedElementsAre(Pair(0, 1), Pair(0, 3), Pair(1, 2),
                                          Pair(2, 3)));
}

TEST(SmallWorldGraph, GenerateWattsStrogatzGraphP1EvenK) {
  std::unique_ptr<SmallWorldGraph> ws =
      SmallWorldGraph::GenerateWattsStrogatzGraph(/*n=*/4, /*k=*/2, /*p=*/1.0);
  std::vector<std::pair<int, int>> edges = ws->GetEdges();
  EXPECT_EQ(ws->NumNodes(), 4);
  EXPECT_EQ(ws->Degree(), 2);
  EXPECT_FLOAT_EQ(ws->RewireProbability(), 1.0);
  EXPECT_EQ(edges.size(), 4);
  // TODO: Mock BitGen to test rewiring.
}

TEST(SmallWorldGraph, GenerateWattsStrogatzGraphPhalfEvenK) {
  std::unique_ptr<SmallWorldGraph> ws =
      SmallWorldGraph::GenerateWattsStrogatzGraph(/*n=*/4, /*k=*/2, /*p=*/0.5);
  std::vector<std::pair<int, int>> edges = ws->GetEdges();
  EXPECT_EQ(ws->NumNodes(), 4);
  EXPECT_EQ(ws->Degree(), 2);
  EXPECT_FLOAT_EQ(ws->RewireProbability(), 0.5);
  EXPECT_EQ(edges.size(), 4);
}

TEST(SmallWorldGraph, GenerateWattsStrogatzGraphP0OddK) {
  std::unique_ptr<SmallWorldGraph> ws =
      SmallWorldGraph::GenerateWattsStrogatzGraph(/*n=*/5, /*k=*/3, /*p=*/0.0);
  std::vector<std::pair<int, int>> edges = ws->GetEdges();
  EXPECT_EQ(ws->NumNodes(), 5);
  EXPECT_EQ(ws->Degree(), 3);
  EXPECT_FLOAT_EQ(ws->RewireProbability(), 0.0);
  EXPECT_EQ(edges.size(), 5);
  EXPECT_THAT(edges, UnorderedElementsAre(Pair(0, 1), Pair(0, 4), Pair(1, 2),
                                          Pair(2, 3), Pair(3, 4)));
}

TEST(SmallWorldGraph, GenerateWattsStrogatzGraphP1OddK) {
  std::unique_ptr<SmallWorldGraph> ws =
      SmallWorldGraph::GenerateWattsStrogatzGraph(/*n=*/5, /*k=*/3,
                                                  /*p=*/1.0);
  std::vector<std::pair<int, int>> edges = ws->GetEdges();
  EXPECT_EQ(ws->NumNodes(), 5);
  EXPECT_EQ(ws->Degree(), 3);
  EXPECT_FLOAT_EQ(ws->RewireProbability(), 1.0);
  EXPECT_EQ(edges.size(), 5);
}

TEST(SmallWorldGraph, GenerateWattsStrogatzGraphPhalfOddK) {
  std::unique_ptr<SmallWorldGraph> ws =
      SmallWorldGraph::GenerateWattsStrogatzGraph(/*n=*/5, /*k=*/3,
                                                  /*p=*/0.5);
  std::vector<std::pair<int, int>> edges = ws->GetEdges();
  EXPECT_EQ(ws->NumNodes(), 5);
  EXPECT_EQ(ws->Degree(), 3);
  EXPECT_FLOAT_EQ(ws->RewireProbability(), 0.5);
  EXPECT_EQ(edges.size(), 5);
}

}  // namespace
}  // namespace abesim
