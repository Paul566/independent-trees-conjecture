#include "Graph.h"
#include "GraphGenerator.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>

int main() {
    constexpr int kMaxVertices = 50;
    constexpr int kFirstConnectivity = 2;
    constexpr int kSecondConnectivity = 4;
    constexpr int kNumAttempts = 10000;

    const std::filesystem::path output_directory =
        "graphs/non_decomposable_adversarial_even_pinched";
    std::filesystem::create_directories(output_directory);

    GraphGenerator generator(4);
    for (int attempt = 0; attempt < kNumAttempts; ++attempt) {
        if (attempt % 100 == 0) {
            std::cout << "attempt=" << attempt << std::endl;
        }
        std::unique_ptr<Graph> graph = generator.AdversarialPinchingEvenGraph(
            kMaxVertices, kFirstConnectivity, kSecondConnectivity, true);
        if (!graph) {
            continue;
        }

        std::cout << "Found graph on attempt " << attempt
                  << " with " << graph->NumVertices() << " vertices and "
                  << graph->NumEdges() << " edges.\n";
        graph->ExportGraph((output_directory / "graph.txt").string());

        std::ofstream metadata(output_directory / "metadata.txt");
        metadata << "attempt=" << attempt << "\n";
        metadata << "num_vertices=" << graph->NumVertices() << "\n";
        metadata << "num_edges=" << graph->NumEdges() << "\n";
        metadata << "connectivity=" << graph->Connectivity() << "\n";
        metadata << "decomposition_exists="
                 << graph->DecomposeConnectivity(
                        kFirstConnectivity, kSecondConnectivity).has_value()
                 << "\n";
        return 0;
    }

    std::cout << "No graph found in " << kNumAttempts << " attempts.\n";
    return 0;
}
