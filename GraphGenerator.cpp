//
// Created by paul on 4/4/26.
//

#include "GraphGenerator.h"

#include "Graph.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <set>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {

struct KargerCutSample {
    std::vector<int> cut_edges;
    bool is_single_vertex_cut = false;
};

struct DisjointSet {
    explicit DisjointSet(int size) : parent(size), rank(size, 0) {
        std::iota(parent.begin(), parent.end(), 0);
    }

    int Find(int vertex) {
        if (parent[vertex] == vertex) {
            return vertex;
        }
        parent[vertex] = Find(parent[vertex]);
        return parent[vertex];
    }

    bool Unite(int first, int second) {
        first = Find(first);
        second = Find(second);
        if (first == second) {
            return false;
        }
        if (rank[first] < rank[second]) {
            std::swap(first, second);
        }
        parent[second] = first;
        if (rank[first] == rank[second]) {
            ++rank[first];
        }
        return true;
    }

    std::vector<int> parent;
    std::vector<int> rank;
};

std::vector<int> SampleDistinctIndices(
    std::mt19937 *rng, int total_size, int sample_size) {
    std::vector<int> indices(total_size);
    std::iota(indices.begin(), indices.end(), 0);
    std::shuffle(indices.begin(), indices.end(), *rng);
    indices.resize(sample_size);
    return indices;
}

std::vector<int> SampleDistinctIndicesWithRequiredElement(
    std::mt19937 *rng, int total_size, int sample_size,
    const std::vector<bool> &is_required_candidate) {
    std::vector<int> required_candidates;
    std::vector<int> optional_candidates;
    required_candidates.reserve(total_size);
    optional_candidates.reserve(total_size);
    for (int index = 0; index < total_size; ++index) {
        if (is_required_candidate[index]) {
            required_candidates.push_back(index);
        } else {
            optional_candidates.push_back(index);
        }
    }

    if (required_candidates.empty()) {
        throw std::logic_error("No valid required element exists for sampling");
    }
    if (sample_size <= 0 || sample_size > total_size) {
        throw std::invalid_argument("Sample size must be between 1 and total size");
    }

    std::shuffle(required_candidates.begin(), required_candidates.end(), *rng);
    std::shuffle(optional_candidates.begin(), optional_candidates.end(), *rng);

    std::vector<int> sampled_indices;
    sampled_indices.reserve(sample_size);
    sampled_indices.push_back(required_candidates.front());

    std::vector<int> remaining_candidates;
    remaining_candidates.reserve(total_size - 1);
    for (int index = 0; index < total_size; ++index) {
        if (index != sampled_indices.front()) {
            remaining_candidates.push_back(index);
        }
    }
    std::shuffle(remaining_candidates.begin(), remaining_candidates.end(), *rng);
    remaining_candidates.resize(sample_size - 1);
    sampled_indices.insert(
        sampled_indices.end(), remaining_candidates.begin(),
        remaining_candidates.end());
    return sampled_indices;
}

KargerCutSample SampleKargerCut(std::mt19937 *rng, const Graph &graph) {
    if (graph.NumVertices() <= 2) {
        std::vector<int> all_edges(graph.NumEdges());
        std::iota(all_edges.begin(), all_edges.end(), 0);
        return {.cut_edges = std::move(all_edges), .is_single_vertex_cut = true};
    }

    std::vector<int> edge_indices(graph.NumEdges());
    std::iota(edge_indices.begin(), edge_indices.end(), 0);
    std::shuffle(edge_indices.begin(), edge_indices.end(), *rng);

    DisjointSet components(graph.NumVertices());
    int remaining_components = graph.NumVertices();
    for (const int edge_index : edge_indices) {
        const Edge &edge = graph.edges[edge_index];
        if (components.Unite(edge.head, edge.tail)) {
            --remaining_components;
            if (remaining_components == 2) {
                break;
            }
        }
    }

    std::vector<int> component_sizes(graph.NumVertices(), 0);
    for (int vertex = 0; vertex < graph.NumVertices(); ++vertex) {
        ++component_sizes[components.Find(vertex)];
    }

    std::vector<int> cut_edges;
    cut_edges.reserve(graph.NumEdges());
    for (int edge_index = 0; edge_index < graph.NumEdges(); ++edge_index) {
        const Edge &edge = graph.edges[edge_index];
        if (components.Find(edge.head) != components.Find(edge.tail)) {
            cut_edges.push_back(edge_index);
        }
    }

    bool is_single_vertex_cut = false;
    for (const int component_size : component_sizes) {
        if (component_size == 1) {
            is_single_vertex_cut = true;
            break;
        }
    }
    return {
        .cut_edges = std::move(cut_edges),
        .is_single_vertex_cut = is_single_vertex_cut,
    };
}

double FractionOfParallelEdges(const Graph &graph) {
    if (graph.NumEdges() == 0) {
        return 0.0;
    }

    std::map<std::pair<int, int>, int> multiplicities;
    for (const Edge &edge : graph.edges) {
        ++multiplicities[{std::min(edge.head, edge.tail),
                          std::max(edge.head, edge.tail)}];
    }

    int parallel_edges = 0;
    for (const auto &[endpoints, multiplicity] : multiplicities) {
        static_cast<void>(endpoints);
        if (multiplicity > 1) {
            parallel_edges += multiplicity;
        }
    }
    return static_cast<double>(parallel_edges) / graph.NumEdges();
}

std::vector<int> SelectPinchEdgesFromCut(
    std::mt19937 *rng, const Graph &graph, std::vector<int> cut_edges,
    int pinch_size, bool prefer_non_parallel) {
    std::shuffle(cut_edges.begin(), cut_edges.end(), *rng);
    if (!prefer_non_parallel) {
        cut_edges.resize(pinch_size);
        return cut_edges;
    }

    std::vector<int> selected_edges;
    selected_edges.reserve(pinch_size);
    std::set<std::pair<int, int>> used_endpoint_pairs;
    std::vector<bool> is_selected(cut_edges.size(), false);
    for (int i = 0; i < static_cast<int>(cut_edges.size()); ++i) {
        const Edge &edge = graph.edges[cut_edges[i]];
        const std::pair<int, int> endpoints = {
            std::min(edge.head, edge.tail), std::max(edge.head, edge.tail)};
        if (used_endpoint_pairs.contains(endpoints)) {
            continue;
        }
        used_endpoint_pairs.insert(endpoints);
        selected_edges.push_back(cut_edges[i]);
        is_selected[i] = true;
        if (static_cast<int>(selected_edges.size()) == pinch_size) {
            return selected_edges;
        }
    }

    for (int i = 0; i < static_cast<int>(cut_edges.size()); ++i) {
        if (is_selected[i]) {
            continue;
        }
        selected_edges.push_back(cut_edges[i]);
        if (static_cast<int>(selected_edges.size()) == pinch_size) {
            return selected_edges;
        }
    }

    throw std::logic_error("Not enough cut edges available for pinching");
}

}  // namespace

GraphGenerator::GraphGenerator(std::mt19937::result_type seed) : rng_(seed) {
}

std::unique_ptr<Graph> GraphGenerator::GenerateRandomGraph(int n, int m) {
    if (n < 0) {
        throw std::invalid_argument("Number of vertices must be non-negative");
    }
    if (m < 0) {
        throw std::invalid_argument("Number of edges must be non-negative");
    }
    if (n == 0 && m > 0) {
        throw std::invalid_argument("Cannot sample edges from an empty graph");
    }
    if (n == 1 && m > 0) {
        throw std::invalid_argument(
            "Cannot sample loop-free edges from a single-vertex graph");
    }

    auto graph = std::make_unique<Graph>(n);
    if (n <= 1) {
        return graph;
    }

    std::uniform_int_distribution<int> vertex_distribution(0, n - 1);
    for (int i = 0; i < m; ++i) {
        const int u = vertex_distribution(rng_);
        int v = vertex_distribution(rng_);
        while (v == u) {
            v = vertex_distribution(rng_);
        }
        graph->AddEdge(u, v);
    }
    return graph;
}

std::unique_ptr<Graph> GraphGenerator::RandomPinchingOddGraph(
    int n, int connectivity) {
    if (n < 2 || n % 2 != 0) {
        throw std::invalid_argument(
            "Odd pinching construction requires an even number of vertices >= 2");
    }
    if (connectivity <= 1 || connectivity % 2 == 0) {
        throw std::invalid_argument(
            "Connectivity must be an odd integer >= 3");
    }

    const int pinch_size = connectivity / 2;
    auto graph = std::make_unique<Graph>(2);
    for (int i = 0; i < connectivity; ++i) {
        graph->AddEdge(0, 1);
    }

    for (int step = 0; step < (n - 2) / 2; ++step) {
        const std::vector<int> first_edges =
            SampleDistinctIndices(&rng_, graph->NumEdges(), pinch_size);
        graph->Pinch(first_edges);
        const int u = graph->NumVertices() - 1;

        std::vector<bool> not_incident_to_u(graph->NumEdges(), true);
        for (const int edge_index : graph->adj_list[u]) {
            not_incident_to_u[edge_index] = false;
        }
        const std::vector<int> second_edges =
            SampleDistinctIndicesWithRequiredElement(
                &rng_, graph->NumEdges(), pinch_size, not_incident_to_u);
        graph->Pinch(second_edges);
        const int v = graph->NumVertices() - 1;
        graph->AddEdge(u, v);
    }

    return graph;
}

std::unique_ptr<Graph> GraphGenerator::RandomPinchingEvenGraph(
    int n, int connectivity) {
    if (n < 2) {
        throw std::invalid_argument(
            "Pinching construction requires at least 2 vertices");
    }
    if (connectivity <= 0 || connectivity % 2 != 0) {
        throw std::invalid_argument(
            "Connectivity must be a positive even integer");
    }

    auto graph = std::make_unique<Graph>(2);
    for (int i = 0; i < connectivity; ++i) {
        graph->AddEdge(0, 1);
    }

    const int pinch_size = connectivity / 2;
    for (int step = 0; step < n - 2; ++step) {
        std::vector<int> edge_indices(graph->NumEdges());
        std::iota(edge_indices.begin(), edge_indices.end(), 0);
        std::shuffle(edge_indices.begin(), edge_indices.end(), rng_);
        edge_indices.resize(pinch_size);
        graph->Pinch(edge_indices);
    }

    return graph;
}

std::unique_ptr<Graph> GraphGenerator::SingletonPinchingEvenGraph(
    int n, int connectivity) {
    if (n < 2) {
        throw std::invalid_argument(
            "Pinching construction requires at least 2 vertices");
    }
    if (connectivity <= 0 || connectivity % 2 != 0) {
        throw std::invalid_argument(
            "Connectivity must be a positive even integer");
    }

    auto graph = std::make_unique<Graph>(2);
    for (int i = 0; i < connectivity; ++i) {
        graph->AddEdge(0, 1);
    }

    const int pinch_size = connectivity / 2;
    for (int step = 0; step < n - 2; ++step) {
        std::uniform_int_distribution<int> vertex_distribution(
            0, graph->NumVertices() - 1);
        const int pinch_vertex = vertex_distribution(rng_);
        const std::vector<int> sampled_incident_indices =
            SampleDistinctIndices(
                &rng_, static_cast<int>(graph->adj_list[pinch_vertex].size()),
                pinch_size);

        std::vector<int> pinch_edges;
        pinch_edges.reserve(pinch_size);
        for (const int incident_index : sampled_incident_indices) {
            pinch_edges.push_back(graph->adj_list[pinch_vertex][incident_index]);
        }
        graph->Pinch(pinch_edges);
    }

    return graph;
}

std::unique_ptr<Graph> GraphGenerator::KargerPinchingEvenGraph(
    int n, int connectivity, bool verbose, bool prefer_non_parallel,
    bool prefer_non_single_vertex_cut, int connectivity_luft,
    int max_cut_sampling_attempts) {
    if (n < 2) {
        throw std::invalid_argument(
            "Karger pinching construction requires at least 2 vertices");
    }
    if (connectivity <= 0 || connectivity % 2 != 0) {
        throw std::invalid_argument(
            "Connectivity must be a positive even integer");
    }
    if (max_cut_sampling_attempts <= 0) {
        throw std::invalid_argument(
            "Maximum cut sampling attempts must be positive");
    }

    const int pinch_size = connectivity / 2;
    auto graph = std::make_unique<Graph>(2);
    for (int i = 0; i < connectivity; ++i) {
        graph->AddEdge(0, 1);
    }

    int total_cut_size = 0;
    int single_vertex_cut_count = 0;
    for (int step = 0; step < n - 2; ++step) {
        std::optional<KargerCutSample> selected_cut;
        for (int attempt = 0; attempt < max_cut_sampling_attempts; ++attempt) {
            KargerCutSample candidate_cut = SampleKargerCut(&rng_, *graph);
            if (candidate_cut.cut_edges.size() > connectivity + connectivity_luft) {
                continue;
            }
            if (!selected_cut.has_value()) {
                selected_cut = std::move(candidate_cut);
                if (!prefer_non_single_vertex_cut ||
                    !selected_cut->is_single_vertex_cut) {
                    break;
                }
                continue;
            }
            if (prefer_non_single_vertex_cut &&
                !candidate_cut.is_single_vertex_cut) {
                selected_cut = std::move(candidate_cut);
                break;
            }
        }
        if (!selected_cut.has_value()) {
            throw std::logic_error(
                "Karger-style contraction failed to find an acceptable cut");
        }
        std::vector<int> cut_edges = selected_cut->cut_edges;
        total_cut_size += static_cast<int>(cut_edges.size());
        if (selected_cut->is_single_vertex_cut) {
            ++single_vertex_cut_count;
        }
        if (static_cast<int>(cut_edges.size()) < pinch_size) {
            throw std::logic_error(
                "Karger-style contraction produced a cut that is too small");
        }
        graph->Pinch(SelectPinchEdgesFromCut(
            &rng_, *graph, std::move(cut_edges), pinch_size,
            prefer_non_parallel));
    }

    if (verbose) {
        const int num_pinches = n - 2;
        const double average_cut_size =
            num_pinches == 0 ? 0.0
                             : static_cast<double>(total_cut_size) / num_pinches;
        const double single_vertex_cut_probability =
            num_pinches == 0 ? 0.0
                             : static_cast<double>(single_vertex_cut_count) / num_pinches;
        std::cout << "average_cut_size=" << average_cut_size
                  << " single_vertex_cut_probability="
                  << single_vertex_cut_probability
                  << " parallel_edge_fraction="
                  << FractionOfParallelEdges(*graph) << std::endl;
    }

    return graph;
}

std::unique_ptr<Graph> GraphGenerator::AdversarialPinchingEvenGraph(
    int n_max, int a, int b, bool pinch_from_first_factor) {
    if (n_max < 2) {
        throw std::invalid_argument("Adversarial pinching requires n_max >= 2");
    }
    if (a < 0 || b < 0) {
        throw std::invalid_argument("Connectivity parameters must be non-negative");
    }
    if (b < a) {
        throw std::invalid_argument("This generator requires b >= a");
    }
    if ((a + b) <= 0 || (a + b) % 2 != 0) {
        throw std::invalid_argument("a + b must be a positive even integer");
    }

    const int total_connectivity = a + b;
    const int pinch_size = total_connectivity / 2;
    auto graph = std::make_unique<Graph>(2);
    for (int i = 0; i < total_connectivity; ++i) {
        graph->AddEdge(0, 1);
    }

    while (true) {
        const std::optional<std::vector<bool> > decomposition =
            graph->DecomposeConnectivity(a, b);
        if (!decomposition.has_value()) {
            return graph;
        }
        if (graph->NumVertices() >= n_max) {
            return nullptr;
        }

        std::vector<int> chosen_factor_edges;
        chosen_factor_edges.reserve(graph->NumEdges());
        for (int edge_index = 0; edge_index < graph->NumEdges(); ++edge_index) {
            if ((*decomposition)[edge_index] == !pinch_from_first_factor) {
                chosen_factor_edges.push_back(edge_index);
            }
        }

        if (static_cast<int>(chosen_factor_edges.size()) >= pinch_size) {
            std::shuffle(chosen_factor_edges.begin(), chosen_factor_edges.end(), rng_);
            chosen_factor_edges.resize(pinch_size);
            graph->Pinch(chosen_factor_edges);
            continue;
        }

        std::vector<int> random_edges =
            SampleDistinctIndices(&rng_, graph->NumEdges(), pinch_size);
        graph->Pinch(random_edges);
    }
}
