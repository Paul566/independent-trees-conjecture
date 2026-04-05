//
// Created by paul on 4/4/26.
//

#include "Graph.h"

#include <boost/graph/adjacency_list.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/graph/stoer_wagner_min_cut.hpp>
#include <ortools/sat/cp_model.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

namespace {

using CpModelBuilder = operations_research::sat::CpModelBuilder;
using BoolVar = operations_research::sat::BoolVar;
using CpSolverResponse = operations_research::sat::CpSolverResponse;
using CpSolverStatus = operations_research::sat::CpSolverStatus;
using LinearExpr = operations_research::sat::LinearExpr;
using SatParameters = operations_research::sat::SatParameters;
using operations_research::sat::SolutionBooleanValue;
using operations_research::sat::SolveWithParameters;

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

struct CutResult {
    int cut_value;
    std::vector<int> crossing_edges;
};

CutResult FindMinimumWeightedCut(
    const Graph &graph, const std::vector<int> &edge_weights) {
    using BoostGraph = boost::adjacency_list<
        boost::vecS, boost::vecS, boost::undirectedS, boost::no_property,
        boost::property<boost::edge_weight_t, int> >;

    if (graph.NumVertices() <= 1) {
        return {0, {}};
    }

    BoostGraph weighted_graph(graph.NumVertices());
    for (int edge_index = 0; edge_index < graph.NumEdges(); ++edge_index) {
        const Edge &edge = graph.edges[edge_index];
        boost::add_edge(edge.head, edge.tail, edge_weights[edge_index], weighted_graph);
    }

    std::vector<int> partition(graph.NumVertices(), 0);
    const auto parity_map = boost::make_iterator_property_map(
        partition.begin(), get(boost::vertex_index, weighted_graph));
    const int cut_value = boost::stoer_wagner_min_cut(
        weighted_graph, get(boost::edge_weight, weighted_graph),
        boost::parity_map(parity_map));

    std::vector<int> crossing_edges;
    for (int edge_index = 0; edge_index < graph.NumEdges(); ++edge_index) {
        const Edge &edge = graph.edges[edge_index];
        if (partition[edge.head] != partition[edge.tail]) {
            crossing_edges.push_back(edge_index);
        }
    }

    return {cut_value, crossing_edges};
}

std::optional<std::vector<int> > FindViolatingCut(
    const Graph &graph, const std::vector<bool> &edge_partition,
    bool use_one_edges, int required_connectivity) {
    if (required_connectivity <= 0 || graph.NumVertices() <= 1) {
        return std::nullopt;
    }

    std::vector<int> edge_weights(graph.NumEdges(), 0);
    for (int edge_index = 0; edge_index < graph.NumEdges(); ++edge_index) {
        if (edge_partition[edge_index] == use_one_edges) {
            edge_weights[edge_index] = 1;
        }
    }

    const CutResult cut = FindMinimumWeightedCut(graph, edge_weights);
    if (cut.cut_value >= required_connectivity) {
        return std::nullopt;
    }
    return cut.crossing_edges;
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

std::optional<std::vector<bool> > Graph::DecomposeConnectivity(
    int connectivity_first, int connectivity_second) const {
    if (connectivity_first < 0 || connectivity_second < 0) {
        throw std::invalid_argument("Connectivity requirements must be non-negative");
    }
    if (NumVertices() <= 1) {
        if (connectivity_first == 0 && connectivity_second == 0) {
            return std::vector<bool>(NumEdges(), false);
        }
        return std::nullopt;
    }

    CpModelBuilder model;
    std::vector<BoolVar> edge_in_second;
    edge_in_second.reserve(NumEdges());
    for (int edge_index = 0; edge_index < NumEdges(); ++edge_index) {
        edge_in_second.push_back(model.NewBoolVar());
    }

    for (int vertex = 0; vertex < NumVertices(); ++vertex) {
        std::vector<BoolVar> incident_variables;
        incident_variables.reserve(adj_list[vertex].size());
        for (const int edge_index : adj_list[vertex]) {
            incident_variables.push_back(edge_in_second[edge_index]);
        }

        if (connectivity_first > 0) {
            model.AddLessOrEqual(
                LinearExpr::Sum(incident_variables),
                static_cast<int64_t>(adj_list[vertex].size()) - connectivity_first);
        }
        if (connectivity_second > 0) {
            model.AddGreaterOrEqual(
                LinearExpr::Sum(incident_variables), connectivity_second);
        }
    }

    while (true) {
        SatParameters parameters;
        parameters.set_num_search_workers(1);
        const CpSolverResponse response =
            SolveWithParameters(model.Build(), parameters);
        if (response.status() != CpSolverStatus::FEASIBLE &&
            response.status() != CpSolverStatus::OPTIMAL) {
            return std::nullopt;
        }

        std::vector<bool> edge_partition(NumEdges(), false);
        for (int edge_index = 0; edge_index < NumEdges(); ++edge_index) {
            edge_partition[edge_index] =
                SolutionBooleanValue(response, edge_in_second[edge_index]);
        }

        const std::optional<std::vector<int> > first_cut = FindViolatingCut(
            *this, edge_partition, false, connectivity_first);
        if (first_cut.has_value()) {
            std::vector<BoolVar> cut_variables;
            cut_variables.reserve(first_cut->size());
            for (const int edge_index : *first_cut) {
                cut_variables.push_back(edge_in_second[edge_index]);
            }
            model.AddLessOrEqual(
                LinearExpr::Sum(cut_variables),
                static_cast<int64_t>(first_cut->size()) - connectivity_first);
            continue;
        }

        const std::optional<std::vector<int> > second_cut = FindViolatingCut(
            *this, edge_partition, true, connectivity_second);
        if (second_cut.has_value()) {
            std::vector<BoolVar> cut_variables;
            cut_variables.reserve(second_cut->size());
            for (const int edge_index : *second_cut) {
                cut_variables.push_back(edge_in_second[edge_index]);
            }
            model.AddGreaterOrEqual(
                LinearExpr::Sum(cut_variables), connectivity_second);
            continue;
        }

        return edge_partition;
    }
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

void Graph::ExportGraph(const std::string &path) const {
    std::ofstream output(path);
    if (!output.is_open()) {
        throw std::runtime_error("Failed to open graph export file");
    }

    output << NumVertices() << "\n";
    for (const Edge &edge : edges) {
        output << edge.head << " " << edge.tail << "\n";
    }
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
