//
// Created by paul on 4/4/26.
//

#include "Graph.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/stoer_wagner_min_cut.hpp>

#include <algorithm>
#include <map>
#include <queue>
#include <set>
#include <stdexcept>
#include <utility>

namespace {

int OtherEndpoint(const Edge &edge, int vertex) {
    if (edge.head == vertex) {
        return edge.tail;
    }
    if (edge.tail == vertex) {
        return edge.head;
    }
    throw std::invalid_argument("Vertex is not incident to the edge");
}

void RemoveIncidentEdge(std::vector<int> *incident_edges, int edge_index) {
    const auto it =
        std::find(incident_edges->begin(), incident_edges->end(), edge_index);
    if (it == incident_edges->end()) {
        throw std::invalid_argument("Adjacency list is missing an incident edge");
    }
    incident_edges->erase(it);
}

bool IsConnected(const Graph &graph) {
    if (graph.NumVertices() <= 1) {
        return true;
    }

    std::vector<bool> visited(graph.NumVertices(), false);
    std::queue<int> to_visit;
    to_visit.push(0);
    visited[0] = true;

    while (!to_visit.empty()) {
        const int vertex = to_visit.front();
        to_visit.pop();
        for (const int edge_index : graph.adj_list[vertex]) {
            const int neighbor = OtherEndpoint(graph.edges[edge_index], vertex);
            if (visited[neighbor]) {
                continue;
            }
            visited[neighbor] = true;
            to_visit.push(neighbor);
        }
    }

    return std::all_of(visited.begin(), visited.end(), [](const bool seen) {
        return seen;
    });
}

}  // namespace

Graph::Graph() = default;

Graph::Graph(int num_vertices) {
    if (num_vertices < 0) {
        throw std::invalid_argument("Number of vertices must be non-negative");
    }
    adj_list.resize(num_vertices);
}

Graph::Graph(const std::vector<std::tuple<int, int> > &edges) {
    int max_vertex = -1;
    for (const auto &[u, v] : edges) {
        if (u < 0 || v < 0) {
            throw std::invalid_argument("Edge endpoints must be non-negative");
        }
        if (u == v) {
            throw std::invalid_argument("Loops are not allowed");
        }
        max_vertex = std::max({max_vertex, u, v});
    }

    adj_list.resize(max_vertex + 1);
    for (const auto &[u, v] : edges) {
        AddEdge(u, v);
    }
}

int Graph::Connectivity() const {
    if (NumVertices() <= 1) {
        return 0;
    }
    if (!IsConnected(*this)) {
        return 0;
    }

    using BoostGraph = boost::adjacency_list<
        boost::vecS, boost::vecS, boost::undirectedS, boost::no_property,
        boost::property<boost::edge_weight_t, int> >;

    std::map<std::pair<int, int>, int> multiplicities;
    for (const Edge &edge : edges) {
        ++multiplicities[{std::min(edge.head, edge.tail),
                          std::max(edge.head, edge.tail)}];
    }

    BoostGraph graph(NumVertices());
    for (const auto &[endpoints, multiplicity] : multiplicities) {
        boost::add_edge(endpoints.first, endpoints.second, multiplicity, graph);
    }

    return boost::stoer_wagner_min_cut(graph, boost::get(boost::edge_weight, graph));
}

int Graph::NumEdges() const {
    return static_cast<int>(edges.size());
}

int Graph::NumVertices() const {
    return static_cast<int>(adj_list.size());
}

void Graph::AddEdge(int u, int v) {
    if (u < 0 || v < 0) {
        throw std::invalid_argument("Edge endpoints must be non-negative");
    }
    if (u == v) {
        throw std::invalid_argument("Loops are not allowed");
    }
    if (u >= NumVertices() || v >= NumVertices()) {
        throw std::out_of_range("Edge endpoint is outside the graph");
    }

    const int edge_index = NumEdges();
    edges.push_back({u, v});
    adj_list[u].push_back(edge_index);
    adj_list[v].push_back(edge_index);
}

void Graph::Pinch(const std::vector<int> &edge_indices) {
    if (edge_indices.empty()) {
        throw std::invalid_argument("Must pinch at least one edge");
    }

    const std::set<int> unique_indices(edge_indices.begin(), edge_indices.end());
    if (unique_indices.size() != edge_indices.size()) {
        throw std::invalid_argument("Pinch edge indices must be distinct");
    }
    for (const int edge_index : edge_indices) {
        if (edge_index < 0 || edge_index >= NumEdges()) {
            throw std::out_of_range("Pinch edge index is outside the graph");
        }
    }

    const int new_vertex = NumVertices();
    adj_list.emplace_back();

    for (const int edge_index : edge_indices) {
        const Edge old_edge = edges[edge_index];
        RemoveIncidentEdge(&adj_list[old_edge.head], edge_index);
        RemoveIncidentEdge(&adj_list[old_edge.tail], edge_index);

        edges[edge_index] = {old_edge.head, new_vertex};
        adj_list[old_edge.head].push_back(edge_index);
        adj_list[new_vertex].push_back(edge_index);

        const int new_edge_index = NumEdges();
        edges.push_back({new_vertex, old_edge.tail});
        adj_list[new_vertex].push_back(new_edge_index);
        adj_list[old_edge.tail].push_back(new_edge_index);
    }
}
