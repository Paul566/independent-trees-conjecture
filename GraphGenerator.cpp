//
// Created by paul on 4/4/26.
//

#include "GraphGenerator.h"

#include "Graph.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace {

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

std::unique_ptr<Graph> GraphGenerator::RandomPinchingGraph(
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
