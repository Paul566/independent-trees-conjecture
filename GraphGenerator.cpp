//
// Created by paul on 4/4/26.
//

#include "GraphGenerator.h"

#include "Graph.h"

#include <algorithm>
#include <memory>
#include <numeric>
#include <stdexcept>
#include <tuple>
#include <vector>

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
