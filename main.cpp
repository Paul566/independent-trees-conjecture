#include "Graph.h"
#include "GraphGenerator.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

int main() {
    constexpr int kMinVertices = 4;
    constexpr int kMaxVertices = 16;
    constexpr int kConnectivity = 5;
    // constexpr int kFirstConnectivity = 2;
    // constexpr int kSecondConnectivity = 2;

    const std::filesystem::path output_directory =
        "graphs/non_decomposable_random_pinched";
    std::filesystem::create_directories(output_directory);

    GraphGenerator generator(1);
    for (int i = 0; i < 10000; ++i) {
        std::unique_ptr<Graph> graph =
            generator.RandomPinchingOddGraph(12, 7);

        std::optional<std::vector<bool> > decomposition = graph->DecomposeConnectivity(2, 5);
        if (!decomposition.has_value()) {
            std::cout << " " << i << " no 25 " << std::endl;
        }

        decomposition = graph->DecomposeConnectivity(3, 4);
        if (!decomposition.has_value()) {
            std::cout << " " << i << " no 34 " << std::endl;
        }

        // graph->ExportGraph((output_directory / "graph.txt").string());

        if (i % 100 == 0) {
            std::cout << i << std::endl;
        }
    }

    return 0;
}
