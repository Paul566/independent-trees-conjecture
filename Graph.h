//
// Created by paul on 4/4/26.
//

#ifndef INDEPENDENT_TREES_CONJECTURE_GRAPH_H
#define INDEPENDENT_TREES_CONJECTURE_GRAPH_H

#include <tuple>
#include <vector>

struct Edge {
    int head;
    int tail;
};

class Graph {
    public:
        Graph();
        explicit Graph(int num_vertices);
        explicit Graph(const std::vector<std::tuple<int, int> > &edges);

        [[nodiscard]] int Connectivity() const;
        [[nodiscard]] int NumEdges() const;
        [[nodiscard]] int NumVertices() const;
        void AddEdge(int u, int v);
        void Pinch(const std::vector<int> &edge_indices);

        std::vector<Edge> edges;
        std::vector<std::vector<int> > adj_list;
};

#endif //INDEPENDENT_TREES_CONJECTURE_GRAPH_H
