#include "Graph.h"
#include "GraphGenerator.h"

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {

int OtherEndpointInTest(const Edge &edge, int vertex) {
    if (edge.head == vertex) {
        return edge.tail;
    }
    if (edge.tail == vertex) {
        return edge.head;
    }
    throw std::invalid_argument("Vertex is not incident to the edge");
}

int EdgeCount(const Graph &graph) {
    return graph.NumEdges();
}

int Degree(const Graph &graph, int vertex) {
    return static_cast<int>(graph.adj_list[vertex].size());
}

bool HasSelfLoop(const Graph &graph) {
    for (const Edge &edge : graph.edges) {
        if (edge.head == edge.tail) {
            return true;
        }
    }
    return false;
}

int ParentVertex(
    const Graph &graph, const RootedSpanningTree &tree, int vertex, int root) {
    if (vertex == root || tree[vertex] < 0 || tree[vertex] >= graph.NumEdges()) {
        throw std::invalid_argument("Tree has an invalid parent edge");
    }
    return OtherEndpointInTest(graph.edges[tree[vertex]], vertex);
}

std::vector<int> PathEdgesToRoot(
    const Graph &graph, const RootedSpanningTree &tree, int root, int vertex) {
    std::vector<int> path_edges;
    std::vector<bool> visited(graph.NumVertices(), false);
    int current = vertex;
    while (current != root) {
        if (visited[current]) {
            throw std::invalid_argument("Tree path contains a cycle");
        }
        visited[current] = true;
        const int parent_edge = tree[current];
        if (parent_edge < 0 || parent_edge >= graph.NumEdges()) {
            throw std::invalid_argument("Tree path is missing a parent edge");
        }
        path_edges.push_back(parent_edge);
        current = ParentVertex(graph, tree, current, root);
    }
    return path_edges;
}

bool IsValidEdgeIndependentTree(
    const Graph &graph, const RootedSpanningTree &tree, int root) {
    if (static_cast<int>(tree.size()) != graph.NumVertices()) {
        return false;
    }
    if (tree[root] != -1) {
        return false;
    }

    std::set<int> used_edges;
    for (int vertex = 0; vertex < graph.NumVertices(); ++vertex) {
        if (vertex == root) {
            continue;
        }
        if (tree[vertex] < 0 || tree[vertex] >= graph.NumEdges()) {
            return false;
        }
        const Edge &edge = graph.edges[tree[vertex]];
        if (edge.head != vertex && edge.tail != vertex) {
            return false;
        }
        used_edges.insert(tree[vertex]);
        try {
            static_cast<void>(PathEdgesToRoot(graph, tree, root, vertex));
        } catch (const std::invalid_argument &) {
            return false;
        }
    }
    return static_cast<int>(used_edges.size()) == graph.NumVertices() - 1;
}

bool AreValidEdgeIndependentTrees(
    const Graph &graph, const std::vector<RootedSpanningTree> &trees, int root) {
    for (const RootedSpanningTree &tree : trees) {
        if (!IsValidEdgeIndependentTree(graph, tree, root)) {
            return false;
        }
    }

    for (int vertex = 0; vertex < graph.NumVertices(); ++vertex) {
        if (vertex == root) {
            continue;
        }
        std::set<int> used_path_edges;
        for (const RootedSpanningTree &tree : trees) {
            const std::vector<int> path_edges =
                PathEdgesToRoot(graph, tree, root, vertex);
            for (const int edge_index : path_edges) {
                if (!used_path_edges.insert(edge_index).second) {
                    return false;
                }
            }
        }
    }
    return true;
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

std::vector<RootedSpanningTree> EnumerateRootedSpanningTrees(
    const Graph &graph, int root) {
    if (graph.NumEdges() >= static_cast<int>(sizeof(std::uint64_t) * 8)) {
        throw std::invalid_argument("Brute-force tree enumeration is only for small graphs");
    }

    std::vector<RootedSpanningTree> rooted_trees;
    const std::uint64_t mask_limit = std::uint64_t{1} << graph.NumEdges();
    for (std::uint64_t mask = 0; mask < mask_limit; ++mask) {
        if (std::popcount(mask) != graph.NumVertices() - 1) {
            continue;
        }

        RootedSpanningTree tree(graph.NumVertices(), -1);
        std::vector<bool> visited(graph.NumVertices(), false);
        std::queue<int> to_visit;
        to_visit.push(root);
        visited[root] = true;
        while (!to_visit.empty()) {
            const int vertex = to_visit.front();
            to_visit.pop();
            for (int edge_index = 0; edge_index < graph.NumEdges(); ++edge_index) {
                if (((mask >> edge_index) & std::uint64_t{1}) == 0) {
                    continue;
                }
                const Edge &edge = graph.edges[edge_index];
                int neighbor = -1;
                if (edge.head == vertex) {
                    neighbor = edge.tail;
                } else if (edge.tail == vertex) {
                    neighbor = edge.head;
                } else {
                    continue;
                }

                if (visited[neighbor]) {
                    continue;
                }
                visited[neighbor] = true;
                tree[neighbor] = edge_index;
                to_visit.push(neighbor);
            }
        }
        if (std::all_of(visited.begin(), visited.end(), [](bool value) { return value; })) {
            rooted_trees.push_back(tree);
        }
    }
    return rooted_trees;
}

bool BruteForceEdgeIndependentTreesSearch(
    const Graph &graph, const std::vector<RootedSpanningTree> &candidate_trees,
    int root, int k, std::vector<RootedSpanningTree> *chosen_trees) {
    if (static_cast<int>(chosen_trees->size()) == k) {
        return AreValidEdgeIndependentTrees(graph, *chosen_trees, root);
    }

    for (const RootedSpanningTree &tree : candidate_trees) {
        chosen_trees->push_back(tree);
        if (AreValidEdgeIndependentTrees(graph, *chosen_trees, root) &&
            BruteForceEdgeIndependentTreesSearch(
                graph, candidate_trees, root, k, chosen_trees)) {
            return true;
        }
        chosen_trees->pop_back();
    }
    return false;
}

std::optional<std::vector<RootedSpanningTree> > BruteForceEdgeIndependentTrees(
    const Graph &graph, int k, int root) {
    if (k < 0) {
        throw std::invalid_argument("Number of trees must be non-negative");
    }
    if (k == 0) {
        return std::vector<RootedSpanningTree>();
    }
    if (graph.NumVertices() == 1) {
        return std::vector<RootedSpanningTree>(k, RootedSpanningTree(1, -1));
    }

    const std::vector<RootedSpanningTree> candidate_trees =
        EnumerateRootedSpanningTrees(graph, root);
    std::vector<RootedSpanningTree> chosen_trees;
    if (BruteForceEdgeIndependentTreesSearch(
            graph, candidate_trees, root, k, &chosen_trees)) {
        return chosen_trees;
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
        Graph graph(std::vector<std::tuple<int, int> >{{-1, 0}});
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);
}

void TestGraphRejectsLoops() {
    bool threw = false;
    try {
        Graph graph(std::vector<std::tuple<int, int> >{{0, 0}});
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

void TestGraphConstructsFromExportedFile() {
    Graph original_graph(5);
    original_graph.AddEdge(0, 1);
    original_graph.AddEdge(1, 3);
    original_graph.AddEdge(1, 3);
    original_graph.AddEdge(2, 4);

    const std::filesystem::path output_path =
        std::filesystem::temp_directory_path() / "graph_round_trip_test.txt";
    original_graph.ExportGraph(output_path.string());

    const Graph imported_graph(output_path.string());
    assert(imported_graph.NumVertices() == original_graph.NumVertices());
    assert(imported_graph.NumEdges() == original_graph.NumEdges());
    assert(imported_graph.edges == original_graph.edges);
    assert(imported_graph.adj_list == original_graph.adj_list);

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

void TestEdgeIndependentTreesOnKnownGraphs() {
    Graph double_edge({{0, 1}, {0, 1}});
    const std::optional<std::vector<RootedSpanningTree> > double_edge_trees =
        double_edge.EdgeIndependentTrees(2, 0);
    assert(double_edge_trees.has_value());
    assert(AreValidEdgeIndependentTrees(double_edge, *double_edge_trees, 0));

    Graph path({{0, 1}, {1, 2}});
    assert(!path.EdgeIndependentTrees(2, 0).has_value());

    Graph k4({{0, 1}, {0, 2}, {0, 3}, {1, 2}, {1, 3}, {2, 3}});
    const std::optional<std::vector<RootedSpanningTree> > k4_trees =
        k4.EdgeIndependentTrees(2, 0);
    assert(k4_trees.has_value());
    assert(AreValidEdgeIndependentTrees(k4, *k4_trees, 0));
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
        generator.RandomPinchingEvenGraph(1, 4);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        generator.RandomPinchingEvenGraph(5, 3);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);
}

void TestKargerPinchingEvenGraphRejectsInvalidArguments() {
    GraphGenerator generator(23);

    bool threw = false;
    try {
        generator.KargerPinchingEvenGraph(1, 4);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        generator.KargerPinchingEvenGraph(5, 3);
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
            generator.RandomPinchingEvenGraph(kNumVertices, kConnectivity);

        assert(graph->NumVertices() == kNumVertices);
        assert(graph->NumEdges() == kExpectedEdges);
        assert(!HasSelfLoop(*graph));
        assert(graph->Connectivity() == kConnectivity);
    }
}

void TestKargerPinchingEvenGraphOnManySeeds() {
    constexpr int kNumVertices = 8;
    constexpr int kConnectivity = 4;
    constexpr int kExpectedEdges =
        kConnectivity + (kNumVertices - 2) * (kConnectivity / 2);

    for (std::uint32_t seed = 0; seed < 100; ++seed) {
        GraphGenerator generator(seed);
        const std::unique_ptr<Graph> graph =
            generator.KargerPinchingEvenGraph(kNumVertices, kConnectivity);

        assert(graph->NumVertices() == kNumVertices);
        assert(graph->NumEdges() == kExpectedEdges);
        assert(!HasSelfLoop(*graph));
        for (int vertex = 0; vertex < graph->NumVertices(); ++vertex) {
            assert(Degree(*graph, vertex) == kConnectivity);
        }
        assert(graph->Connectivity() == kConnectivity);
    }
}

void TestRandomPinchingOddGraphRejectsInvalidArguments() {
    GraphGenerator generator(13);

    bool threw = false;
    try {
        generator.RandomPinchingOddGraph(5, 3);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        generator.RandomPinchingOddGraph(6, 4);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        generator.RandomPinchingOddGraph(6, 1);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);
}

void TestRandomPinchingOddGraphOnManySeeds() {
    constexpr int kNumVertices = 8;
    constexpr int kConnectivity = 5;
    constexpr int kExpectedEdges = kNumVertices * kConnectivity / 2;

    for (std::uint32_t seed = 0; seed < 100; ++seed) {
        GraphGenerator generator(seed);
        const std::unique_ptr<Graph> graph =
            generator.RandomPinchingOddGraph(kNumVertices, kConnectivity);

        assert(graph->NumVertices() == kNumVertices);
        assert(graph->NumEdges() == kExpectedEdges);
        assert(!HasSelfLoop(*graph));
        for (int vertex = 0; vertex < graph->NumVertices(); ++vertex) {
            assert(Degree(*graph, vertex) == kConnectivity);
        }
        assert(graph->Connectivity() == kConnectivity);
    }
}

void TestAdversarialPinchingEvenGraphRejectsInvalidArguments() {
    GraphGenerator generator(17);

    bool threw = false;
    try {
        generator.AdversarialPinchingEvenGraph(1, 2, 4, false);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        generator.AdversarialPinchingEvenGraph(8, 4, 2, false);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);

    threw = false;
    try {
        generator.AdversarialPinchingEvenGraph(8, 2, 3, false);
    } catch (const std::invalid_argument &) {
        threw = true;
    }
    assert(threw);
}

void TestAdversarialPinchingEvenGraphReturnsNullAtMaxSize() {
    GraphGenerator generator(19);
    const std::unique_ptr<Graph> graph =
        generator.AdversarialPinchingEvenGraph(2, 2, 4, true);
    assert(graph == nullptr);
}

void TestAdversarialPinchingEvenGraphContractOnManySeeds() {
    for (std::uint32_t seed = 0; seed < 50; ++seed) {
        for (const bool pinch_from_first_factor : {false, true}) {
            GraphGenerator generator(seed);
            const std::unique_ptr<Graph> graph =
                generator.AdversarialPinchingEvenGraph(
                    8, 2, 4, pinch_from_first_factor);

            if (!graph) {
                continue;
            }
            assert(graph->NumVertices() <= 8);
            assert(!HasSelfLoop(*graph));
            assert(graph->Connectivity() == 6);
            assert(!graph->DecomposeConnectivity(2, 4).has_value());
        }
    }
}

void TestEdgeIndependentTreesMatchesBruteForceOnRandomGraphs() {
    std::mt19937 rng(20260406u);
    std::uniform_int_distribution<int> vertex_distribution(2, 5);
    std::uniform_int_distribution<int> edge_distribution(0, 7);

    for (int iteration = 0; iteration < 60; ++iteration) {
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

        for (int root = 0; root < num_vertices; ++root) {
            for (int k = 0; k <= 2; ++k) {
                const std::optional<std::vector<RootedSpanningTree> > expected =
                    BruteForceEdgeIndependentTrees(graph, k, root);
                const std::optional<std::vector<RootedSpanningTree> > actual =
                    graph.EdgeIndependentTrees(k, root);
                assert(expected.has_value() == actual.has_value());
                if (actual.has_value()) {
                    assert(AreValidEdgeIndependentTrees(graph, *actual, root));
                }
            }
        }
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
    TestGraphConstructsFromExportedFile();
    TestDecomposeConnectivityOnKnownGraphs();
    TestEdgeIndependentTreesOnKnownGraphs();
    TestRandomGraphGenerationOnManySeeds();
    TestConnectivityMatchesBruteForceOnRandomGraphs();
    TestRandomPinchingGraphRejectsInvalidArguments();
    TestRandomPinchingGraphOnManySeeds();
    TestKargerPinchingEvenGraphRejectsInvalidArguments();
    TestKargerPinchingEvenGraphOnManySeeds();
    TestRandomPinchingOddGraphRejectsInvalidArguments();
    TestRandomPinchingOddGraphOnManySeeds();
    TestAdversarialPinchingEvenGraphRejectsInvalidArguments();
    TestAdversarialPinchingEvenGraphReturnsNullAtMaxSize();
    TestAdversarialPinchingEvenGraphContractOnManySeeds();
    TestEdgeIndependentTreesMatchesBruteForceOnRandomGraphs();
    TestDecomposeConnectivityMatchesBruteForceOnRandomGraphs();
    return 0;
}
