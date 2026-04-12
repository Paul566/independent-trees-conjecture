//
// Created by paul on 4/4/26.
//

#ifndef INDEPENDENT_TREES_CONJECTURE_GRAPH_H
#define INDEPENDENT_TREES_CONJECTURE_GRAPH_H

#include <filesystem>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

struct Edge {
    int head;
    int tail;

    bool operator==(const Edge &) const = default;
};

using RootedSpanningTree = std::vector<int>;

class Graph {
    public:
        Graph();
        explicit Graph(int num_vertices);
        explicit Graph(const std::vector<std::tuple<int, int> > &edges);
        explicit Graph(const std::filesystem::path &path);

        [[nodiscard]] int Connectivity() const;
        [[nodiscard]] std::optional<std::vector<bool> > DecomposeConnectivity(
            int connectivity_first, int connectivity_second) const;
        [[nodiscard]] std::optional<std::vector<RootedSpanningTree> > EdgeIndependentTrees(
            int k, int r) const;
        [[nodiscard]] std::optional<std::vector<int> > NowhereZeroKFlow(int k) const;
        [[nodiscard]] int NumEdges() const;
        [[nodiscard]] int NumVertices() const;
        void AddEdge(int u, int v);
        void ExportGraph(const std::string &path) const;
        void Pinch(const std::vector<int> &edge_indices);

        std::vector<Edge> edges;
        std::vector<std::vector<int> > adj_list;
};

#endif //INDEPENDENT_TREES_CONJECTURE_GRAPH_H
