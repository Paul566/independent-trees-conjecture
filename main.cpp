#include "Graph.h"
#include "GraphGenerator.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <random>

int main() {
    constexpr int kConnectivity = 6;
    constexpr int kFirstConnectivity = 2;
    constexpr int kSecondConnectivity = 4;
    constexpr int kThirdConnectivity = 3;
    constexpr int kNumAttempts = 10000;

    const std::filesystem::path output_directory = "graphs/6_non_decomposable";
    std::filesystem::create_directories(output_directory);

    GraphGenerator generator(0);
    std::mt19937 rng(1);
    std::uniform_int_distribution<int> vertex_distribution(4, 20);

    int exported_graphs = 0;
    for (int attempt = 0; attempt < kNumAttempts; ++attempt) {
        if (attempt % 100 == 0) {
            std::cout << "attempt=" << attempt
                      << " exported=" << exported_graphs << std::endl;
        }

        const int num_vertices = vertex_distribution(rng);
        std::unique_ptr<Graph> graph =
            generator.KargerPinchingEvenGraph(num_vertices, kConnectivity);

        const bool is_24_non_decomposable =
            !graph->DecomposeConnectivity(
                kFirstConnectivity, kSecondConnectivity).has_value();
        if (!is_24_non_decomposable) {
            continue;
        }
        const bool is_33_non_decomposable =
            !graph->DecomposeConnectivity(
                kThirdConnectivity, kThirdConnectivity).has_value();
        if (!is_33_non_decomposable) {
            continue;
        }

        const std::string base_name = "graph_" + std::to_string(exported_graphs);
        graph->ExportGraph((output_directory / (base_name + ".txt")).string());

        std::ofstream metadata(output_directory / (base_name + "_metadata.txt"));
        metadata << "attempt=" << attempt << "\n";
        metadata << "num_vertices=" << graph->NumVertices() << "\n";
        metadata << "num_edges=" << graph->NumEdges() << "\n";
        metadata << "connectivity=" << graph->Connectivity() << "\n";
        metadata << "non_decomposable_2_4=" << is_24_non_decomposable << "\n";
        metadata << "non_decomposable_3_3=" << is_33_non_decomposable << "\n";

        ++exported_graphs;
    }

    std::cout << "exported_total=" << exported_graphs << std::endl;
    return 0;
}
