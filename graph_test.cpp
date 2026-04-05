#include "Graph.h"
#include "GraphGenerator.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {

int EdgeCount(const Graph &graph) {
    return graph.NumEdges();
}

bool HasSelfLoop(const Graph &graph) {
    for (const Edge &edge : graph.edges) {
        if (edge.head == edge.tail) {
            return true;
        }
    }
    return false;
}

Graph ExtractSubgraph(
    const Graph &graph, const std::vector<bool> &edge_partition,
    bool use_one_edges) {
    Graph subgraph(graph.NumVertices());
    for (int edge_index = 0; edge_index < graph.NumEdges(); ++edge_index) {
        if (edge_partition[edge_index] != use_one_edges) {
            continue;
        }
        const Edge &edge = graph.edges[edge_index];
        subgraph.AddEdge(edge.head, edge.tail);
    }
    return subgraph;
}

bool IsValidDecomposition(
    const Graph &graph, const std::vector<bool> &edge_partition,
    int connectivity_first, int connectivity_second) {
    if (static_cast<int>(edge_partition.size()) != graph.NumEdges()) {
        return false;
    }

    const Graph first_subgraph = ExtractSubgraph(graph, edge_partition, false);
    const Graph second_subgraph = ExtractSubgraph(graph, edge_partition, true);
    return first_subgraph.Connectivity() >= connectivity_first &&
           second_subgraph.Connectivity() >= connectivity_second;
}

std::optional<std::vector<bool> > BruteForceDecomposition(
    const Graph &graph, int connectivity_first, int connectivity_second) {
    if (graph.NumEdges() >= static_cast<int>(sizeof(std::uint64_t) * 8)) {
        throw std::invalid_argument("Brute-force decomposition is only for small graphs");
    }

    const std::uint64_t num_partitions = std::uint64_t{1} << graph.NumEdges();
    for (std::uint64_t mask = 0; mask < num_partitions; ++mask) {
        std::vector<bool> edge_partition(graph.NumEdges(), false);
        for (int edge_index = 0; edge_index < graph.NumEdges(); ++edge_index) {
            edge_partition[edge_index] =
                ((mask >> edge_index) & std::uint64_t{1}) != 0;
        }
        if (IsValidDecomposition(
                graph, edge_partition, connectivity_first, connectivity_second)) {
            return edge_partition;
        }
    }
    return std::nullopt;
}

int BruteForceConnectivity(const Graph &graph) {
    const int num_vertices = graph.NumVertices();
    if (num_vertices <= 1) {
        return 0;
    }

    int best_cut = std::numeric_limits<int>::max();
    const int mask_limit = 1 << num_vertices;
    for (int mask = 1; mask < mask_limit - 1; ++mask) {
        if ((mask & 1) == 0) {
            continue;
        }

        int cut_size = 0;
        for (const Edge &edge : graph.edges) {
            const bool head_in_cut = (mask & (1 << edge.head)) != 0;
            const bool tail_in_cut = (mask & (1 << edge.tail)) != 0;
            if (head_in_cut != tail_in_cut) {
                ++cut_size;
            }
        }
        best_cut = std::min(best_cut, cut_size);
    }

    return best_cut == std::numeric_limits<int>::max() ? 0 : best_cut;
}

void TestGraphConstructsFromEdges() {
    Graph graph({{0, 1}, {1, 2}, {1, 2}});

    assert(graph.NumVertices() == 3);
    assert(graph.NumEdges() == 3);
    assert(graph.edges[0].head == 0 && graph.edges[0].tail == 1);
    assert(graph.edges[1].head == 1 && graph.edges[1].tail == 2);
    assert(graph.edges[2].head == 1 && graph.edges[2].tail == 2);
    assert(graph.adj_list[0] == std::vector<int>({0}));
    assert(graph.adj_list[1] == std::vector<int>({0, 1, 2}));
    assert(graph.adj_list[2] == std::vector<int>({1, 2}));
}

void TestGraphRejectsNegativeEndpoints() {
    bool threw = false;
    try {
        Graph graph({{-1, 0}});
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);
}

void TestGraphRejectsLoops() {
    bool threw = false;
    try {
        Graph graph({{0, 0}});
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);
}

void TestConnectivityOnKnownGraphs() {
    Graph path({{0, 1}, {1, 2}, {2, 3}});
    assert(path.Connectivity() == 1);

    Graph cycle({{0, 1}, {1, 2}, {2, 3}, {3, 0}});
    assert(cycle.Connectivity() == 2);

    Graph parallel_edges({{0, 1}, {0, 1}, {0, 1}});
    assert(parallel_edges.Connectivity() == 3);

    Graph disconnected(4);
    disconnected.AddEdge(0, 1);
    disconnected.AddEdge(2, 3);
    assert(disconnected.Connectivity() == 0);
}

void TestRandomGraphGenerationMatchesReferenceSampling() {
    constexpr std::uint32_t kSeed = 123456789u;
    constexpr int kNumVertices = 6;
    constexpr int kNumEdges = 20;

    GraphGenerator generator(kSeed);
    const std::unique_ptr<Graph> graph =
        generator.GenerateRandomGraph(kNumVertices, kNumEdges);

    std::mt19937 reference_rng(kSeed);
    std::uniform_int_distribution<int> vertex_distribution(0, kNumVertices - 1);
    std::vector<std::tuple<int, int> > sampled_edges;
    sampled_edges.reserve(kNumEdges);
    for (int i = 0; i < kNumEdges; ++i) {
        const int u = vertex_distribution(reference_rng);
        int v = vertex_distribution(reference_rng);
        while (v == u) {
            v = vertex_distribution(reference_rng);
        }
        sampled_edges.emplace_back(u, v);
    }

    Graph expected_graph(kNumVertices);
    for (const auto &[u, v] : sampled_edges) {
        expected_graph.AddEdge(u, v);
    }
    assert(graph->edges.size() == expected_graph.edges.size());
    for (std::size_t i = 0; i < graph->edges.size(); ++i) {
        assert(graph->edges[i].head == expected_graph.edges[i].head);
        assert(graph->edges[i].tail == expected_graph.edges[i].tail);
    }
    assert(graph->adj_list == expected_graph.adj_list);
    assert(graph->NumVertices() == kNumVertices);
    assert(EdgeCount(*graph) == kNumEdges);
}

void TestRandomGraphGenerationAllowsEmptyGraph() {
    GraphGenerator generator(7);
    const std::unique_ptr<Graph> graph = generator.GenerateRandomGraph(5, 0);

    assert(graph->NumVertices() == 5);
    assert(EdgeCount(*graph) == 0);
}

void TestRandomGraphGenerationRejectsEdgesOnEmptyVertexSet() {
    GraphGenerator generator(7);

    bool threw = false;
    try {
        generator.GenerateRandomGraph(0, 1);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);
}

void TestRandomGraphGenerationRejectsEdgesOnSingleVertex() {
    GraphGenerator generator(7);

    bool threw = false;
    try {
        generator.GenerateRandomGraph(1, 1);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);
}

void TestPinchUpdatesGraphStructure() {
    Graph graph(2);
    graph.AddEdge(0, 1);
    graph.AddEdge(0, 1);
    graph.AddEdge(0, 1);
    graph.AddEdge(0, 1);

    graph.Pinch({1, 3});

    assert(graph.NumVertices() == 3);
    assert(graph.NumEdges() == 6);
    assert(graph.edges[0].head == 0 && graph.edges[0].tail == 1);
    assert(graph.edges[1].head == 0 && graph.edges[1].tail == 2);
    assert(graph.edges[2].head == 0 && graph.edges[2].tail == 1);
    assert(graph.edges[3].head == 0 && graph.edges[3].tail == 2);
    assert(graph.edges[4].head == 2 && graph.edges[4].tail == 1);
    assert(graph.edges[5].head == 2 && graph.edges[5].tail == 1);
    assert(graph.adj_list[0] == std::vector<int>({0, 2, 1, 3}));
    assert(graph.adj_list[1] == std::vector<int>({0, 2, 4, 5}));
    assert(graph.adj_list[2] == std::vector<int>({1, 4, 3, 5}));
    assert(!HasSelfLoop(graph));
}

void TestExportGraphWritesExpectedFormat() {
    Graph graph(4);
    graph.AddEdge(0, 1);
    graph.AddEdge(1, 3);
    graph.AddEdge(1, 3);

    const std::filesystem::path output_path =
        std::filesystem::temp_directory_path() / "graph_export_test.txt";
    graph.ExportGraph(output_path.string());

    std::ifstream input(output_path);
    assert(input.is_open());

    std::ostringstream content;
    content << input.rdbuf();
    assert(content.str() == "4\n0 1\n1 3\n1 3\n");

    std::filesystem::remove(output_path);
}

void TestDecomposeConnectivityOnKnownGraphs() {
    Graph double_edge({{0, 1}, {0, 1}});
    const std::optional<std::vector<bool> > two_trees =
        double_edge.DecomposeConnectivity(1, 1);
    assert(two_trees.has_value());
    assert(IsValidDecomposition(double_edge, *two_trees, 1, 1));

    Graph triple_edge({{0, 1}, {0, 1}, {0, 1}});
    const std::optional<std::vector<bool> > split_21 =
        triple_edge.DecomposeConnectivity(2, 1);
    assert(split_21.has_value());
    assert(IsValidDecomposition(triple_edge, *split_21, 2, 1));

    Graph cycle({{0, 1}, {1, 2}, {2, 3}, {3, 0}});
    assert(!cycle.DecomposeConnectivity(1, 1).has_value());

    Graph k4({{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}});
    const std::optional<std::vector<bool> > k4_decomposition =
        k4.DecomposeConnectivity(1, 1);
    assert(k4_decomposition.has_value());
    assert(IsValidDecomposition(k4, *k4_decomposition, 1, 1));
}


void TestRandomGraphGenerationOnManySeeds() {
    for (std::uint32_t seed = 0; seed < 100; ++seed) {
        GraphGenerator generator(seed);
        const std::unique_ptr<Graph> graph = generator.GenerateRandomGraph(8, 25);

        assert(graph->NumVertices() == 8);
        assert(EdgeCount(*graph) == 25);
        assert(!HasSelfLoop(*graph));
    }
}

void TestConnectivityMatchesBruteForceOnRandomGraphs() {
    std::mt19937 rng(987654321u);
    std::uniform_int_distribution<int> vertex_distribution(2, 6);
    std::uniform_int_distribution<int> edge_distribution(0, 10);

    for (int iteration = 0; iteration < 200; ++iteration) {
        const int num_vertices = vertex_distribution(rng);
        const int num_edges = edge_distribution(rng);

        Graph graph(num_vertices);
        if (num_vertices >= 2) {
            std::uniform_int_distribution<int> endpoint_distribution(0, num_vertices - 1);
            for (int i = 0; i < num_edges; ++i) {
                const int u = endpoint_distribution(rng);
                int v = endpoint_distribution(rng);
                while (v == u) {
                    v = endpoint_distribution(rng);
                }
                graph.AddEdge(u, v);
            }
        }

        assert(graph.Connectivity() == BruteForceConnectivity(graph));
    }
}

void TestRandomPinchingGraphRejectsInvalidArguments() {
    GraphGenerator generator(11);

    bool threw = false;
    try {
        generator.RandomPinchingGraph(1, 4);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        generator.RandomPinchingGraph(5, 3);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);
}

void TestRandomPinchingGraphOnManySeeds() {
    constexpr int kNumVertices = 8;
    constexpr int kConnectivity = 4;
    constexpr int kExpectedEdges =
        kConnectivity + (kNumVertices - 2) * (kConnectivity / 2);

    for (std::uint32_t seed = 0; seed < 100; ++seed) {
        GraphGenerator generator(seed);
        const std::unique_ptr<Graph> graph =
            generator.RandomPinchingGraph(kNumVertices, kConnectivity);

        assert(graph->NumVertices() == kNumVertices);
        assert(graph->NumEdges() == kExpectedEdges);
        assert(!HasSelfLoop(*graph));
        assert(graph->Connectivity() == kConnectivity);
    }
}

void TestDecomposeConnectivityMatchesBruteForceOnRandomGraphs() {
    std::mt19937 rng(20260405u);
    std::uniform_int_distribution<int> vertex_distribution(2, 5);
    std::uniform_int_distribution<int> edge_distribution(0, 8);

    for (int iteration = 0; iteration < 80; ++iteration) {
        const int num_vertices = vertex_distribution(rng);
        const int num_edges = edge_distribution(rng);

        Graph graph(num_vertices);
        std::uniform_int_distribution<int> endpoint_distribution(0, num_vertices - 1);
        for (int edge_index = 0; edge_index < num_edges; ++edge_index) {
            const int u = endpoint_distribution(rng);
            int v = endpoint_distribution(rng);
            while (v == u) {
                v = endpoint_distribution(rng);
            }
            graph.AddEdge(u, v);
        }

        for (int connectivity_first = 0; connectivity_first <= 2;
             ++connectivity_first) {
            for (int connectivity_second = 0; connectivity_second <= 2;
                 ++connectivity_second) {
                const std::optional<std::vector<bool> > expected =
                    BruteForceDecomposition(
                        graph, connectivity_first, connectivity_second);
                const std::optional<std::vector<bool> > actual =
                    graph.DecomposeConnectivity(
                        connectivity_first, connectivity_second);
                assert(expected.has_value() == actual.has_value());
                if (actual.has_value()) {
                    assert(IsValidDecomposition(
                        graph, *actual, connectivity_first, connectivity_second));
                }
            }
        }
    }
}

}  // namespace

int main() {
    TestGraphConstructsFromEdges();
    TestGraphRejectsNegativeEndpoints();
    TestGraphRejectsLoops();
    TestConnectivityOnKnownGraphs();
    TestRandomGraphGenerationMatchesReferenceSampling();
    TestRandomGraphGenerationAllowsEmptyGraph();
    TestRandomGraphGenerationRejectsEdgesOnEmptyVertexSet();
    TestRandomGraphGenerationRejectsEdgesOnSingleVertex();
    TestPinchUpdatesGraphStructure();
    TestExportGraphWritesExpectedFormat();
    TestDecomposeConnectivityOnKnownGraphs();
    TestRandomGraphGenerationOnManySeeds();
    TestConnectivityMatchesBruteForceOnRandomGraphs();
    TestRandomPinchingGraphRejectsInvalidArguments();
    TestRandomPinchingGraphOnManySeeds();
    TestDecomposeConnectivityMatchesBruteForceOnRandomGraphs();
    return 0;
}
