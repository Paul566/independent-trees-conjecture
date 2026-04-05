#include "Graph.h"
#include "GraphGenerator.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

int main() {
    constexpr int kMinVertices = 6;
    constexpr int kMaxVertices = 10;
    constexpr int kConnectivity = 4;
    constexpr int kFirstConnectivity = 2;
    constexpr int kSecondConnectivity = 2;

    const std::filesystem::path output_directory =
        "graphs/non_decomposable_random_pinched";
    std::filesystem::create_directories(output_directory);

    GraphGenerator generator(0);
    int successes = 0;
    for (int i = 0; i < 100; ++i) {
        for (int num_vertices = kMinVertices; num_vertices <= kMaxVertices;
             ++num_vertices) {

            std::unique_ptr<Graph> graph =
                generator.RandomPinchingGraph(num_vertices, kConnectivity);
            const std::optional<std::vector<bool> > decomposition =
                graph->DecomposeConnectivity(
                    kFirstConnectivity, kSecondConnectivity);
            if (!decomposition.has_value()) {
                ++successes;
            }

            // graph->ExportGraph((output_directory / "graph.txt").string());
        }
    }

    std::cout << successes << std::endl;

    return 0;
}
